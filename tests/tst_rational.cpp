#include "../src/core/Rational.h"

#include <QtTest/QtTest>

using namespace yave;

class TestRational : public QObject
{
    Q_OBJECT

private slots:
    void reduced();
    void comparison();
    void overflowComparison();
    void arithmetic();
    void secondsToFrames();
    void framesToSeconds();
    void rescaleFrames();
    void dropFrameTimebase();

private:
    static constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
};

void TestRational::reduced()
{
    const auto r = Rational::reduced(60000, 1001);
    QCOMPARE(r.num, 60000);
    QCOMPARE(r.den, 1001);

    // 約分
    const auto g = Rational::reduced(1920, 1080);
    QCOMPARE(g.num, 16);
    QCOMPARE(g.den, 9);

    // 負の分母は正規化される
    const auto neg = Rational(1, -2);
    QCOMPARE(neg.num, -1);
    QCOMPARE(neg.den, 2);

    // ゼロ除算の保護 (den=0 は 1 にフォールバック)
    const auto z = Rational(5, 0);
    QCOMPARE(z.den, 1);
}

void TestRational::comparison()
{
    QVERIFY(Rational(1, 3) == Rational(2, 6));
    QVERIFY(Rational(1, 3) != Rational(1, 2));
    QVERIFY(Rational(1, 3) < Rational(1, 2));
    QVERIFY(Rational(1, 2) > Rational(1, 3));
    QVERIFY(Rational(-1, 2) < Rational(0, 1));
    QVERIFY(Rational(1, 2) <= Rational(1, 2));
}

void TestRational::overflowComparison()
{
    // 交差乗算が int64 を溢れるケース。__int128 / 分割で正しく比較できること。
    constexpr int64_t kMaxI64 = std::numeric_limits<int64_t>::max();
    const Rational a{kMaxI64, kMaxI64 / 2};
    const Rational b{kMaxI64 - 2, kMaxI64 / 2};
    QVERIFY(a != b);
    QVERIFY(a > b);

    // 極端な値でもクラッシュ / 誤判定しない
    const Rational big{kMaxI64 - 1, kMaxI64};
    QVERIFY(big < Rational(1, 1));

    const Rational x{9223372036854775807LL, 1};
    const Rational y{1, 9223372036854775807LL};
    QVERIFY(x > y);
}

void TestRational::arithmetic()
{
    // フレーム長どうしの加減算
    const Rational f30{1, 30};
    const Rational f60{1, 60};

    const auto sum = f30 + f60;
    QCOMPARE(sum.toDouble(), 1.0 / 20.0);

    const auto diff = f30 - f60;
    QCOMPARE(diff.toDouble(), 1.0 / 60.0);

    // 1 フレーム長 × fps の逆数 = 1 秒
    const Rational frameLen{1001, 60000};
    const auto seconds = frameLen * Rational(60000, 1001);
    QCOMPARE(seconds.num, 1);
    QCOMPARE(seconds.den, 1);
}

void TestRational::secondsToFrames()
{
    const Rational tb{1001, 60000};   // 59.94fps

    // ちょうどの値
    QCOMPARE(yave::secondsToFrames(1001.0 / 60000.0 * 10.0, tb, RoundMode::Nearest), 10);

    // Floor / Ceil / Nearest の丸め差
    const double awkward = 10.5 / 59.94;   ///< 10.5 フレーム分の秒数
    QCOMPARE(int64_t(std::floor(awkward * double(tb.den) / double(tb.num))),
             yave::secondsToFrames(awkward, tb, RoundMode::Floor));
}

void TestRational::framesToSeconds()
{
    const Rational tb{1001, 60000};
    QCOMPARE(yave::framesToSeconds(0, tb), 0.0);
    QCOMPARE(yave::framesToSeconds(60000, tb), 1001.0);
}

void TestRational::rescaleFrames()
{
    // from / to は「1 フレームの長さ (duration)」を表す。
    // 30fps の 30 フレーム (= 1 秒) を 59.94fps タイムベースへ変換すると 59.94 フレーム
    const int64_t r =
        yave::rescaleFrames(30, {1, 30}, timebase::Fps59_94, RoundMode::Nearest);
    QVERIFY(qAbs(double(r) - 59.94) < 1.0);

    // 同一タイムベースなら恒等
    QCOMPARE(yave::rescaleFrames(12345, timebase::Fps59_94, timebase::Fps59_94,
                                 RoundMode::Nearest),
             12345);

    // Ceil は必ず以上、Floor は以下
    const int64_t f = yave::rescaleFrames(7, timebase::Fps24, timebase::Fps25, RoundMode::Floor);
    const int64_t c = yave::rescaleFrames(7, timebase::Fps24, timebase::Fps25, RoundMode::Ceil);
    QVERIFY(f <= c);
}

void TestRational::dropFrameTimebase()
{
    // 23.976 / 29.97 / 59.94 の往復変換でズレが出ないこと (設計方針 3.1)
    for (const Rational tb : {timebase::Fps23_976, timebase::Fps29_97,
                              timebase::Fps59_94}) {
        for (int64_t frames : {1, 10, 100, 1001, 60000}) {
            const double sec = yave::framesToSeconds(frames, tb);
            const int64_t back = yave::secondsToFrames(sec, tb, RoundMode::Nearest);
            QCOMPARE(back, frames);
        }
    }
}

QTEST_MAIN(TestRational)
#include "tst_rational.moc"
