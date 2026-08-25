#pragma once

#include <cstdint>
#include <vector>

namespace yave::audio {

struct AudioRenderGraph;

/// プラグイン遅延補正 (PDC, 5.5)。
///
/// アルゴリズム:
///   1. 各トラック t のチェーン内プラグインのレイテンシを合計
///        chainLatency[t] = Σ fx.latencySamples()
///   2. マスターチェーンのレイテンシも算出
///        masterLatency = Σ masterFx.latencySamples()
///   3. 全トラック中の最大値を求める
///        maxLatency = max(chainLatency[t])
///   4. 各トラックに追加すべき遅延を決める
///        compensationDelay[t] = maxLatency - chainLatency[t]
///   5. 全体を maxLatency + masterLatency だけ前倒しする値を返す
///      (AudioClock::setOutputLatencySamples へ加算)
class DelayCompensator
{
public:
    /// グラフ構築時に UI スレッドから呼ぶ。graph の compensationDelay を書き込む。
    /// 戻り値 = totalPluginLatency (AudioClock に反映すべき値)
    static int64_t compute(AudioRenderGraph& graph);

    /// グラフなしで計算だけ行うユーティリティ (テスト用)。
    /// chainLatencies[i] を入力とし、compensationDelay[i] と総レイテンシを出す。
    static int64_t computeForChains(const std::vector<int64_t>& chainLatencies,
                                    int64_t masterLatency,
                                    std::vector<int64_t>* compensationDelaysOut);
};

/// 単純な遅延リングバッファ。RT セーフ。
class DelayLine
{
public:
    void prepare(int channels, int64_t maxDelaySamples);   ///< 非 RT (事前確保)
    void setDelay(int64_t samples) noexcept;               ///< RT 可 (maxDelay 以下)
    int64_t delay() const noexcept { return delay_; }

    /// buf の内容を delay_ サンプルだけ遅らせる (in-place)。
    void process(float* const* buf, int channels, int numFrames) noexcept;
    void reset() noexcept;

private:
    std::vector<std::vector<float>> buffers_;   ///< [channel][ring]
    int64_t writePos_ = 0;
    int64_t delay_    = 0;
    int64_t capacity_ = 0;
};

} // namespace yave::audio
