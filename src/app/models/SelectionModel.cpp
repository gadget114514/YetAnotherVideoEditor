#include "SelectionModel.h"

#include "../../core/AudioClip.h"
#include "../../core/Clip.h"
#include "../../core/Project.h"
#include "../../core/Timeline.h"
#include "../../core/Track.h"
#include "../../subtitle/SubtitleClip.h"
#include "../../subtitle/SubtitleStylePreset.h"

namespace yave {

namespace {

QString clipTypeName(ClipType t)
{
    switch (t) {
    case ClipType::Video:        return QStringLiteral("video");
    case ClipType::Audio:        return QStringLiteral("audio");
    case ClipType::Subtitle:     return QStringLiteral("subtitle");
    case ClipType::Title:        return QStringLiteral("title");
    case ClipType::AiPlaceholder:return QStringLiteral("ai placeholder");
    case ClipType::Image:        return QStringLiteral("image");
    case ClipType::Color:        return QStringLiteral("color");
    }
    return QStringLiteral("unknown");
}

QString trackTypeKey(TrackType t)
{
    switch (t) {
    case TrackType::Video:       return QStringLiteral("video");
    case TrackType::Audio:       return QStringLiteral("audio");
    case TrackType::Subtitle:    return QStringLiteral("subtitle");
    case TrackType::AiGenerated: return QStringLiteral("aiGenerated");
    }
    return QStringLiteral("unknown");
}

} // anonymous namespace

SelectionModel::SelectionModel(QObject* parent) : QObject(parent) {}

void SelectionModel::setProject(Project* project)
{
    project_ = project;
    clipId_  = QUuid();
    trackId_ = QUuid();
    if (!project_)
        return;

    Timeline* tl = project_->timeline();
    connect(tl, &Timeline::structureChanged, this, &SelectionModel::onStructureChanged);
    connect(tl, &Timeline::clipRemoved, this, [this](const QUuid&, const QUuid& clipId) {
        if (clipId == clipId_)
            clear();
    });
    connect(tl, &Timeline::trackRemoved, this, [this](int) {
        if (project_ && project_->timeline() && !trackId_.isNull()
            && !project_->timeline()->trackById(trackId_))
            clear();
    });
    emit selectionChanged();
}

void SelectionModel::selectClip(const QString& trackId, const QString& clipId)
{
    trackId_ = QUuid(trackId);
    clipId_  = QUuid(clipId);
    emit selectionChanged();
}

void SelectionModel::selectTrack(const QString& trackId)
{
    trackId_ = QUuid(trackId);
    clipId_  = QUuid();
    emit selectionChanged();
}

void SelectionModel::clear()
{
    if (clipId_.isNull() && trackId_.isNull())
        return;
    clipId_  = QUuid();
    trackId_ = QUuid();
    emit selectionChanged();
}

void SelectionModel::onStructureChanged()
{
    // 編集コマンドが発行する structureChanged を受けて、選択物の生存確認と
    // インスペクタの再表示 (selectionChanged) を行う。
    if (project_ && project_->timeline()) {
        if (!clipId_.isNull()) {
            if (!project_->timeline()->findClip(clipId_)) {
                clipId_  = QUuid();
                trackId_ = QUuid();
                emit selectionChanged();
                return;
            }
        } else if (!trackId_.isNull()) {
            if (!project_->timeline()->trackById(trackId_)) {
                clipId_  = QUuid();
                trackId_ = QUuid();
                emit selectionChanged();
                return;
            }
        }
    }
    if (!clipId_.isNull() || !trackId_.isNull())
        emit selectionChanged();
}

QVariantMap SelectionModel::clipInfo() const
{
    QVariantMap m;
    if (clipId_.isNull() || !project_ || !project_->timeline())
        return m;

    auto clip = project_->timeline()->findClip(clipId_);
    if (!clip)
        return m;

    m[QStringLiteral("name")]          = clip->name();
    m[QStringLiteral("type")]          = clipTypeName(clip->type());
    m[QStringLiteral("start")]         = qint64(clip->range().start);
    m[QStringLiteral("duration")]      = qint64(clip->range().duration);
    m[QStringLiteral("end")]           = qint64(clip->range().end());
    m[QStringLiteral("sourceOffset")]  = qint64(clip->sourceOffset());
    m[QStringLiteral("maxDuration")]   = qint64(clip->maxDuration());
    m[QStringLiteral("opacity")]       = clip->opacity();
    m[QStringLiteral("blendMode")]     = int(clip->blendMode());
    m[QStringLiteral("fadeIn")]        = qint64(clip->fadeInFrames());
    m[QStringLiteral("fadeOut")]       = qint64(clip->fadeOutFrames());
    m[QStringLiteral("enabled")]       = clip->isEnabled();
    m[QStringLiteral("locked")]        = clip->isLocked();
    m[QStringLiteral("generatedByAi")] = clip->isAiGenerated();

    if (const auto* ac = dynamic_cast<const AudioClip*>(clip.get())) {
        m[QStringLiteral("gain")]    = ac->gain();
        m[QStringLiteral("pan")]     = ac->pan();
        m[QStringLiteral("isAudio")] = true;
    } else {
        m[QStringLiteral("gain")]    = 0.0;
        m[QStringLiteral("pan")]     = 0.0;
        m[QStringLiteral("isAudio")] = false;
    }

    if (clip->type() == ClipType::Subtitle || clip->type() == ClipType::Title) {
        const auto* sc = static_cast<const subtitle::SubtitleClip*>(clip.get());
        m[QStringLiteral("text")]          = sc->plainText();
        m[QStringLiteral("isSubtitle")]    = true;
        m[QStringLiteral("stylePresetId")] = sc->stylePresetId();

        // 解決済みスタイル (既定プリセット + クリップ固有の差分)。
        // 現在のアプリは既定プリセットのみを持つため、テーブルは既定で構築する。
        subtitle::SubtitleStylePresetTable presets;
        const subtitle::SubtitleStyle s = sc->resolvedStyle(presets);

        m[QStringLiteral("fontFamily")]    = s.fontFamily;
        m[QStringLiteral("fontPointSize")] = s.fontPointSize;
        m[QStringLiteral("fontWeight")]    = s.fontWeight;
        m[QStringLiteral("italic")]        = s.italic;
        m[QStringLiteral("fillColor")]     = s.fillColor.name(QColor::HexArgb);
        m[QStringLiteral("outlineColor")]  = s.outlineColor.name(QColor::HexArgb);
        m[QStringLiteral("outlineWidth")]  = s.outlineWidth;
        m[QStringLiteral("shadowColor")]   = s.shadowColor.name(QColor::HexArgb);
        m[QStringLiteral("shadowBlur")]    = s.shadowBlur;
        m[QStringLiteral("boxEnabled")]    = s.boxEnabled;
        m[QStringLiteral("boxColor")]      = s.boxColor.name(QColor::HexArgb);
        m[QStringLiteral("hAlign")]        = int(s.hAlign);
        m[QStringLiteral("vAlign")]        = int(s.vAlign);
        m[QStringLiteral("anchorX")]       = s.anchor.x();
        m[QStringLiteral("anchorY")]       = s.anchor.y();
        m[QStringLiteral("lineSpacing")]   = s.lineSpacing;
        m[QStringLiteral("letterSpacing")] = s.letterSpacing;
        m[QStringLiteral("maxWidthRatio")] = s.maxWidthRatio;
        m[QStringLiteral("rotationDeg")]   = s.rotationDeg;
        m[QStringLiteral("scaleX")]        = s.scale.x();
        m[QStringLiteral("scaleY")]        = s.scale.y();
        m[QStringLiteral("styleOpacity")]  = s.opacity;
        m[QStringLiteral("vertical")]      = s.vertical;
    } else {
        m[QStringLiteral("isSubtitle")] = false;
    }

    return m;
}

QVariantMap SelectionModel::trackInfo() const
{
    QVariantMap m;
    if (trackId_.isNull() || !project_ || !project_->timeline())
        return m;

    const Track* t = project_->timeline()->trackById(trackId_);
    if (!t)
        return m;

    m[QStringLiteral("name")]      = t->name();
    m[QStringLiteral("type")]      = trackTypeKey(t->type());
    m[QStringLiteral("gain")]      = t->gain();
    m[QStringLiteral("pan")]       = t->pan();
    m[QStringLiteral("muted")]     = t->isMuted();
    m[QStringLiteral("solo")]      = t->isSolo();
    m[QStringLiteral("opacity")]   = t->opacity();
    m[QStringLiteral("blendMode")] = int(t->blendMode());
    m[QStringLiteral("visible")]   = t->isVisible();
    m[QStringLiteral("locked")]    = t->isLocked();
    m[QStringLiteral("height")]    = t->uiHeight();
    m[QStringLiteral("clipCount")] = int(t->clipCount());
    m[QStringLiteral("isAudio")]   = t->type() == TrackType::Audio;
    m[QStringLiteral("isVideo")]   = t->type() == TrackType::Video;
    m[QStringLiteral("isSubtitle")]= t->type() == TrackType::Subtitle;
    return m;
}

} // namespace yave