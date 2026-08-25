#pragma once

#include "AudioClock.h"
#include "AudioRenderGraph.h"
#include "IAudioDevice.h"
#include "MeterBridge.h"

#include "../core/Rational.h"
#include "../core/TimeRange.h"

#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace yave {

class Timeline;
class Project;

} // namespace yave

namespace yave::audio {

/// オーディオ出力の中核。マスタークロックの供給元。
///
/// スレッド安全性:
///   - rtCallback() は OS のリアルタイムスレッドから呼ばれる。
///     malloc / lock / Qt シグナル / 例外 は一切使わない。
///   - それ以外の public メソッドは UI スレッドから呼ぶ。
class AudioRenderEngine : public QObject
{
    Q_OBJECT
public:
    static AudioRenderEngine& instance();

    // ================= デバイス =================

    bool openDevice(const QString& deviceId = {},   ///< 空なら既定デバイス
                    int sampleRate = 48000,
                    int bufferFrames = 512,
                    QString* errorOut = nullptr);
    void closeDevice();
    bool isDeviceOpen() const { return device_ != nullptr; }

    int sampleRate()   const;
    int bufferFrames() const;

    /// デバイス出力レイテンシ + プラグイン合計レイテンシ
    int64_t totalLatencySamples() const;

    // ================= 再生制御 =================

    /// 再生を開始する。startFrame はタイムラインのフレーム番号。
    void play(int64_t startFrame);
    void pause();
    void stop();
    /// 再生中のシーク。RT スレッドを止めずに位置を変える。
    void seek(int64_t frame);

    bool isPlaying() const { return playing_.load(std::memory_order_relaxed); }

    /// 現在「耳に届いている」フレーム位置。映像はこれに追従する。
    int64_t currentFrame() const;

    void setTimebase(const Rational& tb) { timebase_ = tb; }

    /// ループ再生
    void setLoopRange(const TimeRange& r, bool enabled);

    // ================= グラフ =================

    /// Timeline から AudioRenderGraph を構築して差し替える。
    /// Timeline::structureChanged / トラックのゲイン変更などで呼ばれる。
    /// 再生を止めずに差し替えられる (RCU)。
    void rebuildGraph(const Timeline& timeline, const Project& project);

    /// プラグインのレイテンシ変更通知を受けたときに立てるフラグ。
    void requestGraphRebuild() { rebuildRequested_.store(true, std::memory_order_relaxed); }
    bool isGraphRebuildRequested() const
    { return rebuildRequested_.load(std::memory_order_relaxed); }

    // ================= PDC =================

    void setPdcEnabled(bool on) { pdcEnabled_ = on; }
    bool isPdcEnabled() const { return pdcEnabled_; }

    // ================= オフライン (書き出し用。雛形) =================

    using OfflineSink = std::function<void(const float* const*, int)>;

    // ================= メーター =================

    /// 直近のピークレベル。UI スレッドから 30Hz 程度で呼ぶ。
    std::vector<float> masterPeaks();

signals:
    void deviceOpened();
    void deviceClosed();
    void deviceError(const QString& message);
    void playbackStateChanged(bool playing);
    void latencyChanged(int64_t totalSamples);

private:
    explicit AudioRenderEngine(QObject* parent = nullptr);
    ~AudioRenderEngine() override;

    // RT コールバックから使うミックスヘルパ。
    // 引数はすべて事前確保済みバッファ / 値のみ。RT 内で確保は行わない。
    static void mixClipsInto(TrackNode& t, int64_t blockStart, int numFrames,
                             float* const* buf, int channels) noexcept;
    static void processEffect(yave::IAudioEffectNode* fx, float* const* buf,
                              int channels, int frames) noexcept;
    static void accumulateWithGainPan(float* const* src, float* const* dst,
                                      int channels, int frames,
                                      float gain, float pan) noexcept;
    static void applyGain(float* const* buf, int channels, int frames,
                          float gain) noexcept;

    /// リアルタイムコールバック。static にして this を userData で渡す。
    ///
    /// このスコープ内で禁止されていること:
    ///   malloc/free/new/delete、mutex の取得、Qt シグナルの発火、
    ///   ファイル I/O、例外の送出。
    static void rtCallback(float* const* out, int numChannels, int numFrames,
                           void* userData) noexcept;

    void publishGraph(std::unique_ptr<AudioRenderGraph> g);
    void collectRetiredGraphs();

    /// ミックス用スクラッチバッファへのポインタ配列を返す。
    /// 事前確保分を超える要求には nullptr を返す (RT 内で確保しない)。
    float* const* trackScratch(int channels, int frames) noexcept;

    std::unique_ptr<IAudioDevice>   device_;
    AudioClock                      clock_;
    MeterBridge                     meterBridge_;
    Rational                        timebase_{1001, 60000};

    std::atomic<AudioRenderGraph*>  activeGraph_{nullptr};
    std::vector<std::unique_ptr<AudioRenderGraph>> liveGraphs_;
    struct RetiredGraph
    {
        std::unique_ptr<AudioRenderGraph> g;
        uint64_t retiredAtGeneration;
    };
    std::vector<RetiredGraph>       retired_;      ///< UI スレッドのみ触る
    mutable QMutex                  retiredMutex_;
    std::atomic<uint64_t>           rtGeneration_{0};

    std::atomic<bool>               playing_{false};
    std::atomic<bool>               rebuildRequested_{false};
    bool                            pdcEnabled_ = true;
    int64_t                         pluginLatencySamples_ = 0;

    // ミックス用スクラッチバッファ (open 時に事前確保)
    std::vector<std::vector<float>> scratch_;
    std::vector<float*>             scratchPtrs_;

    TimeRange                       loopRange_{0, 0};
    bool                            loopEnabled_ = false;
};

} // namespace yave::audio
