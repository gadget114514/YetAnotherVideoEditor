#pragma once

#include "../core/Rational.h"

#include <atomic>
#include <cstdint>

namespace yave::audio {

/// サンプル単位のアトミック再生位置。マスタークロックの供給元。
///
/// オーディオファースト同期 (5.1):
///   RT スレッドが advance() で進め、任意スレッドが audibleSamplePosition()
///   で読む。映像はこの位置に追従する。
class AudioClock
{
public:
    /// RT スレッドからのみ呼ぶ
    void advance(int64_t sampleFrames) noexcept
    { played_.fetch_add(sampleFrames, std::memory_order_release); }

    void reset(int64_t startSample) noexcept
    { played_.store(startSample, std::memory_order_release); }

    /// デバイスへ書き込んだ生のサンプル数
    int64_t rawPlayedPosition() const noexcept
    { return played_.load(std::memory_order_acquire); }

    /// 任意スレッドから呼べる。「実際に耳に届いている」位置を返す。
    ///
    /// playedSamples_ は「デバイスに書き込んだ量」であり、実際にスピーカーから
    /// 出るのはバッファ分だけ後。outputLatency を引かないと、映像が音より
    /// バッファ長 (数十ms) 分だけ先行して見える。
    int64_t audibleSamplePosition() const noexcept
    {
        return played_.load(std::memory_order_acquire)
             - outputLatencySamples_.load(std::memory_order_relaxed);
    }

    void setOutputLatencySamples(int64_t n) noexcept
    {
        outputLatencySamples_.store(n < 0 ? 0 : n, std::memory_order_relaxed);
    }
    int64_t outputLatencySamples() const noexcept
    { return outputLatencySamples_.load(std::memory_order_relaxed); }

    int  sampleRate() const noexcept { return sampleRate_; }
    void setSampleRate(int sr) noexcept { sampleRate_ = sr; }

    /// サンプル位置 -> タイムラインフレーム番号
    int64_t sampleToFrame(int64_t sample, const Rational& tb) const noexcept
    { return yave::secondsToFrames(double(sample) / double(sampleRate_), tb,
                                   yave::RoundMode::Nearest); }

private:
    alignas(64) std::atomic<int64_t> played_{0};
    std::atomic<int64_t>             outputLatencySamples_{0};
    int                              sampleRate_ = 48000;
};

} // namespace yave::audio
