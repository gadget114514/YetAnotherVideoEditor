# 5. オーディオエンジン

[← 目次に戻る](../design.md)

---

## 5.1 オーディオファースト同期

### 5.1.1 基本原則

**オーディオデバイスのコールバックがマスタークロックである。**
映像はそれに追従する。逆(映像タイマー駆動で音を合わせる)は採らない。

> **理由**: 人間は音の途切れ / ピッチ変動を映像のコマ落ちより遥かに敏感に検知する。
> また、オーディオデバイスのクロックは OS のタイマーとは独立した水晶発振器で動いており、
> `QTimer` で 16.67ms を刻んでも必ずドリフトする。
> 音を基準にすれば、映像は「今の音の位置に一番近いフレームを出す」だけでよい。

### 5.1.2 クロックの流れ

```
 [Audio RT Thread]                         [UI Thread]
   デバイスコールバック
   (256 frames @ 48kHz)
        |
   AudioRenderGraph を評価して PCM を書き込む
        |
   playedSamples_ += 256   (std::atomic<int64_t>, relaxed)
   outputTimestamp_ = デバイス報告のホスト時刻 (atomic)
        |
        +--------------------------------> PlaybackController::currentFrame()
                                              |
                                              |  presentedSamples = playedSamples_
                                              |  latency = デバイス出力レイテンシ
                                              |  audibleSample = presentedSamples - latency
                                              |  seconds = audibleSample / sampleRate
                                              v
                                           frameIndex = secondsToFrames(seconds, tb, Nearest)
                                              |
                                              v
                                           Timeline::buildSnapshot(frameIndex)
```

```cpp
// src/audio/AudioClock.h
namespace yave::audio {

class AudioClock
{
public:
    /// RT スレッドからのみ呼ぶ
    void advance(int64_t sampleFrames) noexcept
    { played_.fetch_add(sampleFrames, std::memory_order_release); }

    void reset(int64_t startSample) noexcept
    { played_.store(startSample, std::memory_order_release); }

    /// 任意スレッドから呼べる。実際に耳に届いている位置を返す。
    int64_t audibleSamplePosition() const noexcept
    {
        const int64_t played = played_.load(std::memory_order_acquire);
        return played - outputLatencySamples_.load(std::memory_order_relaxed);
    }

    void setOutputLatencySamples(int64_t n) noexcept
    { outputLatencySamples_.store(n, std::memory_order_relaxed); }

    int sampleRate() const noexcept { return sampleRate_; }

private:
    std::atomic<int64_t> played_{0};
    std::atomic<int64_t> outputLatencySamples_{0};
    int sampleRate_ = 48000;
};

} // namespace yave::audio
```

> **`outputLatency` を引く理由**: `playedSamples_` は「デバイスに書き込んだ量」であり、
> 実際にスピーカーから出るのはバッファ分だけ後。これを補正しないと映像が音より
> バッファ長 (数十ms) 分だけ先行する。WASAPI は `IAudioClock::GetPosition()`、
> CoreAudio は `AudioTimeStamp` でより正確な値が取れるので、可能ならそちらを使う。

### 5.1.3 映像側の追従

`PreviewItem` は `QQuickItem::update()` を毎 vsync で呼び、その都度
`AudioClock` から現在フレームを取り直す。

- 音より映像が **進んでいる** → そのフレームをそのまま表示 (何もしない)
- 音より映像が **1 フレーム以上遅れている** → 中間フレームをスキップして最新へジャンプ
- デコードが間に合わない → 直前フレームをホールド (4.6 参照)

音声側でフレームを間引いたり補間したりは**しない**。

## 5.2 デバイス抽象

```cpp
// src/audio/IAudioDevice.h
namespace yave::audio {

struct AudioDeviceInfo
{
    QString id;
    QString displayName;
    int     maxOutputChannels = 2;
    std::vector<int> supportedSampleRates;
    bool    isDefault = false;
};

/// RT コールバック。この関数の中で許されるのは:
///   - 事前確保済みバッファへの読み書き
///   - アトミック操作
///   - VST3 プラグインの process() 呼び出し
/// 禁止: malloc/free, mutex, Qt シグナル, ファイル I/O, 例外
using AudioCallback = void(*)(float* const* outputChannels,
                             int numChannels,
                             int numFrames,
                             void* userData) noexcept;

class IAudioDevice
{
public:
    virtual ~IAudioDevice() = default;

    static std::vector<AudioDeviceInfo> enumerate();
    static std::unique_ptr<IAudioDevice> create();   // プラットフォームごとの実装を返す

    virtual bool open(const QString& deviceId, int sampleRate, int bufferFrames,
                      AudioCallback cb, void* userData, QString* errorOut) = 0;
    virtual void close() = 0;
    virtual bool start() = 0;
    virtual void stop()  = 0;

    virtual int     sampleRate()    const = 0;
    virtual int     bufferFrames()  const = 0;
    virtual int64_t outputLatencySamples() const = 0;
};

} // namespace yave::audio
```

| プラットフォーム | 実装 | 備考 |
|---|---|---|
| Windows | `WasapiDevice` (共有モード既定 / 排他モード選択可) | `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` でイベント駆動。スレッドは `AvSetMmThreadCharacteristics("Pro Audio")` で昇格 |
| macOS | `CoreAudioDevice` (`AudioUnit` kAudioUnitSubType_HALOutput) | `kAudioDevicePropertyBufferFrameSize` でバッファ長設定 |

> **`QAudioSink` を使わない理由**: Qt Multimedia のオーディオ出力は
> バッファ長の細かい制御ができず、実測で 50〜100ms のレイテンシになる。
> VST3 のリアルタイム処理には不足。

## 5.3 AudioRenderGraph

RT スレッドが読む唯一のデータ構造。`Timeline` とは完全に分離された POD グラフ。

```cpp
// src/audio/AudioRenderGraph.h
namespace yave::audio {

struct ClipSource            // 再生すべき音声クリップ 1 個分
{
    int64_t timelineStart = 0;    // サンプル単位
    int64_t timelineEnd   = 0;
    int64_t sourceOffset  = 0;    // ソース内の開始サンプル
    float   gain          = 1.0f;
    float   pan           = 0.0f;
    const float* const* preloadedData = nullptr;  // 事前デコード済み PCM (チャンネル配列)
    int64_t preloadedFrames = 0;
    int     channels        = 2;
    // フェード
    int64_t fadeInFrames  = 0;
    int64_t fadeOutFrames = 0;
};

struct TrackNode
{
    std::vector<ClipSource> clips;      // timelineStart 昇順
    float  gain  = 1.0f;
    float  pan   = 0.0f;
    bool   muted = false;
    bool   solo  = false;

    std::vector<Vst3ProcessorNode*> effectChain;   // 所有はしない
    int64_t chainLatencySamples = 0;               // PDC 用の合計レイテンシ
    int64_t compensationDelay   = 0;               // このトラックに追加すべき遅延
    DelayLine* delayLine = nullptr;                // compensationDelay 用リングバッファ
};

struct AudioRenderGraph
{
    int sampleRate    = 48000;
    int maxBlockFrames= 512;
    std::vector<TrackNode> tracks;
    float masterGain  = 1.0f;
    std::vector<Vst3ProcessorNode*> masterChain;
    int64_t masterLatency = 0;
    bool    anySolo = false;
};

} // namespace yave::audio
```

### 5.3.1 グラフの差し替え (RCU)

`Timeline` が編集されると、UI スレッドが新しい `AudioRenderGraph` を構築して
アトミックにポインタを差し替える。RT スレッドはロックを取らない。

```cpp
class AudioRenderEngine
{
    std::atomic<AudioRenderGraph*> activeGraph_{nullptr};
    std::vector<std::unique_ptr<AudioRenderGraph>> retired_;   // 遅延解放用
    std::atomic<uint64_t> rtGeneration_{0};

public:
    /// UI スレッドから呼ぶ
    void publishGraph(std::unique_ptr<AudioRenderGraph> g)
    {
        AudioRenderGraph* old = activeGraph_.exchange(g.get(), std::memory_order_acq_rel);
        graphs_.push_back(std::move(g));
        if (old)
            retired_.push_back(/* old を所有していた unique_ptr を移す */);
        collectRetired();     // RT が 2 世代進んだものだけ実際に破棄
    }
};
```

**遅延解放が必須**: `exchange` した瞬間に RT スレッドが古いグラフを処理中の可能性がある。
RT 側は毎コールバックで `rtGeneration_` をインクリメントし、UI 側は
「差し替え時の世代 + 2 以上」になったグラフのみ破棄する。

### 5.3.2 音声データの事前デコード

RT スレッド内でデコードはできない (malloc とディスク I/O が発生する)。
そのため、**再生範囲の音声はあらかじめデコードしてメモリに載せる**。

- 48kHz / stereo / float32 = 384KB/秒。10 分のプロジェクトで約 230MB。
- 長尺プロジェクトでは全体は載らないため、再生ヘッド前後 ±30 秒をストリーミングで
  先読みする `AudioStreamCache` を使う。ページ単位 (5 秒) でロードし、
  RT スレッドはロード済みページのみを読む。未ロードページは無音を出す
  (ここで待つと音が途切れるため)。

## 5.4 レンダリングコールバック

```cpp
// src/audio/AudioRenderEngine.cpp
void AudioRenderEngine::rtCallback(float* const* out, int numCh, int numFrames, void* user) noexcept
{
    auto* self = static_cast<AudioRenderEngine*>(user);
    AudioRenderGraph* g = self->activeGraph_.load(std::memory_order_acquire);

    // 出力をクリア
    for (int c = 0; c < numCh; ++c)
        std::memset(out[c], 0, sizeof(float) * numFrames);

    if (!g || !self->playing_.load(std::memory_order_relaxed)) {
        self->rtGeneration_.fetch_add(1, std::memory_order_release);
        return;
    }

    const int64_t startSample = self->clock_.rawPlayedPosition();

    for (TrackNode& t : g->tracks) {
        if (t.muted || (g->anySolo && !t.solo))
            continue;

        float* const* trackBuf = self->scratch_.trackBuffer(&t);   // 事前確保済み
        clearBuffer(trackBuf, numCh, numFrames);

        // (1) クリップをミックス
        mixClips(t, startSample, numFrames, trackBuf, numCh);

        // (2) VST3 エフェクトチェーンを通す
        for (Vst3ProcessorNode* fx : t.effectChain)
            fx->processRt(trackBuf, numCh, numFrames);

        // (3) PDC 遅延を適用
        if (t.compensationDelay > 0)
            t.delayLine->process(trackBuf, numCh, numFrames);

        // (4) ゲイン / パン を適用してマスターへ加算
        applyGainPanAndAccumulate(trackBuf, out, numCh, numFrames, t.gain, t.pan);
    }

    // (5) マスターチェーン
    for (Vst3ProcessorNode* fx : g->masterChain)
        fx->processRt(out, numCh, numFrames);

    applyGain(out, numCh, numFrames, g->masterGain);

    // (6) メーター値を UI へ (ロックフリー)
    self->meterBridge_.pushPeaks(out, numCh, numFrames);

    // (7) クロック更新
    self->clock_.advance(numFrames);
    self->rtGeneration_.fetch_add(1, std::memory_order_release);
}
```

`scratch_` は `open()` 時に「最大トラック数 × 最大チャンネル数 × 最大ブロック長」を
確保しておく。トラックが増えて足りなくなった場合は、UI スレッド側が
`publishGraph()` の前に `scratch_` を拡張する
(RT が古いグラフを見ている間は古い scratch を使うので、拡張は追加確保のみで既存は解放しない)。

## 5.5 プラグイン遅延補正 (PDC)

### 5.5.1 問題

VST3 プラグインの中には、処理のために入力を先読みするもの
(ルックアヘッドリミッター、リニアフェーズ EQ、一部のリバーブ) がある。
これらは `IAudioProcessor::getLatencySamples()` で自身の遅延を申告する。
補正しないと、そのトラックだけ音が遅れて聞こえる。

### 5.5.2 補正アルゴリズム

```
1. 各トラック t について、チェーン内プラグインのレイテンシを合計する
     chainLatency[t] = Σ fx.getLatencySamples()

2. マスターチェーンのレイテンシも算出
     masterLatency = Σ masterFx.getLatencySamples()

3. 全トラック中の最大値を求める
     maxLatency = max(chainLatency[t]) for all t

4. 各トラックに追加すべき遅延を決める
     compensationDelay[t] = maxLatency - chainLatency[t]

5. 映像との同期のため、全体を maxLatency + masterLatency だけ前倒しする。
   すなわち AudioClock の outputLatency に加算する。
     totalPluginLatency = maxLatency + masterLatency
```

```cpp
// src/audio/DelayCompensator.h
namespace yave::audio {

class DelayCompensator
{
public:
    /// グラフ構築時に UI スレッドから呼ぶ。graph の compensationDelay を書き込む。
    static int64_t compute(AudioRenderGraph& graph);
    // 戻り値 = totalPluginLatency (AudioClock に反映すべき値)
};

/// 単純な遅延リングバッファ。RT セーフ。
class DelayLine
{
public:
    void prepare(int channels, int64_t maxDelaySamples);  // 非 RT
    void setDelay(int64_t samples) noexcept;              // RT 可 (maxDelay 以下)
    void process(float* const* buf, int channels, int numFrames) noexcept;
    void reset() noexcept;
private:
    std::vector<std::vector<float>> buffers_;
    int64_t writePos_ = 0;
    int64_t delay_    = 0;
    int64_t capacity_ = 0;
};

} // namespace yave::audio
```

### 5.5.3 レイテンシ変更への対応

プラグインは動作中にレイテンシを変えることがある
(例: リニアフェーズ ⇄ ミニマムフェーズの切替)。
VST3 では `IComponentHandler::restartComponent(kLatencyChanged)` で通知される。

```cpp
tresult PLUGIN_API Vst3ComponentHandler::restartComponent(int32 flags)
{
    if (flags & Steinberg::Vst::kLatencyChanged) {
        // RT スレッドから呼ばれる可能性があるため、直接グラフを触らない。
        // フラグを立てて UI スレッドで再構築させる。
        host_->requestGraphRebuild();       // atomic flag を立てるだけ
    }
    if (flags & Steinberg::Vst::kParamValuesChanged) {
        host_->markParametersDirty();
    }
    return Steinberg::kResultOk;
}
```

UI スレッドは次のイベントループで `AudioRenderGraph` を作り直して `publishGraph()` する。
**再構築中に音が途切れないよう、古いグラフは差し替え完了まで動き続ける。**

### 5.5.4 PDC の副作用

`maxLatency` が大きい (例: 4096 サンプル = 85ms) と、再生開始からの反応が鈍くなる。
そのため以下を提供する:

- **PDC 無効モード**: 編集中は PDC を切って低レイテンシを優先し、書き出し時のみ有効にする。
  DAW では一般的な機能。UI にトグルを置く。
- レイテンシの大きいプラグインは、トラックヘッダに遅延量をバッジ表示する。

## 5.6 オフラインレンダリング (書き出し時)

書き出しでは RT 制約が不要になるため、同じ `AudioRenderGraph` を
非リアルタイムで回す。

```cpp
void AudioRenderEngine::renderOffline(const TimeRange& range,
                                      int blockFrames,
                                      const std::function<void(const float* const*, int)>& sink)
{
    // 1. 全音声を事前デコード (メモリに載らない場合はストリーミング)
    // 2. rtCallback と同じ処理を、デバイスコールバックの代わりにループで回す
    // 3. PDC は必ず有効
    // 4. VST3 は setProcessing(true) 済みで、offline モード
    //    (ProcessSetup::processMode = kOffline) を指定して初期化し直す
}
```

> **`kOffline` モードで初期化し直す理由**: 一部のプラグインは
> オフラインモードでより高品質なアルゴリズムに切り替える。
> また、リアルタイムモードのままだとプラグインが内部でスレッドを使い、
> 決定的な出力にならないことがある。

## 5.7 波形表示

タイムライン上のクリップ波形は UI スレッドをブロックせずに生成する。

- 素材取り込み時にバックグラウンドでピークファイルを生成し、
  `.yave_cache/waveform/<assetId>.peaks` に保存。
- フォーマット: ミップマップ構造。`[level0: 256 サンプルごとの min/max]`,
  `[level1: 1024 サンプルごと]`, ... と複数解像度を持つ。
  ズームレベルに応じて適切な level を読む。
- 生成中はプレースホルダ(灰色の帯)を表示する。

```cpp
struct PeakFileHeader {
    char     magic[4];        // "YWPK"
    uint32_t version;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t levelCount;
    uint64_t totalFrames;
    // 続いて level ごとに { uint32_t samplesPerPeak; uint64_t offset; uint64_t count; }
};
```
