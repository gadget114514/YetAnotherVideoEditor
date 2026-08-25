#include "Rational.h"

#include <cmath>
#include <limits>

namespace yave {

int64_t secondsToFrames(double seconds, const Rational& tb, RoundMode mode)
{
    if (tb.den == 0 || tb.num == 0)
        return 0;
    // frames = seconds * (tb.den / tb.num)
    const double raw = seconds * double(tb.den) / double(tb.num);
    switch (mode) {
    case RoundMode::Floor:   return int64_t(std::floor(raw));
    case RoundMode::Nearest: return int64_t(raw >= 0 ? raw + 0.5 : raw - 0.5);
    case RoundMode::Ceil:    return int64_t(std::ceil(raw));
    }
    return int64_t(raw);
}

double framesToSeconds(int64_t frames, const Rational& tb)
{
    if (tb.den == 0 || tb.num == 0)
        return 0.0;
    return double(frames) * double(tb.num) / double(tb.den);
}

int64_t rescaleFrames(int64_t frames, const Rational& from, const Rational& to, RoundMode mode)
{
    // out = frames * from/to = frames * from.num * to.den / (from.den * to.num)
    if (from.den == 0 || to.num == 0)
        return 0;

#if defined(__SIZEOF_INT128__)
    __int128 n = __int128(frames) * __int128(from.num) * __int128(to.den);
    __int128 d = __int128(from.den) * __int128(to.num);
    if (d == 0) return 0;
    __int128 q = n / d;
    const __int128 rem = n % d;
    switch (mode) {
    case RoundMode::Floor:   break;
    case RoundMode::Nearest: {
        const __int128 twiceRem2 = (rem < 0 ? -rem : rem) * 2;
        if (twiceRem2 >= (d < 0 ? -d : d))
            q += (n < 0 ? -1 : 1);
        break;
    }
    case RoundMode::Ceil:
        if (rem != 0 && ((d > 0 && rem > 0) || (d < 0 && rem < 0)))
            q += 1;
        break;
    }
    if (q > (__int128)std::numeric_limits<int64_t>::max())
        return std::numeric_limits<int64_t>::max();
    if (q < (__int128)std::numeric_limits<int64_t>::min())
        return std::numeric_limits<int64_t>::min();
    return int64_t(q);
#else
    const double raw = double(frames) * double(from.num) * double(to.den)
                       / (double(from.den) * double(to.num));
    switch (mode) {
    case RoundMode::Floor:   return int64_t(std::floor(raw));
    case RoundMode::Nearest: return int64_t(raw >= 0 ? raw + 0.5 : raw - 0.5);
    case RoundMode::Ceil:    return int64_t(std::ceil(raw));
    }
    return int64_t(raw);
#endif
}

} // namespace yave
