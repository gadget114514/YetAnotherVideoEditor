#pragma once

#include "LockFreeRingBuffer.h"

#include <vector>

namespace yave::audio {

/// RT -> UI のレベル通知。
///
/// RT スレッドは pushPeaks() でブロックごとのピークを積み、
/// UI スレッドは drainPeaks() を 30Hz 程度で呼んで表示する。
class MeterBridge
{
public:
    struct PeakSample
    {
        float left  = 0.0f;
        float right = 0.0f;
        int64_t samplePosition = 0;
    };

    explicit MeterBridge(size_t capacity = 256)
        : queue_(capacity) {}

    /// RT スレッドから呼ぶ (ロックフリー)。満杯時は最古の値を捨てるのではなく drop。
    void pushPeaks(float* const* out, int numChannels, int numFrames) noexcept
    {
        PeakSample p;
        p.left = peakOf(out, numChannels > 0 ? 0 : -1, numFrames);
        p.right = numChannels > 1
                      ? peakOf(out, 1, numFrames)
                      : p.left;
        // 満杯なら何もしない (UI が追いついていないだけなので古い値でよい)
        queue_.push(p);
    }

    /// UI スレッドから呼ぶ。最新のピーク値を取り出す。
    bool popPeak(PeakSample& out) noexcept { return queue_.pop(out); }

private:
    static float peakOf(float* const* channels, int ch, int n) noexcept
    {
        if (!channels || ch < 0 || !channels[ch])
            return 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i) {
            const float v = channels[ch][i] < 0 ? -channels[ch][i] : channels[ch][i];
            if (v > peak)
                peak = v;
        }
        return peak;
    }

    LockFreeRingBuffer<PeakSample> queue_;
};

} // namespace yave::audio
