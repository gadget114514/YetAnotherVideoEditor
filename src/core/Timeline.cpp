#include "Timeline.h"
#include "AiPlaceholderClip.h"
#include "Project.h"
#include "Track.h"

#include <QtGlobal>

#include <algorithm>

namespace yave {

namespace {

QString defaultTrackName(TrackType type, int ordinal)
{
    switch (type) {
    case TrackType::Video:    return QObject::tr("Video %1").arg(ordinal);
    case TrackType::Audio:    return QObject::tr("Audio %1").arg(ordinal);
    case TrackType::Subtitle: return QObject::tr("Subtitles %1").arg(ordinal);
    case TrackType::AiGenerated:
        return QObject::tr("AI Generated %1").arg(ordinal);
    }
    return QObject::tr("Track %1").arg(ordinal);
}

} // anonymous namespace

// ===========================================================================
//  構築 / 破棄
// ===========================================================================

Timeline::Timeline(Project* project, QObject* parent)
    : QObject(parent), project_(project)
{}

Timeline::~Timeline() = default;

void Timeline::bumpRevision()
{
    ++revision_;
}

void Timeline::notifyClipCountChanged()
{
    const int64_t d = duration();
    if (d != lastDuration_) {
        lastDuration_ = d;
        emit durationChanged(d);
    }
}

// ===========================================================================
//  トラック操作
// ===========================================================================

Track* Timeline::appendTrack(TrackType type, const QString& name)
{
    return insertTrack(trackCount(), type, name);
}

Track* Timeline::insertTrack(int index, TrackType type, const QString& name)
{
    auto track = std::make_unique<Track>(type);
    track->setName(name.isEmpty()
                       ? defaultTrackName(type, trackCount() + 1)
                       : name);

    Track* raw = track.get();
    index = qBound(0, index, int(tracks_.size()));
    tracks_.insert(tracks_.begin() + index, std::move(track));

    bumpRevision();
    emit trackInserted(index);
    emit structureChanged();
    notifyClipCountChanged();
    return raw;
}

std::unique_ptr<Track> Timeline::takeTrack(int index)
{
    if (index < 0 || index >= int(tracks_.size()))
        return nullptr;
    auto out = std::move(tracks_[size_t(index)]);
    tracks_.erase(tracks_.begin() + index);
    bumpRevision();
    emit trackRemoved(index);
    emit structureChanged();
    notifyClipCountChanged();
    return out;
}

std::unique_ptr<Track> Timeline::takeTrackById(const QUuid& id)
{
    return takeTrack(indexOfTrack(id));
}

void Timeline::reinsertTrack(int index, std::unique_ptr<Track> track)
{
    if (!track)
        return;
    index = qBound(0, index, int(tracks_.size()));
    tracks_.insert(tracks_.begin() + index, std::move(track));
    bumpRevision();
    emit trackInserted(index);
    emit structureChanged();
    notifyClipCountChanged();
}

void Timeline::moveTrack(int from, int to)
{
    if (from == to || from < 0 || to < 0)
        return;
    if (from >= int(tracks_.size()) || to >= int(tracks_.size()))
        return;

    if (from < to)
        std::rotate(tracks_.begin() + from, tracks_.begin() + from + 1, tracks_.begin() + to + 1);
    else
        std::rotate(tracks_.begin() + to, tracks_.begin() + from, tracks_.begin() + from + 1);

    bumpRevision();
    emit trackMoved(from, to);
    emit structureChanged();
}

Track* Timeline::trackAt(int index) const
{
    if (index < 0 || index >= int(tracks_.size()))
        return nullptr;
    return tracks_[size_t(index)].get();
}

Track* Timeline::trackById(const QUuid& id) const
{
    for (const auto& t : tracks_)
        if (t->id() == id)
            return t.get();
    return nullptr;
}

int Timeline::indexOfTrack(const Track* t) const
{
    for (size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i].get() == t)
            return int(i);
    return -1;
}

int Timeline::indexOfTrack(const QUuid& id) const
{
    for (size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i]->id() == id)
            return int(i);
    return -1;
}

std::vector<Track*> Timeline::tracksOfType(TrackType type) const
{
    std::vector<Track*> out;
    for (const auto& t : tracks_)
        if (t->type() == type)
            out.push_back(t.get());
    return out;
}

// ===========================================================================
//  クリップ横断操作
// ===========================================================================

std::shared_ptr<Clip> Timeline::findClip(const QUuid& clipId, Track** ownerOut) const
{
    for (const auto& t : tracks_) {
        for (const auto& c : t->clips()) {
            if (c->id() == clipId) {
                if (ownerOut)
                    *ownerOut = t.get();
                return c;
            }
        }
    }
    if (ownerOut)
        *ownerOut = nullptr;
    return nullptr;
}

int64_t Timeline::duration() const
{
    int64_t maxEnd = 0;
    for (const auto& t : tracks_)
        maxEnd = std::max(maxEnd, t->contentDuration());
    return maxEnd;
}

std::vector<int64_t> Timeline::snapCandidates(const TimeRange& visibleRange,
                                              const std::vector<int>& visibleTrackIndices) const
{
    std::vector<int64_t> candidates;
    for (int idx : visibleTrackIndices) {
        const Track* t = trackAt(idx);
        if (!t)
            continue;
        for (const auto& c : t->clips()) {
            if (visibleRange.contains(c->range().start))
                candidates.push_back(c->range().start);
            if (visibleRange.contains(c->range().end()))
                candidates.push_back(c->range().end());
        }
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return candidates;
}

// ===========================================================================
//  AI プレースホルダ
// ===========================================================================

void Timeline::insertPlaceholder(int trackIndex, const std::shared_ptr<Clip>& placeholder)
{
    Track* t = trackAt(trackIndex);
    if (!t || !placeholder)
        return;
    if (t->insertClip(placeholder)) {
        bumpRevision();
        emit clipInserted(t->id(), placeholder->id());
        emit structureChanged();
        notifyClipCountChanged();
    }
}

std::shared_ptr<Clip> Timeline::takePlaceholder(const QUuid& taskId)
{
    for (const auto& t : tracks_) {
        for (const auto& c : t->clips()) {
            if (c->type() == ClipType::AiPlaceholder
                && static_cast<const AiPlaceholderClip*>(c.get())->taskId() == taskId) {
                auto out = t->takeClip(c->id());
                if (out) {
                    bumpRevision();
                    emit clipRemoved(t->id(), c->id());
                    emit structureChanged();
                    notifyClipCountChanged();
                }
                return out;
            }
        }
    }
    return nullptr;
}

void Timeline::restorePlaceholder(const std::shared_ptr<Clip>& placeholder)
{
    // 元のトラックが分からない場合は最後のトラックへ戻す。
    // Undo コマンド側で元トラックを記録している場合はそちらを使う。
    insertPlaceholder(trackCount() - 1, placeholder);
}

void Timeline::updatePlaceholderProgress(const QUuid& taskId, double progress)
{
    for (const auto& t : tracks_) {
        for (const auto& c : t->clips()) {
            if (c->type() == ClipType::AiPlaceholder
                && static_cast<AiPlaceholderClip*>(c.get())->taskId() == taskId) {
                static_cast<AiPlaceholderClip*>(c.get())->setProgress(progress);
                emit clipChanged(t->id(), c->id());
                return;
            }
        }
    }
}

// ===========================================================================
//  レンダリング
// ===========================================================================

void Timeline::setTimebase(const Rational& tb)
{
    if (tb.den <= 0 || tb.num <= 0)
        return;
    timebase_ = tb;
    bumpRevision();
    emit structureChanged();
}

void Timeline::setCanvasSize(const QSize& s)
{
    if (s.width() <= 0 || s.height() <= 0)
        return;
    canvasSize_ = s;
    bumpRevision();
}

RenderSnapshot Timeline::buildSnapshot(int64_t frame) const
{
    RenderSnapshot snap;
    snap.frameIndex       = frame;
    snap.timebase         = timebase_;
    snap.canvasSize       = canvasSize_;
    snap.timelineRevision = revision_;
    snap.layers.reserve(tracks_.size());

    int z = 0;
    for (const auto& track : tracks_) {          ///< index 0 = 最背面
        const bool isAudio = track->type() == TrackType::Audio;
        if (!track->isVisible() || isAudio) { ++z; continue; }

        // --- トランジション区間なら前後 2 クリップを対で出す (3.10) ---
        // クリップ自体は重なっていないので、Track の不変条件 (2) は破っていない。
        // 対になる 2 レイヤーは同じ zIndex を持ち、合成側が 1 枚に潰す。
        if (const Transition* tr = track->transitionAt(frame)) {
            const auto from = track->clipById(tr->fromClipId);
            const auto to   = track->clipById(tr->toClipId);
            const float progress = tr->progressAt(frame);
            const int   mode      = transitionShaderMode(tr->transitionId);
            const auto  trParams  = resolveTransitionParams(*tr);
            const auto  trColor   = resolveTransitionColor(*tr);

            const auto pushSide = [&](const std::shared_ptr<Clip>& clip, bool incoming) {
                if (!clip || !clip->isEnabled())
                    return;
                // 区間外のフレームはハンドル (ソースの残り) から取る。
                // makeLayerItem はクリップ範囲外のフレームでも変換だけを行う。
                LayerItem item = clip->makeLayerItem(frame, z, *track);
                item.opacity  *= float(track->opacity());
                item.filters   = clip->resolvedFilters();
                item.transition = TransitionRef{ tr->transitionId, progress, incoming,
                                                 mode, trParams, trColor };
                snap.layers.push_back(std::move(item));
            };
            pushSide(from, false);
            pushSide(to,   true);
            ++z;
            continue;
        }

        auto clip = track->clipAt(frame);
        if (!clip || !clip->isEnabled()) { ++z; continue; }

        LayerItem item = clip->makeLayerItem(frame, z, *track);
        item.opacity   *= float(track->opacity());
        item.blendMode  = track->blendMode() != BlendMode::Normal
                              ? track->blendMode() : item.blendMode;
        item.filters    = clip->resolvedFilters();
        snap.layers.push_back(std::move(item));
        ++z;
    }
    return snap;
}

} // namespace yave
