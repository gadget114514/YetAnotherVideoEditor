#include "Track.h"
#include "IAudioEffectNode.h"

#include <QObject>
#include <QtGlobal>

#include <algorithm>

namespace yave {

Track::Track(TrackType type) : id_(QUuid::createUuid()), type_(type) {}
Track::~Track() = default;

// ===========================================================================
//  検索ヘルパ
// ===========================================================================

std::vector<std::shared_ptr<Clip>>::iterator Track::lowerBoundFor(int64_t start)
{
    return std::lower_bound(clips_.begin(), clips_.end(), start,
        [](const std::shared_ptr<Clip>& c, int64_t s) { return c->range().start < s; });
}

std::vector<std::shared_ptr<Clip>>::const_iterator Track::lowerBoundFor(int64_t start) const
{
    return std::lower_bound(clips_.cbegin(), clips_.cend(), start,
        [](const std::shared_ptr<Clip>& c, int64_t s) { return c->range().start < s; });
}

// ===========================================================================
//  クリップ操作
// ===========================================================================

bool Track::acceptsClip(const Clip& c) const
{
    switch (type_) {
    case TrackType::Video:
        // Title は映像トラックにも置ける (3.11)
        return c.type() == ClipType::Video || c.type() == ClipType::Image
            || c.type() == ClipType::Color || c.type() == ClipType::AiPlaceholder
            || c.type() == ClipType::Title;
    case TrackType::Audio:
        return c.type() == ClipType::Audio || c.type() == ClipType::AiPlaceholder;
    case TrackType::Subtitle:
        return c.type() == ClipType::Subtitle || c.type() == ClipType::Title;
    case TrackType::AiGenerated:
        return true;   // 生成物の種別に応じて映像 / 音声どちらでも受け入れる
    }
    return false;
}

bool Track::insertClip(const std::shared_ptr<Clip>& c)
{
    if (!c || c->range().isEmpty())
        return false;

    const TimeRange r = c->range();

    // 重なりチェック: 前後のクリップとの交差を調べる
    auto it = lowerBoundFor(r.start);
    if (it != clips_.end() && (*it)->range().start < r.end())
        return false;
    if (it != clips_.begin()) {
        auto prev = std::prev(it);
        if ((*prev)->range().end() > r.start)
            return false;
    }

    clips_.insert(it, c);
    assertInvariants();
    return true;
}

void Track::overwriteClip(const std::shared_ptr<Clip>& c)
{
    if (!c || c->range().isEmpty())
        return;

    removeClipsIn(c->range());
    // 部分的に掛かるクリップをトリムする
    for (auto& existing : clips_) {
        const TimeRange er = existing->range();
        if (!er.intersects(c->range()))
            continue;

        TimeRange nr = er;
        if (er.start < c->range().start && er.end() > c->range().end()) {
            // 中央を削る -> 前半のみ残す (後半は失われる。無限レイヤー設計上許容)
            nr.duration = c->range().start - er.start;
        } else if (er.start < c->range().start) {
            nr.duration = c->range().start - er.start;
        } else if (er.end() > c->range().end()) {
            // 前方を削る -> ソース内オフセットも進める
            const int64_t cut = c->range().end() - er.start;
            nr.start    += cut;
            nr.duration -= cut;
            existing->shiftSourceOffset(-cut);
            if (nr.duration <= 0) continue;
        } else {
            continue;   // 完全包含 -> removeClipsIn 済みのはず
        }
        if (nr.isEmpty())
            continue;
        existing->setRange(nr);
    }
    clips_.erase(std::remove_if(clips_.begin(), clips_.end(),
                                [](const std::shared_ptr<Clip>& p) { return p->range().isEmpty(); }),
                 clips_.end());

    insertClip(c);
}

std::shared_ptr<Clip> Track::takeClip(const QUuid& clipId)
{
    for (auto it = clips_.begin(); it != clips_.end(); ++it) {
        if ((*it)->id() == clipId) {
            std::shared_ptr<Clip> out = *it;
            clips_.erase(it);
            dropOrphanTransitions();
            return out;
        }
    }
    return nullptr;
}

bool Track::removeClip(const QUuid& clipId)
{
    for (auto it = clips_.begin(); it != clips_.end(); ++it) {
        if ((*it)->id() == clipId) {
            clips_.erase(it);
            dropOrphanTransitions();
            return true;
        }
    }
    return false;
}

std::vector<std::shared_ptr<Clip>> Track::removeClipsIn(const TimeRange& r)
{
    std::vector<std::shared_ptr<Clip>> removed;
    auto it = lowerBoundFor(r.start);
    if (it != clips_.begin()) --it;   // 1 つ前が範囲に掛かる可能性

    while (it != clips_.end() && (*it)->range().start < r.end()) {
        if (r.containsRange((*it)->range())) {
            removed.push_back(*it);
            it = clips_.erase(it);
        } else {
            ++it;
        }
    }
    if (!removed.empty())
        dropOrphanTransitions();
    return removed;
}

void Track::shiftClipsAfter(int64_t startFrame, int64_t delta)
{
    bool changed = false;
    for (auto& c : clips_) {
        if (c->range().start >= startFrame) {
            c->setRange(c->range().translated(delta));
            changed = true;
        }
    }
    if (changed) {
        // 動いたクリップに付いている境界も一緒に動かす。
        for (auto& t : transitions_) {
            if (t.centerFrame >= startFrame)
                t.centerFrame += delta;
        }
        resort();
        dropOrphanTransitions();
    }
}

// ===========================================================================
//  トランジション (3.10)
// ===========================================================================

namespace {

/// クリップの out 点より後ろに残っているソースの尺 (フレーム)。
/// 尺が無制限のソース (画像 / カラー / タイトル) は制限なしとして扱う。
constexpr int64_t kUnlimitedHandle = 1 << 30;

int64_t handleAfter(const Clip& c)
{
    const int64_t maxDur = c.maxDuration();
    if (maxDur < 0)
        return kUnlimitedHandle;
    const int64_t used = c.sourceOffset() + c.range().duration;
    return maxDur > used ? maxDur - used : 0;
}

/// クリップの in 点より前に残っているソースの尺 (フレーム)。
int64_t handleBefore(const Clip& c)
{
    return c.maxDuration() < 0 ? kUnlimitedHandle : c.sourceOffset();
}

} // namespace

std::shared_ptr<Clip> Track::clipEndingAt(int64_t boundaryFrame) const
{
    for (const auto& c : clips_) {
        if (c->range().end() == boundaryFrame)
            return c;
    }
    return nullptr;
}

std::shared_ptr<Clip> Track::clipStartingAt(int64_t boundaryFrame) const
{
    for (const auto& c : clips_) {
        if (c->range().start == boundaryFrame)
            return c;
    }
    return nullptr;
}

const Transition* Track::transitionAt(int64_t frame) const
{
    for (const auto& t : transitions_) {
        if (t.contains(frame))
            return &t;
    }
    return nullptr;
}

const Transition* Track::transitionAtBoundary(int64_t boundaryFrame) const
{
    for (const auto& t : transitions_) {
        if (t.centerFrame == boundaryFrame)
            return &t;
    }
    return nullptr;
}

int64_t Track::maxTransitionDuration(int64_t boundaryFrame) const
{
    const auto from = clipEndingAt(boundaryFrame);
    const auto to   = clipStartingAt(boundaryFrame);
    if (!from && !to)
        return 0;

    int64_t half = kUnlimitedHandle;
    if (from)
        half = std::min({ half, handleAfter(*from), from->range().duration });
    if (to)
        half = std::min({ half, handleBefore(*to), to->range().duration });

    return half > 0 ? half * 2 : 0;
}

bool Track::addTransition(Transition t, QString* errorOut)
{
    const auto fail = [errorOut](const QString& msg) {
        if (errorOut)
            *errorOut = msg;
        return false;
    };

    if (t.transitionId.isEmpty())
        return fail(QObject::tr("Transition id is empty."));

    const auto from = clipEndingAt(t.centerFrame);
    const auto to   = clipStartingAt(t.centerFrame);
    if (!from && !to)
        return fail(QObject::tr("There is no clip boundary at this position."));

    const TransitionDesc* desc = findTransitionDesc(t.transitionId);
    if (desc && !desc->allowsMissingPartner && (!from || !to))
        return fail(QObject::tr("This transition needs a clip on both sides."));

    const int64_t maxDur = maxTransitionDuration(t.centerFrame);
    if (maxDur <= 0)
        return fail(QObject::tr("The clips have no spare source frames for a transition."));

    // ハンドルに収まる長さへ縮める (3.10.3)。拒否せず縮めるのは、
    // 「置けたが短い」ほうが「置けない」より編集の流れを止めないため。
    t.durationFrames = std::min(t.durationFrames, maxDur);
    if (t.durationFrames <= 0)
        return fail(QObject::tr("Transition duration must be positive."));

    if (t.id.isNull())
        t.id = QUuid::createUuid();
    t.fromClipId = from ? from->id() : QUuid();
    t.toClipId   = to   ? to->id()   : QUuid();

    // 同じ境界にあるものは置き換える
    const auto it = std::find_if(transitions_.begin(), transitions_.end(),
                                 [&](const Transition& x) { return x.centerFrame == t.centerFrame; });
    if (it != transitions_.end())
        *it = std::move(t);
    else
        transitions_.push_back(std::move(t));
    return true;
}

void Track::removeTransition(const QUuid& id)
{
    transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(),
                                      [&](const Transition& t) { return t.id == id; }),
                       transitions_.end());
}

void Track::dropOrphanTransitions()
{
    transitions_.erase(
        std::remove_if(transitions_.begin(), transitions_.end(),
                       [this](const Transition& t) {
                           const auto from = clipEndingAt(t.centerFrame);
                           const auto to   = clipStartingAt(t.centerFrame);
                           if (!from && !to)
                               return true;
                           if (!t.fromClipId.isNull() && (!from || from->id() != t.fromClipId))
                               return true;
                           if (!t.toClipId.isNull() && (!to || to->id() != t.toClipId))
                               return true;
                           return t.durationFrames <= 0;
                       }),
        transitions_.end());
}

void Track::setTransitions(std::vector<Transition> t)
{
    transitions_ = std::move(t);
    dropOrphanTransitions();
    for (auto& tr : transitions_) {
        const int64_t maxDur = maxTransitionDuration(tr.centerFrame);
        tr.durationFrames = std::min(tr.durationFrames, maxDur);
    }
    dropOrphanTransitions();
}

std::shared_ptr<Clip> Track::clipAt(int64_t frame) const
{
    // start > frame となる最初の要素を探し、その 1 つ前が候補
    auto it = std::upper_bound(clips_.cbegin(), clips_.cend(), frame,
        [](int64_t f, const std::shared_ptr<Clip>& c) { return f < c->range().start; });
    if (it == clips_.cbegin())
        return nullptr;
    --it;
    return (*it)->range().contains(frame) ? *it : nullptr;
}

std::shared_ptr<Clip> Track::clipById(const QUuid& id) const
{
    for (const auto& c : clips_)
        if (c->id() == id)
            return c;
    return nullptr;
}

std::vector<std::shared_ptr<Clip>> Track::clipsIn(const TimeRange& r) const
{
    std::vector<std::shared_ptr<Clip>> out;
    auto it = std::upper_bound(clips_.cbegin(), clips_.cend(), r.start,
        [](int64_t f, const std::shared_ptr<Clip>& c) { return f < c->range().start; });
    if (it != clips_.cbegin()) --it;                 // 1 つ前が範囲に掛かる可能性
    for (; it != clips_.cend() && (*it)->range().start < r.end(); ++it) {
        if ((*it)->range().intersects(r))
            out.push_back(*it);
    }
    return out;
}

int64_t Track::nextClipStart(int64_t frame) const
{
    auto it = std::lower_bound(clips_.cbegin(), clips_.cend(), frame,
        [](const std::shared_ptr<Clip>& c, int64_t f) { return c->range().start < f; });
    if (it != clips_.end() && (*it)->range().start >= frame)
        return (*it)->range().start;
    return -1;
}

int64_t Track::prevClipEnd(int64_t frame) const
{
    auto it = std::upper_bound(clips_.cbegin(), clips_.cend(), frame - 1,
        [](int64_t f, const std::shared_ptr<Clip>& c) { return f < c->range().start; });
    if (it != clips_.cbegin()) {
        --it;
        return (*it)->range().end();
    }
    return -1;
}

int64_t Track::findFreeStart(int64_t fromFrame, int64_t duration) const
{
    int64_t cursor = fromFrame > 0 ? fromFrame : 0;
    for (const auto& c : clips_) {
        if (c->range().end() <= cursor)
            continue;
        if (c->range().start >= cursor + duration)
            break;
        cursor = c->range().end();
    }
    return cursor;
}

int64_t Track::contentDuration() const
{
    if (clips_.empty())
        return 0;
    return clips_.back()->range().end();
}

void Track::resort()
{
    std::stable_sort(clips_.begin(), clips_.end(),
        [](const std::shared_ptr<Clip>& a, const std::shared_ptr<Clip>& b) {
            return a->range().start < b->range().start;
        });
}

// ===========================================================================
//  エフェクトチェーン
// ===========================================================================

void Track::addEffect(IAudioEffectNode* fx)
{
    if (fx)
        effectChain_.push_back(fx);
}

void Track::removeEffect(int index)
{
    if (index < 0 || index >= int(effectChain_.size()))
        return;
    effectChain_.erase(effectChain_.begin() + index);
}

void Track::moveEffect(int from, int to)
{
    if (from == to || from < 0 || from >= int(effectChain_.size()))
        return;
    to = qBound(0, to, int(effectChain_.size()) - 1);
    auto node = effectChain_.at(size_t(from));
    effectChain_.erase(effectChain_.begin() + from);
    effectChain_.insert(effectChain_.begin() + to, node);
}

int64_t Track::totalLatencySamples() const
{
    int64_t total = 0;
    for (const IAudioEffectNode* fx : effectChain_)
        if (fx && fx->isEnabled())
            total += fx->latencySamples();
    return total;
}

// ===========================================================================
//  デバッグ
// ===========================================================================

void Track::assertInvariants() const
{
#ifndef QT_NO_DEBUG
    for (size_t i = 0; i < clips_.size(); ++i) {
        Q_ASSERT_X(!clips_[i]->range().isEmpty(), "Track::assertInvariants",
                   "clip duration must be positive");
        if (i > 0) {
            Q_ASSERT_X(clips_[i - 1]->range().start <= clips_[i]->range().start,
                       "Track::assertInvariants", "clips must be sorted by start");
            Q_ASSERT_X(!clips_[i - 1]->range().intersects(clips_[i]->range()),
                       "Track::assertInvariants", "clips must not overlap");
        }
    }
#else
    Q_UNUSED(this);
#endif
}

} // namespace yave
