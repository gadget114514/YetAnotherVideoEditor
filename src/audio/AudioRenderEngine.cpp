#include "AudioRenderEngine.h"
#include "AudioRenderGraph.h"
#include "DelayCompensator.h"

#include "../core/IAudioEffectNode.h"

#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace yave::audio {

AudioRenderEngine& AudioRenderEngine::instance()
{
    static AudioRenderEngine s;
    return s;
}

AudioRenderEngine::AudioRenderEngine(QObject* parent) : QObject(parent) {}

AudioRenderEngine::~AudioRenderEngine()
{
    closeDevice();
    // 残ったグラフを破棄 (RT スレッドは既に停止している前提)
    activeGraph_.store(nullptr, std::memory_order_release);
    liveGraphs_.clear();
    retired_.clear();
}

// ===========================================================================
//  デバイス
// ===========================================================================

bool AudioRenderEngine::openDevice(const QString& deviceId, int sampleRate,
                                   int bufferFrames, QString* errorOut)
{
    closeDevice();

    device_ = IAudioDevice::create();
    if (!device_) {
        if (errorOut)
            *errorOut = QStringLiteral("No audio device implementation available.");
        return false;
    }

    if (!device_->open(deviceId, sampleRate, bufferFrames, &rtCallback, this, errorOut)) {
        device_.reset();
        return false;
    }
    device_->start();

    clock_.setSampleRate(device_->sampleRate());
    clock_.setOutputLatencySamples(device_->outputLatencySamples());

    emit deviceOpened();
    return true;
}

void AudioRenderEngine::closeDevice()
{
    stop();
    if (device_) {
        device_->stop();
        device_->close();
        device_.reset();
        emit deviceClosed();
    }
}

int AudioRenderEngine::sampleRate() const
{
    return device_ ? device_->sampleRate() : 48000;
}

int AudioRenderEngine::bufferFrames() const
{
    return device_ ? device_->bufferFrames() : 512;
}

int64_t AudioRenderEngine::totalLatencySamples() const
{
    const int64_t deviceLatency = device_ ? device_->outputLatencySamples() : 0;
    return deviceLatency + pluginLatencySamples_;
}

// ===========================================================================
//  再生制御
// ===========================================================================

void AudioRenderEngine::play(int64_t startFrame)
{
    const int64_t startSample =
        int64_t(double(startFrame) / timebase_.toDouble() * double(sampleRate()));
    clock_.reset(startSample);
    playing_.store(true, std::memory_order_relaxed);
    emit playbackStateChanged(true);
}

void AudioRenderEngine::pause()
{
    playing_.store(false, std::memory_order_relaxed);
    emit playbackStateChanged(false);
}

void AudioRenderEngine::stop()
{
    pause();
    clock_.reset(0);
}

void AudioRenderEngine::seek(int64_t frame)
{
    const int64_t sample =
        int64_t(double(frame) / timebase_.toDouble() * double(sampleRate()));
    clock_.reset(sample);
}

int64_t AudioRenderEngine::currentFrame() const
{
    // audibleSamplePosition() は「デバイスへ書き込んだサンプル数」-「出力レイテンシ」
    // を返す。これを引かないと、映像がバッファ長分だけ音より先行して見える。
    // PDC 分のレイテンシも setOutputLatencySamples に加算済み。
    const int64_t sample = clock_.audibleSamplePosition();
    return yave::secondsToFrames(double(sample) / double(clock_.sampleRate()),
                                 timebase_, yave::RoundMode::Nearest);
}

void AudioRenderEngine::setLoopRange(const TimeRange& r, bool enabled)
{
    // グラフに書き込むのは rebuildGraph 経由。ここでは要求だけ保持する。
    loopRange_      = r;
    loopEnabled_    = enabled && !r.isEmpty();
    if (auto* g = activeGraph_.load(std::memory_order_acquire)) {
        g->loopEnabled     = loopEnabled_;
        g->loopStartSample = int64_t(double(r.start) / timebase_.toDouble()
                                     * double(sampleRate()));
        g->loopEndSample   = int64_t(double(r.end()) / timebase_.toDouble()
                                     * double(sampleRate()));
    }
}

// ===========================================================================
//  グラフ管理
// ===========================================================================

void AudioRenderEngine::rebuildGraph(const Timeline& timeline, const Project& project)
{
    auto graph = AudioRenderGraphBuilder::build(timeline, project);
    // PDC を計算して反映
    const int64_t pluginLatency =
        pdcEnabled_ ? DelayCompensator::compute(*graph) : 0;
    pluginLatencySamples_ = pluginLatency;
    clock_.setOutputLatencySamples(
        (device_ ? device_->outputLatencySamples() : 0) + pluginLatency);

    graph->maxBlockFrames = std::max(64, bufferFrames());
    publishGraph(std::move(graph));
    rebuildRequested_.store(false, std::memory_order_relaxed);
    emit latencyChanged(totalLatencySamples());
}

void AudioRenderEngine::publishGraph(std::unique_ptr<AudioRenderGraph> g)
{
    // UI スレッドから呼ぶ。RT スレッドを止めない。

    AudioRenderGraph* raw = g.get();
    liveGraphs_.push_back(std::move(g));

    // アトミックに差し替える
    AudioRenderGraph* old = activeGraph_.exchange(raw, std::memory_order_acq_rel);

    // 古いグラフを「引退リスト」へ入れる。すぐには破棄しない。
    // exchange した瞬間に RT スレッドが古いグラフを処理中の可能性がある。
    if (old) {
        auto it = std::find_if(liveGraphs_.begin(), liveGraphs_.end(),
                               [old](const auto& p) { return p.get() == old; });
        if (it != liveGraphs_.end()) {
            QMutexLocker lock(&retiredMutex_);
            retired_.push_back({std::move(*it),
                                rtGeneration_.load(std::memory_order_acquire)});
            liveGraphs_.erase(it);
        }
    }

    collectRetiredGraphs();
}

void AudioRenderEngine::collectRetiredGraphs()
{
    // RT が 2 世代以上進んだグラフだけを実際に破棄する
    const uint64_t now = rtGeneration_.load(std::memory_order_acquire);
    QMutexLocker lock(&retiredMutex_);
    retired_.erase(std::remove_if(retired_.begin(), retired_.end(),
                                  [now](const RetiredGraph& r) {
                                      return now >= r.retiredAtGeneration + 2;
                                  }),
                   retired_.end());
}

std::vector<float> AudioRenderEngine::masterPeaks()
{
    MeterBridge::PeakSample p;
    std::vector<float> out;
    while (meterBridge_.popPeak(p)) {
        out.push_back(p.left);
        out.push_back(p.right);
    }
    return out;
}

// ===========================================================================
//  RT コールバック
// ===========================================================================

void AudioRenderEngine::rtCallback(float* const* out, int numChannels,
                                   int numFrames, void* userData) noexcept
{
    // ===================================================================
    //  このスコープ内で禁止されていること:
    //    malloc/free/new/delete、mutex、Qt シグナル、ファイル I/O、例外
    //  違反すると priority inversion で音が途切れる。
    // ===================================================================

    auto* self = static_cast<AudioRenderEngine*>(userData);

    // グラフは RCU で差し替えられる。acquire で読めば、UI スレッドが
    // publishGraph() で書いた内容が確実に見える。
    AudioRenderGraph* g = self->activeGraph_.load(std::memory_order_acquire);

    for (int c = 0; c < numChannels; ++c)
        std::memset(out[c], 0, sizeof(float) * size_t(numFrames));

    if (!g || !self->playing_.load(std::memory_order_relaxed)) {
        // 停止中でも世代は進める。UI 側の遅延解放判定に使われる。
        self->rtGeneration_.fetch_add(1, std::memory_order_release);
        return;
    }

    // このブロックが担当するタイムライン上のサンプル位置
    const int64_t blockStart = self->clock_.rawPlayedPosition();

    for (TrackNode& t : g->tracks) {
        if (t.muted || (g->anySolo && !t.solo))
            continue;

        float* const* buf = self->trackScratch(numChannels, numFrames);
        for (int c = 0; c < numChannels; ++c)
            std::memset(buf[c], 0, sizeof(float) * size_t(numFrames));

        // (a) クリップの PCM をミックス
        mixClipsInto(t, blockStart, numFrames, buf, numChannels);

        // (b) VST3 エフェクトチェーン (in-place)
        for (yave::IAudioEffectNode* fx : t.effectChain)
            if (fx && fx->isEnabled())
                processEffect(fx, buf, numChannels, numFrames);

        // (c) PDC の遅延を適用
        if (t.compensationDelay > 0 && t.delayLine)
            t.delayLine->process(buf, numChannels, numFrames);

        // (d) ゲイン / パンを掛けてマスターへ加算
        accumulateWithGainPan(buf, out, numChannels, numFrames, t.gain, t.pan);
    }

    applyGain(out, numChannels, numFrames, g->masterGain);

    // メーター値を UI へ (ロックフリーリングバッファ)
    self->meterBridge_.pushPeaks(out, numChannels, numFrames);

    // ===================================================================
    //  クロック更新。release 順序で書くことで、他スレッドが acquire で
    //  読んだときにここまでの処理結果が見えることを保証する。
    // ===================================================================
    self->clock_.advance(numFrames);
    self->rtGeneration_.fetch_add(1, std::memory_order_release);
}

float* const* AudioRenderEngine::trackScratch(int channels, int frames) noexcept
{
    // open 時に確保済み。足りない場合は無音トラックとして扱う (RT 内で確保しない)。
    if (int(scratch_.size()) < channels || int(scratch_[0].size()) < frames)
        return nullptr;

    scratchPtrs_.resize(size_t(channels));
    for (int c = 0; c < channels; ++c)
        scratchPtrs_[size_t(c)] = scratch_[size_t(c)].data();
    return scratchPtrs_.data();
}

void AudioRenderEngine::mixClipsInto(TrackNode&, int64_t, int, float* const*, int) noexcept
{
    // PCM データは事前デコード済みバッファ (ClipSource::preloadedData) から
    // コピーする。現行ビルドではデコーダ未接続のため無音のまま返す。
}

void AudioRenderEngine::processEffect(yave::IAudioEffectNode*, float* const*, int, int) noexcept
{
    // Vst3ProcessorNode::processRt への委譲ポイント。
    // VST3 ホストが有効なビルドでのみ実処理が入る。
}

void AudioRenderEngine::accumulateWithGainPan(float* const* src, float* const* dst,
                                              int channels, int frames,
                                              float gain, float pan) noexcept
{
    // 等価パワー・パン: cos/sin カーブ
    const float angle = (pan + 1.0f) * 0.5f * 1.5707963267948966f;
    const float lGain = gain * std::cos(angle);
    const float rGain = gain * std::sin(angle);

    if (channels >= 2) {
        for (int i = 0; i < frames; ++i) {
            dst[0][i] += src[0][i] * lGain;
            dst[1][i] += src[1][i] * rGain;
        }
    } else if (channels == 1) {
        for (int i = 0; i < frames; ++i)
            dst[0][i] += src[0][i] * gain;
    }
}

void AudioRenderEngine::applyGain(float* const* buf, int channels, int frames,
                                  float gain) noexcept
{
    for (int c = 0; c < channels; ++c) {
        float* line = buf[c];
        for (int i = 0; i < frames; ++i)
            line[i] *= gain;
    }
}

} // namespace yave::audio
