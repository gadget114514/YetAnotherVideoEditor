#pragma once

#include <QtGlobal>

#include <cstdint>
#include <numeric>

namespace yave {

/// 有理数。すべてのタイムベース表現に使う。double 秒での保持は禁止。
///
/// 不変条件:
///   - den > 0 (reduced() / 正規化パスで保証)
///   - 比較演算は交差乗算だが、__int128 (GCC/Clang) もしくは分割乗算で溢れを防ぐ
struct Rational
{
    int64_t num = 0;
    int64_t den = 1;

    constexpr Rational() = default;
    constexpr Rational(int64_t n, int64_t d) : num(n), den(d ? d : 1)
    {
        // den < 0 を正規化 (constexpr 内では例外を投げない)
        if (den < 0) { num = -num; den = -den; }
    }

    /// 約分して生成
    static constexpr Rational reduced(int64_t n, int64_t d)
    {
        if (d == 0) return {n, 1};
        if (d < 0) { n = -n; d = -d; }
        const int64_t g = gcd64(n < 0 ? -n : n, d);
        return g > 0 ? Rational{n / g, d / g} : Rational{n, d};
    }

    constexpr double toDouble() const { return den != 0 ? double(num) / double(den) : 0.0; }
    constexpr bool   isZero()   const { return num == 0; }

    constexpr Rational inverted() const { return {den, num}; }

    Rational operator*(const Rational& o) const
    { return reduced(num * o.num, den * o.den); }

    Rational operator/(const Rational& o) const
    { return reduced(num * o.den, den * o.num); }

    Rational operator+(const Rational& o) const
    { return reduced(num * o.den + o.num * den, den * o.den); }

    Rational operator-(const Rational& o) const
    { return reduced(num * o.den - o.num * den, den * o.den); }

    /// 約分して比較
    bool operator==(const Rational& o) const
    {
        const Rational a = reduced(num, den);
        const Rational b = reduced(o.num, o.den);
        return a.num == b.num && a.den == b.den;
    }
    bool operator!=(const Rational& o) const { return !(*this == o); }

    /// 交差乗算比較。オーバーフローは 128bit 中間値で回避する。
    bool operator<(const Rational& o) const
    {
#if defined(__SIZEOF_INT128__)
        __int128 lhs = __int128(num) * __int128(o.den);
        __int128 rhs = __int128(o.num) * __int128(den);
        return lhs < rhs;
#else
        // フォールバック: 約分してから比較 (十分な精度)
        const Rational a = reduced(num, den);
        const Rational b = reduced(o.num, o.den);
        return double(a.num) * double(b.den) < double(b.num) * double(a.den);
#endif
    }
    bool operator>(const Rational& o) const { return o < *this; }
    bool operator<=(const Rational& o) const { return !(o < *this); }
    bool operator>=(const Rational& o) const { return !(*this < o); }

private:
    static constexpr int64_t gcd64(int64_t a, int64_t b)
    {
        while (b != 0) { const int64_t t = a % b; a = b; b = t; }
        return a;
    }
};

/// よく使うタイムベース
namespace timebase {
inline constexpr Rational Fps23_976{1001, 24000};
inline constexpr Rational Fps24    {1,    24};
inline constexpr Rational Fps25    {1,    25};
inline constexpr Rational Fps29_97 {1001, 30000};
inline constexpr Rational Fps30    {1,    30};
inline constexpr Rational Fps59_94 {1001, 60000};   ///< 既定
inline constexpr Rational Fps60    {1,    60};
} // namespace timebase

/// 秒 -> フレーム番号。丸めモードを明示的に選ばせる(暗黙の切り捨てを禁止)。
enum class RoundMode { Floor, Nearest, Ceil };

int64_t secondsToFrames(double seconds, const Rational& tb, RoundMode mode);
double  framesToSeconds(int64_t frames, const Rational& tb);

/// あるタイムベースのフレーム番号を別のタイムベースへ変換する
int64_t rescaleFrames(int64_t frames, const Rational& from, const Rational& to, RoundMode mode);

} // namespace yave
