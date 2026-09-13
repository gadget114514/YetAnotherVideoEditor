#include "EditController.h"

#include "../../core/Clip.h"
#include "../../core/Project.h"
#include "../../core/Timeline.h"
#include "../../core/Track.h"
#include "../../core/VideoClip.h"
#include "../../core/AudioClip.h"
#include "../../subtitle/SubtitleClip.h"
#include "../../core/AssetLibrary.h"
#include "../../core/commands/AddClipCommand.h"
#include "../../core/commands/AddTrackCommand.h"
#include "../../core/commands/SplitClipCommand.h"
#include "../../core/commands/FilterCommands.h"
#include "../../core/commands/TransitionCommands.h"
#include "../../core/commands/SetPropertyCommands.h"
#include "../../core/Transition.h"
#include "../../core/VideoFilter.h"
#include "../../subtitle/TitleClip.h"
#include "../../subtitle/commands/AddSubtitleEffectCommand.h"
#include "../../subtitle/commands/EditSubtitleTextCommand.h"
#include "../../subtitle/commands/SetSubtitleStyleCommand.h"
#include "../../subtitle/io/SrtParser.h"
#include "../../core/commands/ImportSubtitleCommand.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

namespace yave {

EditController::EditController(QObject* parent) : QObject(parent) {}

void EditController::setProject(Project* project)
{
    project_ = project;
    if (project_) {
        connect(project_->undoStack(), &QUndoStack::indexChanged, this,
                [this](int) { emit editChanged(); });
    }
}

// ===========================================================================
//  ライブラリからのドロップ (1.7.5)
// ===========================================================================

namespace {

struct DropPayload
{
    QString category;
    QString itemId;
    QUuid   assetId;
    QString name;
    qint64  duration = 0;
    bool    valid = false;
};

DropPayload parsePayload(const QString& json)
{
    DropPayload p;
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    if (o.isEmpty())
        return p;
    p.category = o.value(QStringLiteral("category")).toString();
    p.itemId   = o.value(QStringLiteral("itemId")).toString();
    p.assetId  = QUuid(o.value(QStringLiteral("assetId")).toString());
    p.name     = o.value(QStringLiteral("name")).toString();
    p.duration = qint64(o.value(QStringLiteral("duration")).toDouble());
    p.valid    = !p.category.isEmpty() && !p.itemId.isEmpty();
    return p;
}

/// 尺の分からないものを置くときの既定 (3 秒相当)。
constexpr qint64 kDefaultClipFrames  = 180;
constexpr qint64 kDefaultTransFrames = 30;

} // anonymous namespace

bool EditController::canDropOnClip(const QString& category) const
{
    // クリップの上に落として意味があるのは、そのクリップへ足すものだけ
    return category == QLatin1String("filter") || category == QLatin1String("effect");
}

bool EditController::canDropOnTrack(const QString& category, const QUuid& trackId) const
{
    if (!project_)
        return false;
    const Track* t = project_->timeline()->trackById(trackId);
    if (!t)
        return false;

    if (category == QLatin1String("media"))
        return t->type() == TrackType::Video || t->type() == TrackType::Audio
            || t->type() == TrackType::AiGenerated;
    if (category == QLatin1String("title"))
        return t->type() == TrackType::Video || t->type() == TrackType::Subtitle;
    if (category == QLatin1String("subtitle"))
        return t->type() == TrackType::Subtitle;
    if (category == QLatin1String("transition"))
        return t->type() == TrackType::Video || t->type() == TrackType::AiGenerated;
    return false;
}

QString EditController::assetKind(const QString& assetId) const
{
    if (!project_)
        return {};
    const Asset* a = project_->assets()->asset(QUuid(assetId));
    if (!a)
        return {};
    switch (a->kind) {
    case Asset::Kind::Video:     return QStringLiteral("video");
    case Asset::Kind::Audio:     return QStringLiteral("audio");
    case Asset::Kind::Image:     return QStringLiteral("image");
    case Asset::Kind::Generated: return QStringLiteral("generated");
    }
    return {};
}

bool EditController::canDropAssetOnTrack(const QString& assetKindStr, const QUuid& trackId) const
{
    if (!project_)
        return false;
    const Track* t = project_->timeline()->trackById(trackId);
    if (!t)
        return false;

    if (assetKindStr == QLatin1String("audio"))
        return t->type() == TrackType::Audio || t->type() == TrackType::AiGenerated;
    if (assetKindStr == QLatin1String("image"))
        return t->type() == TrackType::Video || t->type() == TrackType::AiGenerated;
    // video / generated / 不明は映像トラックへ
    return t->type() == TrackType::Video || t->type() == TrackType::AiGenerated;
}

bool EditController::canDropClipOnTrack(const QString& clipId, const QUuid& trackId) const
{
    if (!project_)
        return false;
    Timeline* tl = project_->timeline();
    auto clip = tl ? tl->findClip(QUuid(clipId)) : nullptr;
    if (!clip)
        return false;
    const Track* t = tl->trackById(trackId);
    if (!t)
        return false;
    return t->acceptsClip(*clip);
}

qint64 EditController::clipBoundaryNear(const QUuid& trackId, qint64 frame,
                                        qint64 toleranceFrames) const
{
    if (!project_)
        return -1;
    const Track* t = project_->timeline()->trackById(trackId);
    if (!t)
        return -1;

    qint64 best = -1;
    qint64 bestDist = toleranceFrames + 1;
    for (const auto& c : t->clips()) {
        for (const qint64 edge : { qint64(c->range().start), qint64(c->range().end()) }) {
            const qint64 dist = qAbs(edge - frame);
            if (dist <= toleranceFrames && dist < bestDist) {
                best     = edge;
                bestDist = dist;
            }
        }
    }
    return best;
}

bool EditController::dropLibraryItem(const QString& payloadJson, const QUuid& trackId,
                                     qint64 frame, const QUuid& targetClipId)
{
    lastDropError_.clear();

    qInfo() << "[dropLibraryItem] payload:" << payloadJson << "trackId:" << trackId
            << "frame:" << frame << "targetClipId:" << targetClipId;

    const auto fail = [this](const QString& reason) {
        lastDropError_ = reason;
        emit dropRejected(reason);
        qWarning() << "[dropLibraryItem] REJECTED:" << reason;
        return false;
    };

    if (!project_)
        return fail(tr("No project is open."));

    const DropPayload payload = parsePayload(payloadJson);
    if (!payload.valid)
        return fail(tr("The dragged item could not be read."));

    Track* track = project_->timeline()->trackById(trackId);
    if (!track)
        return fail(tr("The drop target track no longer exists."));

    if (frame < 0)
        frame = 0;

    // ---- クリップの上へ落ちたもの: フィルタ / エフェクト ----
    if (!targetClipId.isNull()) {
        auto clip = track->clipById(targetClipId);
        if (!clip)
            return fail(tr("The drop target clip no longer exists."));

        if (payload.category == QLatin1String("filter")) {
            VideoFilterInstance inst;
            inst.filterId = payload.itemId;
            if (const VideoFilterDesc* desc = findVideoFilterDesc(payload.itemId))
                inst.params = desc->defaultParams;
            project_->undoStack()->push(new AddFilterCommand(project_, targetClipId,
                                                             std::move(inst)));
            return true;
        }

        if (payload.category == QLatin1String("effect")) {
            if (clip->type() != ClipType::Subtitle && clip->type() != ClipType::Title)
                return fail(tr("Effects can only be applied to subtitle or title clips."));
            project_->undoStack()->push(
                new subtitle::AddSubtitleEffectCommand(project_, targetClipId, payload.itemId));
            return true;
        }
        return fail(tr("This item cannot be dropped onto a clip."));
    }

    // ---- トランジション: クリップ境界へ ----
    if (payload.category == QLatin1String("transition")) {
        const qint64 boundary = clipBoundaryNear(trackId, frame, 30);
        if (boundary < 0)
            return fail(tr("Drop a transition onto the boundary between two clips."));

        Transition t;
        t.transitionId   = payload.itemId;
        t.centerFrame    = boundary;
        t.durationFrames = kDefaultTransFrames;
        if (const TransitionDesc* desc = findTransitionDesc(payload.itemId))
            t.params = desc->defaultParams;

        auto* cmd = new AddTransitionCommand(project_, trackId, std::move(t));
        project_->undoStack()->push(cmd);
        if (!cmd->error().isEmpty())
            return fail(cmd->error());
        return true;
    }

    if (!canDropOnTrack(payload.category, trackId))
        return fail(tr("This item cannot be placed on that track."));

    // ---- メディア ----
    if (payload.category == QLatin1String("media")) {
        const qint64 duration = payload.duration > 0 ? payload.duration : kDefaultClipFrames;
        return addAssetClip(-1, trackId, payload.assetId.toString(QUuid::WithoutBraces),
                            frame, duration);
    }

    // ---- タイトル ----
    if (payload.category == QLatin1String("title")) {
        auto clip = std::make_shared<subtitle::TitleClip>();
        clip->applyPreset(payload.itemId);
        qint64 duration = kDefaultClipFrames;
        if (const auto* preset = subtitle::findTitlePreset(payload.itemId))
            duration = preset->defaultDurationFrames;
        clip->setRange({frame, duration});
        project_->undoStack()->push(new AddClipCommand(project_, trackId, -1, clip));
        return true;
    }

    // ---- 字幕 ----
    if (payload.category == QLatin1String("subtitle")) {
        auto clip = std::make_shared<subtitle::SubtitleClip>();
        clip->setRange({frame, kDefaultClipFrames});
        clip->setName(payload.name);
        if (payload.itemId.startsWith(QLatin1String("yave.subtitle.preset.")))
            clip->setStylePresetId(payload.itemId.section(QLatin1Char('.'), -1));
        project_->undoStack()->push(new AddClipCommand(project_, trackId, -1, clip));
        return true;
    }

    return fail(tr("This item cannot be dropped here."));
}

// ===========================================================================
//  クリップ編集
// ===========================================================================

bool EditController::addClip(int trackIndex, const QUuid& trackId,
                             qint64 startFrame, qint64 durationFrames)
{
    if (!project_)
        return false;

    auto clip = std::make_shared<VideoClip>();
    clip->setRange({startFrame, durationFrames});
    auto* cmd = new AddClipCommand(project_, trackId, trackIndex, clip);
    project_->undoStack()->push(cmd);
    return true;
}

bool EditController::addAssetClip(int trackIndex, const QUuid& trackId, const QString& assetIdStr,
                                  qint64 startFrame, qint64 durationFrames)
{
    lastDropError_.clear();

    if (!project_)
        return false;

    const QUuid assetId(assetIdStr);
    const Asset* asset = project_->assets()->asset(assetId);

    // durationFrames <= 0 のときはアセットの実尺を使う (D&D 直後の配置)
    if (asset && durationFrames <= 0)
        durationFrames = asset->durationFrames > 0 ? asset->durationFrames : 180;

    std::shared_ptr<Clip> clip;
    if (asset && asset->kind == Asset::Kind::Audio) {
        auto audioClip = std::make_shared<AudioClip>(assetId);
        audioClip->setRange({startFrame, durationFrames});
        audioClip->setName(QFileInfo(asset->resolvedAbsolutePath).fileName());
        audioClip->setMaxDurationFrames(asset->durationFrames);
        clip = std::move(audioClip);
    } else {
        auto videoClip = std::make_shared<VideoClip>(assetId);
        videoClip->setRange({startFrame, durationFrames});
        if (asset) {
            videoClip->setName(QFileInfo(asset->resolvedAbsolutePath).fileName());
            videoClip->setMaxDurationFrames(asset->durationFrames);
        } else {
            videoClip->setName(tr("Clip"));
        }
        clip = std::move(videoClip);
    }

    // 落下先トラックがクリップ種別を受け付けない場合 (音声→映像トラックなど) は、
    // 種別に合うトラックを探してそこへ置く。無ければ新規作成する。
    Timeline* tl = project_->timeline();
    Track* dstTrack = trackId.isNull() ? nullptr : tl->trackById(trackId);
    if (!dstTrack || !dstTrack->acceptsClip(*clip)) {
        dstTrack = nullptr;
        const TrackType needed = clip->type() == ClipType::Audio ? TrackType::Audio
                                                                 : TrackType::Video;
        for (int i = 0; i < tl->trackCount(); ++i) {
            Track* t = tl->trackAt(i);
            if (t->acceptsClip(*clip)) {
                dstTrack = t;
                break;
            }
        }
        if (!dstTrack)
            dstTrack = tl->appendTrack(needed);
    }

    qInfo() << "[addAssetClip] assetId:" << assetIdStr << "assetFound:" << (asset != nullptr)
            << "assetKind:" << (asset ? int(asset->kind) : -1)
            << "dstTrackType:" << (dstTrack ? int(dstTrack->type()) : -1)
            << "startFrame:" << startFrame << "durationFrames:" << durationFrames;

    auto* cmd = new AddClipCommand(project_, dstTrack->id(), tl->indexOfTrack(dstTrack), clip);
    project_->undoStack()->push(cmd);
    qInfo() << "[addAssetClip] inserted:" << cmd->wasInserted();
    if (!cmd->wasInserted()) {
        lastDropError_ = cmd->rejectReason();
        emit dropRejected(lastDropError_);
        return false;
    }
    return true;
}

bool EditController::addClipToTrack(int trackIndex, const QUuid& trackId,
                                    qint64 startFrame, qint64 durationFrames)
{
    if (!project_)
        return false;

    Track* t = project_->timeline()->trackById(trackId);
    if (!t)
        t = project_->timeline()->trackAt(trackIndex);
    if (!t)
        return false;

    std::shared_ptr<Clip> clip;
    switch (t->type()) {
    case TrackType::Audio:
        clip = std::make_shared<AudioClip>();
        break;
    case TrackType::Subtitle:
        clip = std::make_shared<subtitle::SubtitleClip>();
        break;
    default:
        clip = std::make_shared<VideoClip>();
        break;
    }
    clip->setRange({startFrame, durationFrames});

    auto* cmd = new AddClipCommand(project_, trackId, trackIndex, clip);
    project_->undoStack()->push(cmd);
    return true;
}

void EditController::removeClip(const QUuid& clipId)
{
    if (!project_ || !project_->timeline())
        return;

    Track* owner = nullptr;
    if (auto clip = project_->timeline()->findClip(clipId, &owner); clip && owner) {
        auto* cmd = new RemoveClipCommand(project_, owner->id(), clipId);
        project_->undoStack()->push(cmd);
    }
}

void EditController::moveClip(const QUuid& fromTrackId, const QUuid& toTrackId,
                              const QUuid& clipId, qint64 newStart, qint64 newDuration)
{
    if (!project_)
        return;
    auto* cmd = new MoveClipCommand(project_, fromTrackId, toTrackId, clipId,
                                    {newStart, newDuration});
    project_->undoStack()->push(cmd);
}

void EditController::trimClip(const QUuid& trackId, const QUuid& clipId,
                              qint64 newSourceOffset, qint64 newStart,
                              qint64 newDuration)
{
    if (!project_)
        return;
    auto* cmd = new TrimClipCommand(project_, trackId, clipId, newSourceOffset,
                                    {newStart, newDuration});
    project_->undoStack()->push(cmd);
}

bool EditController::splitClip(const QUuid& trackId, const QUuid& clipId,
                               qint64 splitFrame)
{
    if (!project_)
        return false;
    Track* t = project_->timeline()->trackById(trackId);
    if (!t)
        return false;
    auto c = t->clipById(clipId);
    if (!c || !c->range().contains(splitFrame))
        return false;

    project_->undoStack()->push(new SplitClipCommand(project_, trackId, clipId,
                                                     splitFrame));
    return true;
}

void EditController::rippleDelete(const TimeRange& range)
{
    if (!project_ || range.isEmpty())
        return;

    std::vector<int> allTracks;
    for (int i = 0; i < project_->timeline()->trackCount(); ++i)
        allTracks.push_back(i);
    project_->undoStack()->push(new RippleDeleteCommand(project_, allTracks, range));
}

// ===========================================================================
//  トラック
// ===========================================================================

int EditController::addTrack(const QString& type, int index)
{
    if (!project_)
        return -1;

    TrackType tt = TrackType::Video;
    if (type == QLatin1String("audio"))
        tt = TrackType::Audio;
    else if (type == QLatin1String("subtitle"))
        tt = TrackType::Subtitle;
    else if (type == QLatin1String("aiGenerated"))
        tt = TrackType::AiGenerated;

    const int targetIndex =
        index < 0 ? project_->timeline()->trackCount() : index;
    project_->undoStack()->push(
        new AddTrackCommand(project_, tt, targetIndex));
    return targetIndex;
}

void EditController::removeTrack(const QUuid& trackId)
{
    if (!project_)
        return;
    project_->undoStack()->push(new RemoveTrackCommand(project_, trackId));
}

void EditController::reorderTracks(int from, int to)
{
    if (!project_)
        return;
    project_->undoStack()->push(new ReorderTrackCommand(project_, from, to));
}

void EditController::undo() const
{
    if (project_)
        project_->undoStack()->undo();
}

void EditController::redo() const
{
    if (project_)
        project_->undoStack()->redo();
}

// ===========================================================================
//  インスペクタ (1.7.1 inspector)
// ===========================================================================

namespace {

ClipProperty clipPropertyFromKey(const QString& key)
{
    if (key == QLatin1String("name"))         return ClipProperty::Name;
    if (key == QLatin1String("start"))        return ClipProperty::Start;
    if (key == QLatin1String("duration"))     return ClipProperty::Duration;
    if (key == QLatin1String("sourceOffset")) return ClipProperty::SourceOffset;
    if (key == QLatin1String("opacity"))      return ClipProperty::Opacity;
    if (key == QLatin1String("blendMode"))    return ClipProperty::BlendMode;
    if (key == QLatin1String("fadeIn"))       return ClipProperty::FadeIn;
    if (key == QLatin1String("fadeOut"))      return ClipProperty::FadeOut;
    if (key == QLatin1String("enabled"))      return ClipProperty::Enabled;
    if (key == QLatin1String("locked"))       return ClipProperty::Locked;
    if (key == QLatin1String("gain"))         return ClipProperty::Gain;
    if (key == QLatin1String("pan"))          return ClipProperty::Pan;
    return ClipProperty::Name;
}

TrackProperty trackPropertyFromKey(const QString& key)
{
    if (key == QLatin1String("name"))      return TrackProperty::Name;
    if (key == QLatin1String("gain"))      return TrackProperty::Gain;
    if (key == QLatin1String("pan"))       return TrackProperty::Pan;
    if (key == QLatin1String("muted"))     return TrackProperty::Muted;
    if (key == QLatin1String("solo"))      return TrackProperty::Solo;
    if (key == QLatin1String("opacity"))   return TrackProperty::Opacity;
    if (key == QLatin1String("blendMode")) return TrackProperty::BlendMode;
    if (key == QLatin1String("visible"))   return TrackProperty::Visible;
    if (key == QLatin1String("locked"))    return TrackProperty::Locked;
    if (key == QLatin1String("height"))    return TrackProperty::UiHeight;
    return TrackProperty::Name;
}

} // anonymous namespace

void EditController::setClipProperty(const QString& clipId, const QString& prop,
                                     const QVariant& value)
{
    if (!project_)
        return;
    project_->undoStack()->push(new SetClipPropertyCommand(
        project_, QUuid(clipId), clipPropertyFromKey(prop), value));
}

void EditController::setSubtitleText(const QString& clipId, const QString& text)
{
    if (!project_)
        return;
    project_->undoStack()->push(
        new subtitle::EditSubtitleTextCommand(project_, QUuid(clipId), text));
}

void EditController::setSubtitleStyle(const QString& clipId, const QString& prop,
                                      const QVariant& value)
{
    if (!project_)
        return;
    const auto field = subtitle::subtitleStyleFieldFromName(prop);
    if (!field)
        return;
    project_->undoStack()->push(
        new subtitle::SetSubtitleStyleCommand(project_, QUuid(clipId), *field, value));
}

// ===========================================================================
//  字幕 SRT 取り込み
// ===========================================================================

QVariantMap EditController::importSrt(const QString& pathOrUrl, const QVariantMap& options)
{
    QVariantMap result;
    result[QStringLiteral("ok")]            = false;
    result[QStringLiteral("importedCount")] = 0;
    result[QStringLiteral("trackId")]       = QString();
    result[QStringLiteral("trackIndex")]    = -1;
    result[QStringLiteral("warnings")]      = QStringList();
    result[QStringLiteral("skipped")]       = false;

    // file:// URL はローカルパスへ変換する (D&D から直接呼ばれるため)
    QString path = pathOrUrl;
    if (pathOrUrl.contains(QLatin1String("://"))) {
        const QUrl url(pathOrUrl);
        if (url.isLocalFile())
            path = url.toLocalFile();
    }

    qInfo() << "[importSrt] path:" << path;

    if (!project_)
        return result;

    // ---- 同一ファイルは無視 (2 回目以降) ----
    const QString normPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (importedSrtTracks_.contains(normPath)) {
        result[QStringLiteral("ok")]      = true;
        result[QStringLiteral("skipped")] = true;
        qInfo() << "[importSrt] SKIPPED (already imported):" << normPath
                << "trackIndex=" << importedSrtTracks_.value(normPath);
        return result;
    }

    const subtitle::SrtParseResult parsed = subtitle::SrtParser::parseFile(path);
    QStringList warnings = parsed.warnings;
    if (!parsed.ok) {
        result[QStringLiteral("warnings")] = warnings;
        qWarning() << "[importSrt] parse FAILED:" << warnings.join(';');
        return result;
    }

    const int overlapPolicyInt = options.value(QStringLiteral("overlapPolicy"), 0).toInt();
    OverlapPolicy policy = OverlapPolicy::SplitToNewTracks;
    if (overlapPolicyInt == 1)
        policy = OverlapPolicy::TrimPrevious;
    else if (overlapPolicyInt == 2)
        policy = OverlapPolicy::SkipOverlapping;

    // ---- 配置先トラックの決定 ----
    // 初回: 既存の字幕トラック (最初に見つけたもの) を再利用。
    // 2 回目以降の異なる SRT: 必ず新しい字幕トラックを作る。
    int targetTrackIndex = -1;
    if (importedSrtTracks_.isEmpty()) {
        const int explicitTarget = options.value(QStringLiteral("targetTrackIndex"), -1).toInt();
        if (explicitTarget >= 0) {
            const Track* t = project_->timeline()->trackAt(explicitTarget);
            if (t && t->type() == TrackType::Subtitle)
                targetTrackIndex = explicitTarget;
        }
        if (targetTrackIndex < 0) {
            for (int i = 0; i < project_->timeline()->trackCount(); ++i) {
                if (project_->timeline()->trackAt(i)->type() == TrackType::Subtitle) {
                    targetTrackIndex = i;
                    break;
                }
            }
        }
    }
    qInfo() << "[importSrt] targetTrackIndex=" << targetTrackIndex
            << " (firstImport=" << importedSrtTracks_.isEmpty() << ")";

    const QString stylePresetId =
        options.value(QStringLiteral("stylePresetId"), QStringLiteral("default")).toString();
    const qint64 fadeInFrames  = options.value(QStringLiteral("fadeInFrames"), 0).toLongLong();
    const qint64 fadeOutFrames = options.value(QStringLiteral("fadeOutFrames"), 0).toLongLong();

    QStringList convertWarnings;
    auto clips = subtitle::convertCuesToClips(parsed, project_->timebase(), stylePresetId,
                                              &convertWarnings);
    warnings += convertWarnings;

    for (auto& c : clips) {
        c->setFadeInFrames(fadeInFrames);
        c->setFadeOutFrames(fadeOutFrames);
    }

    std::vector<std::shared_ptr<Clip>> genericClips(clips.begin(), clips.end());
    auto* cmd = new ImportSubtitleCommand(project_, std::move(genericClips), policy,
                                          targetTrackIndex, TrackType::Subtitle,
                                          QFileInfo(path).fileName());
    project_->undoStack()->push(cmd);

    result[QStringLiteral("ok")]            = true;
    result[QStringLiteral("importedCount")] = int(cmd->insertedCount());
    const int baseIdx = cmd->baseTrackIndex();
    result[QStringLiteral("trackIndex")]    = baseIdx;
    if (baseIdx >= 0) {
        if (Track* t = project_->timeline()->trackAt(baseIdx))
            result[QStringLiteral("trackId")] = t->id().toString(QUuid::WithoutBraces);
        importedSrtTracks_[normPath] = baseIdx;
    }
    result[QStringLiteral("warnings")] = warnings;
    qInfo() << "[importSrt] OK imported=" << cmd->insertedCount()
            << "trackIndex=" << baseIdx
            << "warnings=" << warnings.join(';');
    return result;
}

void EditController::setTrackProperty(const QString& trackId, const QString& prop,
                                      const QVariant& value)
{
    if (!project_)
        return;
    project_->undoStack()->push(new SetTrackPropertyCommand(
        project_, QUuid(trackId), trackPropertyFromKey(prop), value));
}

QVariantList EditController::snapCandidates(qint64 visibleStart,
                                            qint64 visibleEnd) const
{
    QVariantList out;
    if (!project_)
        return out;

    std::vector<int> visibleTracks;
    for (int i = 0; i < project_->timeline()->trackCount(); ++i)
        visibleTracks.push_back(i);

    for (int64_t f : project_->timeline()->snapCandidates({visibleStart,
                                                           visibleEnd - visibleStart},
                                                          visibleTracks))
        out << qint64(f);
    return out;
}

} // namespace yave
