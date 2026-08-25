#include "DelayCompensator.h"
#include "AudioRenderGraph.h"

#include "../core/IAudioEffectNode.h"

#include <algorithm>

namespace yave::audio {

// ===========================================================================
//  DelayCompensator
// ===========================================================================

int64_t DelayCompensator::computeForChains(const std::vector<int64_t>& chainLatencies,
                                           int64_t masterLatency,
                                           std::vector<int64_t>* compensationDelaysOut)
{
    int64_t maxLatency = 0;
    for (int64_t l : chainLatencies)
        maxLatency = std::max(maxLatency, l);

    if (compensationDelaysOut) {
        compensationDelaysOut->clear();
        compensationDelaysOut->reserve(chainLatencies.size());
        for (int64_t l : chainLatencies)
            compensationDelaysOut->push_back(maxLatency - l);
    }

    return maxLatency + masterLatency;
}

int64_t DelayCompensator::compute(AudioRenderGraph& graph)
{
    std::vector<int64_t> latencies;
    latencies.reserve(graph.tracks.size());

    for (TrackNode& t : graph.tracks) {
        int64_t total = 0;
        for (const IAudioEffectNode* fx : t.effectChain)
            if (fx && fx->isEnabled())
                total += fx->latencySamples();
        t.chainLatencySamples = total;
        latencies.push_back(total);
    }

    int64_t masterTotal = 0;
    for (const IAudioEffectNode* fx : graph.masterChain)
        if (fx && fx->isEnabled())
            masterTotal += fx->latencySamples();
    graph.masterLatency = masterTotal;

    const int64_t total = computeForChains(latencies, masterTotal, nullptr);
    for (size_t i = 0; i < graph.tracks.size(); ++i)
        graph.tracks[i].compensationDelay =
            *std::max_element(latencies.begin(), latencies.end()) - latencies[i];

    return total;
}

// ===========================================================================
//  DelayLine
// ===========================================================================

void DelayLine::prepare(int channels, int64_t maxDelaySamples)
{
    // 容量は 2 の冪に切り上げる (ビットマスクで剰余を回避)
    int64_t cap = 1;
    while (cap < std::max<int64_t>(1, maxDelaySamples))
        cap <<= 1;
    capacity_ = cap;
    buffers_.assign(size_t(std::max(1, channels)), std::vector<float>(size_t(capacity_), 0.0f));
    writePos_ = 0;
}

void DelayLine::setDelay(int64_t samples) noexcept
{
    delay_ = std::min(samples > 0 ? samples : 0, capacity_);
}

void DelayLine::reset() noexcept
{
    for (auto& b : buffers_)
        std::fill(b.begin(), b.end(), 0.0f);
    writePos_ = 0;
}

void DelayLine::process(float* const* buf, int channels, int numFrames) noexcept
{
    if (delay_ <= 0 || buffers_.empty() || capacity_ <= 0)
        return;

    const int chCount = std::min(channels, int(buffers_.size()));
    const int64_t cap = capacity_;

    for (int c = 0; c < chCount; ++c) {
        float* ring   = buffers_[size_t(c)].data();
        float* line   = buf[c];
        // 単純なリング読み出し。delay_ <= capacity を前提とする。
        for (int i = 0; i < numFrames; ++i) {
            const float delayed = ring[(writePos_ + i - delay_) & (cap - 1)];
            ring[(writePos_ + i) & (cap - 1)] = line[i];
            line[i] = delayed;
        }
    }
    writePos_ = (writePos_ + numFrames) & (cap - 1);
}

} // namespace yave::audio
