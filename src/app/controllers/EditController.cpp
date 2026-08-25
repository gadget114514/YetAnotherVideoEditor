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
#include "../../core/Transition.h"
#include "../../core/VideoFilter.h"
#include "../../subtitle/TitleClip.h"
#include "../../subtitle/commands/AddSubtitleEffectCommand.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

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

    const auto fail = [this](const QString& reason) {
        lastDropError_ = reason;
        emit dropRejected(reason);
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
    if (!project_)
        return false;

    QUuid assetId(assetIdStr);
    auto clip = std::make_shared<VideoClip>(assetId);
    clip->setRange({startFrame, durationFrames});

    // アセットの情報を探して、名前や最大フレーム数をコピーする
    if (auto* asset = project_->assets()->asset(assetId)) {
        clip->setName(QFileInfo(asset->resolvedAbsolutePath).fileName());
        clip->setMaxDurationFrames(asset->durationFrames);
    } else {
        clip->setName(tr("Clip"));
    }

    auto* cmd = new AddClipCommand(project_, trackId, trackIndex, clip);
    project_->undoStack()->push(cmd);
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
