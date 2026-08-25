#include "../src/audio/AudioRenderEngine.h"
#include "../src/audio/DelayCompensator.h"

#include <QtTest/QtTest>

using namespace yave;
using namespace yave::audio;

class TestDelayCompensator : public QObject
{
    Q_OBJECT

private slots:
    void computeBasic();
    void zeroLatency();
    void masterChainOnly();
    void delayLineRoundTrip();
    void delayLinePowerOfTwoCapacity();
};

void TestDelayCompensator::computeBasic()
{
    // 3 トラック: レイテンシ 0 / 512 / 128
    const std::vector<int64_t> chains = {0, 512, 128};
    std::vector<int64_t> compensation;
    const int64_t total = DelayCompensator::computeForChains(chains, 0, &compensation);

    // 最大レイテンシのトラックは補正 0、他は差分
    QCOMPARE(total, int64_t(512));
    QCOMPARE(int(compensation.size()), 3);
    QCOMPARE(compensation[0], int64_t(512));
    QCOMPARE(compensation[1], int64_t(0));
    QCOMPARE(compensation[2], int64_t(384));
}

void TestDelayCompensator::zeroLatency()
{
    std::vector<int64_t> compensation;
    const int64_t total =
        DelayCompensator::computeForChains({0, 0}, 0, &compensation);
    QCOMPARE(total, int64_t(0));
    QCOMPARE(compensation[0], int64_t(0));

    // マスターチェーンのレイテンシも加算される
    const int64_t total2 =
        DelayCompensator::computeForChains({0, 0}, 256, nullptr);
    QCOMPARE(total2, int64_t(256));
}

void TestDelayCompensator::masterChainOnly()
{
    std::vector<int64_t> compensation;
    // 全トラックがレイテンシ 0、マスターのみ 100
    const int64_t total =
        DelayCompensator::computeForChains({0, 0, 0}, 100, &compensation);
    QCOMPARE(total, int64_t(100));
    for (int64_t c : compensation)
        QCOMPARE(c, int64_t(0));   ///< 補正は不要 (マスター分はクロック側で吸収)
}

void TestDelayCompensator::delayLineRoundTrip()
{
    // 遅延 4 サンプル: 入力パルスが 4 サンプル遅れて出てくること。
    DelayLine line;
    line.prepare(2, 8);          ///< 容量 8 (2 の冪へ切り上げ)
    line.setDelay(4);

    float left[16] = {};
    float right[16] = {};
    left[0] = 1.0f;   ///< インパルス

    float* buf[2] = {left, right};
    line.process(buf, 2, 16);

    QCOMPARE(left[4], 1.0f);
    QCOMPARE(left[0], 0.0f);
    QCOMPARE(right[4], 0.0f);   ///< 右チャンネルは無音入力

    // 継続処理でもリングが破綻しないこと
    line.process(buf, 2, 16);
    QVERIFY(std::isfinite(left[15]));
}

void TestDelayCompensator::delayLinePowerOfTwoCapacity()
{
    // 容量は 2 の冪へ切り上げられる (ビットマスクで剰余回避するため)。
    DelayLine line;
    line.prepare(1, 10);         ///< 要求 10 -> 実容量 16
    line.setDelay(10);

    float in[32];
    float* buf[1] = {in};
    for (int i = 0; i < 32; ++i)
        in[i] = float(i);

    line.process(buf, 1, 32);

    // 10 サンプル遅延されていること
    for (int i = 10; i < 32; ++i)
        QCOMPARE(in[i], float(i - 10));
    for (int i = 0; i < 10; ++i)
        QCOMPARE(in[i], 0.0f);
}

QTEST_MAIN(TestDelayCompensator)
#include "tst_delaycompensator.moc"
