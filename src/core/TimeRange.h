#pragma once

#include <cstdint>

namespace yave {

/// 半開区間 [start, start + duration) をフレーム単位で表す。
/// end() は「含まれない最初のフレーム」。
///
/// 半開区間で統一する理由: 閉区間だと隣接クリップの境界が 1 フレーム重なり、
/// 「連結したはずのクリップが 1 フレーム被って点滅する」不具合を生む。
/// UI 表示上の Out 点は end() - 1 として表示する。
struct TimeRange
{
    int64_t start    = 0;
    int64_t duration = 0;

    constexpr int64_t end() const { return start + duration; }
    constexpr bool isEmpty() const { return duration <= 0; }
    constexpr bool contains(int64_t f) const { return f >= start && f < end(); }
    constexpr bool intersects(const TimeRange& o) const
    { return start < o.end() && o.start < end(); }
    constexpr bool containsRange(const TimeRange& o) const
    { return o.start >= start && o.end() <= end(); }

    constexpr TimeRange intersected(const TimeRange& o) const
    {
        const int64_t s = start > o.start ? start : o.start;
        const int64_t e = end() < o.end() ? end() : o.end();
        return e > s ? TimeRange{s, e - s} : TimeRange{s, 0};
    }

    constexpr TimeRange translated(int64_t delta) const
    { return TimeRange{start + delta, duration}; }

    constexpr bool operator==(const TimeRange& o) const
    { return start == o.start && duration == o.duration; }
    constexpr bool operator!=(const TimeRange& o) const { return !(*this == o); }
};

} // namespace yave
