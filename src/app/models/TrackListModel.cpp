#include "TrackListModel.h"

#include "../core/Clip.h"
#include "../core/Project.h"
#include "../core/Timeline.h"
#include "../core/Track.h"
#include "../subtitle/SubtitleClip.h"

namespace yave {

// ===========================================================================
//  TrackListModel
// ===========================================================================

TrackListModel::TrackListModel(QObject* parent) : QAbstractListModel(parent) {}

void TrackListModel::setProject(Project* project)
{
    beginResetModel();
    project_ = project;
    endResetModel();

    if (!project_)
        return;

    // Timeline の構造変更をモデルへ反映する
    Timeline* tl = project_->timeline();
    connect(tl, &Timeline::trackInserted, this, &TrackListModel::rebuild, Qt::UniqueConnection);
    connect(tl, &Timeline::trackRemoved,  this, &TrackListModel::rebuild, Qt::UniqueConnection);
    connect(tl, &Timeline::trackMoved,    this, &TrackListModel::rebuild, Qt::UniqueConnection);
}

void TrackListModel::rebuild()
{
    beginResetModel();
    // tracks_ への参照は Timeline が持つため、ここでは reset だけでよい
    endResetModel();
}

int TrackListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !project_ || !project_->timeline())
        return 0;
    return project_->timeline()->trackCount();
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const
{
    const Track* t = trackAt(index.row());
    if (!t)
        return {};

    switch (role) {
    case TrackIdRole:   return t->id().toString(QUuid::WithoutBraces);
    case NameRole:      return t->name();
    case TypeRole:      return QString::fromLatin1([](TrackType type) {
        switch (type) {
        case TrackType::Video:    return "video";
        case TrackType::Audio:    return "audio";
        case TrackType::Subtitle: return "subtitle";
        case TrackType::AiGenerated: return "aiGenerated";
        }
        return "video";
    }(t->type()));
    case ColorRole:     return t->color();
    case HeightRole:    return t->uiHeight();
    case VisibleRole:   return t->isVisible();
    case LockedRole:    return t->isLocked();
    case MutedRole:     return t->isMuted();
    case SoloRole:      return t->isSolo();
    case GainRole:      return t->gain();
    case PanRole:       return t->pan();
    case OpacityRole:   return t->opacity();
    case BlendModeRole: return int(t->blendMode());
    }
    return {};
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { TrackIdRole,   "trackId" },
        { NameRole,      "name" },
        { TypeRole,      "type" },
        { ColorRole,     "color" },
        { HeightRole,    "height" },
        { VisibleRole,   "visible" },
        { LockedRole,    "locked" },
        { MutedRole,     "muted" },
        { SoloRole,      "solo" },
        { GainRole,      "gain" },
        { PanRole,       "pan" },
        { OpacityRole,   "opacity" },
        { BlendModeRole, "blendMode" },
    };
    return roles;
}

void TrackListModel::moveTrack(int from, int to)
{
    if (!project_)
        return;
    project_->timeline()->moveTrack(from, to);
}

Track* TrackListModel::trackAt(int row) const
{
    if (!project_ || !project_->timeline())
        return nullptr;
    return project_->timeline()->trackAt(row);
}

QObject* TrackListModel::clipModelProvider(int row)
{
    Track* t = trackAt(row);
    if (!t || !project_)
        return nullptr;
    auto* model = new ClipListModel(nullptr);
    model->setTrack(project_->timeline(), t);
    return model;
}

// ===========================================================================
//  ClipListModel
// ===========================================================================

ClipListModel::ClipListModel(QObject* parent) : QAbstractListModel(parent) {}

void ClipListModel::setTrack(Timeline* timeline, Track* track)
{
    beginResetModel();
    if (timeline_) {
        timeline_->disconnect(this);
    }
    timeline_ = timeline;
    track_ = track;
    clips_.clear();
    if (track_) {
        clips_ = track_->clips();
        if (timeline_) {
            connect(timeline_, &Timeline::clipInserted, this, [this](const QUuid& trackId, const QUuid& clipId) {
                if (track_ && track_->id() == trackId) {
                    beginResetModel();
                    clips_ = track_->clips();
                    endResetModel();
                }
            });
            connect(timeline_, &Timeline::clipRemoved, this, [this](const QUuid& trackId, const QUuid& clipId) {
                if (track_ && track_->id() == trackId) {
                    beginResetModel();
                    clips_ = track_->clips();
                    endResetModel();
                }
            });
            connect(timeline_, &Timeline::clipChanged, this, [this](const QUuid& trackId, const QUuid& clipId) {
                if (track_ && track_->id() == trackId) {
                    beginResetModel();
                    clips_ = track_->clips();
                    endResetModel();
                }
            });
        }
    }
    endResetModel();
}

int ClipListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(clips_.size());
}

QVariant ClipListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= int(clips_.size()))
        return {};
    const Clip& c = *clips_[size_t(index.row())];

    switch (role) {
    case ClipIdRole:    return c.id().toString(QUuid::WithoutBraces);
    case StartRole:     return qint64(c.range().start);
    case DurationRole:  return qint64(c.range().duration);
    case EndRole:       return qint64(c.range().end());
    case NameRole:      return c.name();
    case EnabledRole:   return c.isEnabled();
    case LockedRole:    return c.isLocked();
    case OpacityRole:   return c.opacity();
    case FadeInRole:    return qint64(c.fadeInFrames());
    case FadeOutRole:   return qint64(c.fadeOutFrames());
    case GeneratedByAiRole: return c.isAiGenerated();
    case TextPreviewRole:
        if (c.type() == ClipType::Subtitle)
            return static_cast<const subtitle::SubtitleClip&>(c).plainText()
                       .left(20);
        break;
    case HasEffectsRole:
        if (c.type() == ClipType::Subtitle)
            return !static_cast<const subtitle::SubtitleClip&>(c).effectStack().empty();
        break;
    case MissingEffectsRole:
        if (c.type() == ClipType::Subtitle)
            return static_cast<const subtitle::SubtitleClip&>(c).hasMissingEffects();
        break;
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> ClipListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { ClipIdRole,          "clipId" },
        { StartRole,           "start" },
        { DurationRole,        "duration" },
        { EndRole,             "end" },
        { NameRole,            "name" },
        { EnabledRole,         "enabled" },
        { LockedRole,          "locked" },
        { OpacityRole,         "opacity" },
        { FadeInRole,          "fadeIn" },
        { FadeOutRole,         "fadeOut" },
        { TextPreviewRole,     "textPreview" },
        { HasEffectsRole,      "hasEffects" },
        { MissingEffectsRole,  "missingEffects" },
        { GeneratedByAiRole,   "generatedByAi" },
        { ProgressRole,        "progress" },
    };
    return roles;
}

} // namespace yave
