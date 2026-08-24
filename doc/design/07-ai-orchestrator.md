# 7. マルチモーダル生成AIエンジン

[← 目次に戻る](../design.md)

---

## 7.1 設計方針

### 7.1.1 プロバイダ抽象

生成モデルの実行基盤は 3 系統あり、`IGenerationProvider` で抽象化して差し替え可能にする。

| プロバイダ | 実装 | 用途 |
|---|---|---|
| `OnnxLocalProvider` | ONNX Runtime C++ API + DirectML / CUDA / CoreML EP | 軽量タスク: STT(Whisper), TTS, マスク生成(SAM/U2Net), 超解像, 軽量 I2V |
| `RemoteHttpProvider` | `QNetworkAccessManager` で HTTP/JSON | 重量タスク: T2V / I2V / V2V。ComfyUI (ローカル LAN 含む)、OpenAI 互換、Replicate |
| `SidecarProvider` | 子プロセス + stdio/名前付きパイプ IPC | Python 製推論スクリプトをローカルで動かす。ローカル完結しつつ大型モデルに対応 |

> **なぜハイブリッドにするか**: T2V/I2V の実用モデル (Wan 2.x, SVD, HunyuanVideo 等) は
> 重み数 GB〜数十 GB、推論に 10〜80GB の VRAM を要する。これを ONNX Runtime 単体で
> ローカル実行することは多くのユーザー環境で不可能。一方 STT / TTS / マスク生成は
> ONNX でローカル実行するのが速く、ネットワークもコストも不要。
> **タスクの重さに応じて実行先を選べる**構成が現実解。

ユーザーには「モデル」単位で選ばせ、その裏でどのプロバイダが使われるかは
`ProviderRegistry` が決める。

### 7.1.2 UI を絶対にブロックしない

生成には数十秒〜数十分かかる。以下を保証する。

- 生成中も編集・再生・別の生成の投入ができる
- 生成区間には**プレースホルダクリップ**が即座に置かれ、進捗が可視化される
- キャンセルできる
- アプリを閉じても再開できる (タスク状態をディスクに保存)

## 7.2 生成パラメータ

```cpp
// src/ai/AiGenerationParams.h
namespace yave::ai {

/// 何を生成するか
enum class GenerationKind
{
    Video,          // 動画
    Audio,          // 音声 (TTS / SFX / BGM)
    Subtitle,       // 字幕 (STT / スクリプト生成)
    Mask,           // マスク画像シーケンス
    EffectMetadata, // エフェクトパラメータ (自動カラーグレード等)
    Image           // 静止画
};

/// 動画生成のサブモード
enum class VideoGenMode
{
    TextToVideo,     // T2V : プロンプトのみ
    ImageToVideo,    // I2V : 参照画像ベース
    VideoToVideo     // V2V : 既存レイヤー映像ベース (スタイル変換 / アニメ化)
};

/// I2V における参照フレームの与え方
enum class I2VReferenceMode
{
    StartFrameOnly,  // 開始時点のみ参照 (そこから動き出す)
    EndFrameOnly,    // 終了時点のみ参照 (そこへ収束する)
    BothEnds         // 両端参照 = 前後キーフレーム間の補間生成
};

/// 音声生成のサブモード
enum class AudioGenMode { Narration, SoundEffect, Bgm, VoiceConversion };

/// 字幕生成のサブモード
enum class SubtitleGenMode
{
    SpeechToText,    // 音声トラックから書き起こし
    ScriptFromPrompt,// プロンプトからスクリプト生成
    Translate        // 既存字幕の翻訳
};

/// 参照画像の指定
struct ImageReference
{
    enum class Source { FilePath, TimelineFrame };
    Source   source = Source::FilePath;

    QString  filePath;          // Source::FilePath のとき (プロジェクト相対)
    QUuid    sourceTrackId;     // Source::TimelineFrame のとき
    int64_t  sourceFrame = 0;   // 〃
    double   strength = 1.0;    // 参照の効き具合 0..1
};

/// V2V のソース指定
struct VideoReference
{
    QUuid    sourceTrackId;
    TimeRange sourceRange;      // 空なら生成区間と同じ
    double   denoiseStrength = 0.6;   // 元映像をどれだけ残すか
};

/// 生成の全パラメータ。これがそのまま JSON に永続化される。
struct AiGenerationParams
{
    // --- 共通 ---
    GenerationKind kind = GenerationKind::Video;
    QUuid          targetTrackId;         // どのトラックに生成するか
    TimeRange      range;                 // In / Out (フレーム単位)
    QString        modelId;               // "wan2.2-i2v-14b" 等。ProviderRegistry が解決
    QString        providerId;            // 明示指定する場合。空なら自動選択
    QString        prompt;
    QString        negativePrompt;
    int64_t        seed = -1;             // -1 = ランダム。再現性のため生成後に確定値を書き戻す
    int            steps = 30;
    double         guidanceScale = 7.5;
    QJsonObject    extraParams;           // モデル固有の追加パラメータ

    // --- Video ---
    VideoGenMode      videoMode = VideoGenMode::TextToVideo;
    I2VReferenceMode  i2vRefMode = I2VReferenceMode::StartFrameOnly;
    std::optional<ImageReference> startReference;
    std::optional<ImageReference> endReference;
    std::optional<VideoReference> videoReference;
    QSize             outputResolution{1280, 720};
    Rational          outputFrameRate = timebase::Fps30;
    bool              loopSeamless = false;

    // --- Audio ---
    AudioGenMode audioMode = AudioGenMode::Narration;
    QString      voiceId;                 // TTS の話者
    double       speakingRate = 1.0;
    double       pitch = 0.0;
    QString      referenceAudioPath;      // ボイスクローン用 (プロジェクト相対)
    int          audioSampleRate = 48000;
    int          audioChannels = 2;

    // --- Subtitle ---
    SubtitleGenMode subtitleMode = SubtitleGenMode::SpeechToText;
    QUuid           sourceAudioTrackId;   // STT の入力トラック
    QString         language;             // "ja" / "en" / "auto"
    QString         targetLanguage;       // 翻訳先
    bool            wordLevelTimestamps = true;
    QString         subtitleStylePresetId;

    // --- Mask ---
    QUuid           maskSourceTrackId;
    QString         maskTargetDescription;  // "person" / "sky" 等のテキスト指定
    std::vector<QPointF> maskHintPoints;    // クリックによるヒント (SAM 用)
    bool            maskTrackAcrossFrames = true;

    // --- 出力の扱い ---
    bool  replaceExistingClips = false;   // 区間内の既存クリップを置き換えるか
    bool  createNewTrack = false;         // 新規トラックを作って置くか

    QJsonObject toJson() const;
    static AiGenerationParams fromJson(const QJsonObject& o);

    /// 妥当性検証。UI の「生成」ボタンの有効/無効判定に使う。
    struct ValidationResult { bool ok; QString errorKey; };
    ValidationResult validate() const;
};

} // namespace yave::ai
```

### 7.2.1 検証ルール (`validate()`)

| 条件 | エラー |
|---|---|
| `range.duration <= 0` | `error.ai.emptyRange` |
| `kind == Video && videoMode == ImageToVideo && i2vRefMode == StartFrameOnly && !startReference` | `error.ai.missingStartRef` |
| `i2vRefMode == BothEnds && (!startReference \|\| !endReference)` | `error.ai.missingBothRefs` |
| `videoMode == VideoToVideo && !videoReference` | `error.ai.missingVideoRef` |
| `subtitleMode == SpeechToText && sourceAudioTrackId.isNull()` | `error.ai.missingAudioSource` |
| `modelId` が `ProviderRegistry` で解決できない | `error.ai.unknownModel` |
| 選択モデルが要求する VRAM > 利用可能 VRAM (ローカル時) | `error.ai.insufficientVram` (警告に留める) |

## 7.3 タスクのライフサイクル

```
                        ┌─────────┐
     submit() ────────> │ Queued  │
                        └────┬────┘
                             │ ワーカが取得
                             v
                        ┌───────────┐
                        │ Preparing │  参照フレーム抽出 / 音声抽出 / モデルロード
                        └────┬──────┘
                             │
                             v
                        ┌─────────┐    progress(0..1) を随時通知
                        │ Running │◄──────────────┐
                        └────┬────┘               │
                             │                    │ リトライ (最大 2 回)
                             v                    │
                     ┌───────────────┐            │
                     │ PostProcessing│            │ 一時的エラー
                     └────┬──────────┘            │
                          │ 尺合わせ / エンコード / 正規化
                          v
                     ┌─────────┐
                     │ Cached  │  .yave_cache/gen/<uuid>/ に成果物が存在
                     └────┬────┘
                          │ ユーザーが承認 or 自動コミット設定
                          v
                     ┌───────────┐
                     │ Committed │  Timeline に反映済み (Undo 可能)
                     └───────────┘

  任意の状態から:  cancel() -> Cancelled
                   回復不能エラー -> Failed
```

```cpp
// src/ai/AiGenerationTask.h
namespace yave::ai {

enum class TaskState { Queued, Preparing, Running, PostProcessing, Cached, Committed,
                       Failed, Cancelled };

struct GeneratedAsset
{
    enum class Type { Video, Audio, Image, ImageSequence, SubtitleData, Json };
    Type    type = Type::Video;
    QString path;                 // .yave_cache 内の絶対パス
    QSize   resolution;
    int64_t durationFrames = 0;
    Rational frameRate;
    QJsonObject metadata;
};

class AiGenerationTask : public QObject
{
    Q_OBJECT
public:
    AiGenerationTask(AiGenerationParams params, QObject* parent = nullptr);

    QUuid                     id()     const { return id_; }
    TaskState                 state()  const { return state_.load(); }
    const AiGenerationParams& params() const { return params_; }
    double                    progress() const { return progress_.load(); }
    QString                   statusMessageKey() const;   // 翻訳キー
    QString                   errorMessage() const;
    const std::vector<GeneratedAsset>& assets() const { return assets_; }

    void cancel();

    /// ディスクへの永続化 (アプリ再起動後の復帰用)
    QJsonObject toJson() const;
    static std::unique_ptr<AiGenerationTask> fromJson(const QJsonObject& o);

signals:
    void stateChanged(TaskState s);
    void progressChanged(double p);
    void subtitleClipsReady(std::vector<std::shared_ptr<subtitle::SubtitleClip>> clips);
    void assetsReady(std::vector<GeneratedAsset> assets);
    void failed(const QString& message);

private:
    QUuid                        id_;
    AiGenerationParams           params_;
    std::atomic<TaskState>       state_{TaskState::Queued};
    std::atomic<double>          progress_{0.0};
    std::atomic<bool>            cancelRequested_{false};
    std::vector<GeneratedAsset>  assets_;
    QString                      error_;
    int                          retryCount_ = 0;
};

} // namespace yave::ai
```

## 7.4 AiGenerationOrchestrator

```cpp
// src/ai/AiGenerationOrchestrator.h
namespace yave::ai {

class AiGenerationOrchestrator : public QObject
{
    Q_OBJECT
public:
    explicit AiGenerationOrchestrator(Project* project, QObject* parent = nullptr);
    ~AiGenerationOrchestrator() override;

    /// タスクを投入し、即座にプレースホルダクリップを配置する。
    /// 戻り値はタスク ID。
    QUuid submit(const AiGenerationParams& params);

    void cancel(const QUuid& taskId);
    void cancelAll();
    bool waitForDone(int timeoutMs);

    /// Cached 状態のタスクを Timeline へ反映する (Undo コマンドを発行)。
    void commit(const QUuid& taskId);
    void discard(const QUuid& taskId);       // 成果物を捨ててプレースホルダを削除

    /// 同一パラメータでの再生成 (seed だけ変える)
    QUuid regenerate(const QUuid& taskId, bool newSeed);

    std::vector<AiGenerationTask*> tasks() const;
    AiGenerationTask* task(const QUuid& id) const;

    /// 自動コミット設定。true なら Cached になった瞬間に commit() する。
    void setAutoCommit(bool on) { autoCommit_ = on; }

    /// セッション復帰
    void restoreFromDisk();
    void persistToDisk() const;

signals:
    void taskAdded(const QUuid& id);
    void taskStateChanged(const QUuid& id, TaskState s);
    void taskProgressChanged(const QUuid& id, double p);
    void taskFailed(const QUuid& id, const QString& message);

private:
    void runTask(AiGenerationTask* task);              // ワーカスレッドで実行
    void onTaskCached(AiGenerationTask* task);         // UI スレッド

    Project*                 project_ = nullptr;
    QThreadPool              pool_;                    // 既定 2 スレッド
    std::vector<std::unique_ptr<AiGenerationTask>> tasks_;
    ProviderRegistry*        registry_ = nullptr;
    GenerationCache*         cache_ = nullptr;
    bool                     autoCommit_ = false;
    mutable QMutex           mutex_;
};

} // namespace yave::ai
```

### 7.4.1 submit の流れ

```cpp
QUuid AiGenerationOrchestrator::submit(const AiGenerationParams& params)
{
    const auto v = params.validate();
    if (!v.ok) {
        emit taskFailed({}, LanguageManager::translateKey(v.errorKey));
        return {};
    }

    auto task = std::make_unique<AiGenerationTask>(params);
    const QUuid id = task->id();

    // (1) プレースホルダクリップを即座に配置する (Undo コマンド)
    //     ユーザーは「生成中」であることをタイムライン上で見られる
    project_->undoStack()->push(
        new InsertPlaceholderCommand(project_->timeline(), params, id));

    // (2) シグナル接続
    connect(task.get(), &AiGenerationTask::stateChanged, this,
            [this, id](TaskState s) { emit taskStateChanged(id, s); });
    connect(task.get(), &AiGenerationTask::progressChanged, this,
            [this, id](double p) {
                emit taskProgressChanged(id, p);
                project_->timeline()->updatePlaceholderProgress(id, p);
            });

    AiGenerationTask* raw = task.get();
    {
        QMutexLocker lock(&mutex_);
        tasks_.push_back(std::move(task));
    }

    // (3) ワーカへ投入
    pool_.start([this, raw] { runTask(raw); });

    persistToDisk();
    emit taskAdded(id);
    return id;
}
```

### 7.4.2 runTask (ワーカスレッド)

```cpp
void AiGenerationOrchestrator::runTask(AiGenerationTask* task)
{
    try {
        task->setState(TaskState::Preparing);

        // --- キャッシュヒット判定 ---
        const QString key = cache_->makeKey(task->params());
        if (auto cached = cache_->lookup(key)) {
            task->setAssets(*cached);
            task->setState(TaskState::Cached);
            QMetaObject::invokeMethod(this, [this, task] { onTaskCached(task); },
                                      Qt::QueuedConnection);
            return;
        }

        // --- 入力の準備 ---
        GenerationInput input = prepareInput(task->params());   // 7.5 参照
        if (task->isCancelled()) { task->setState(TaskState::Cancelled); return; }

        // --- プロバイダ解決と実行 ---
        IGenerationProvider* provider = registry_->resolve(task->params());
        if (!provider) throw GenerationError("error.ai.noProvider");

        task->setState(TaskState::Running);
        GenerationOutput output = provider->generate(
            input,
            [task](double p, const QString& msgKey) {      // 進捗コールバック
                task->setProgress(p);
                task->setStatusMessageKey(msgKey);
                return !task->isCancelled();               // false を返すと中断
            });

        if (task->isCancelled()) { task->setState(TaskState::Cancelled); return; }

        // --- 後処理 ---
        task->setState(TaskState::PostProcessing);
        std::vector<GeneratedAsset> assets = postProcess(task->params(), output);

        cache_->store(key, assets);
        task->setAssets(std::move(assets));
        task->setState(TaskState::Cached);

        QMetaObject::invokeMethod(this, [this, task] { onTaskCached(task); },
                                  Qt::QueuedConnection);
    }
    catch (const GenerationError& e) {
        if (e.isRetryable() && task->retryCount() < 2) {
            task->incrementRetry();
            pool_.start([this, task] { runTask(task); });
            return;
        }
        task->setError(e.messageKey());
        task->setState(TaskState::Failed);
    }
}
```

## 7.5 入力の準備 (`prepareInput`)

### 7.5.1 I2V の参照フレーム抽出

タイムライン上のフレームを参照画像として使う場合、**そのフレームを実際にレンダリングして
PNG に落とす**必要がある。

```cpp
// src/ai/ReferenceFrameExtractor.h
class ReferenceFrameExtractor
{
public:
    /// タイムラインの指定フレームを合成してファイルに書き出す。
    /// Render Thread へ同期リクエストを投げ、readback して保存する。
    /// ワーカスレッドから呼ばれるため、内部で UI/Render スレッドへ marshalling する。
    static QString extractToPng(Timeline* tl,
                                RhiCompositor* compositor,
                                int64_t frame,
                                const QSize& resolution,
                                const QString& outPath,
                                std::optional<QUuid> onlyTrackId = std::nullopt);
};
```

`onlyTrackId` を指定すると、そのトラックだけを合成する
(「このレイヤーの映像だけを参照したい」という V2V の要求に対応)。

### 7.5.2 I2VReferenceMode ごとの処理

```cpp
GenerationInput AiGenerationOrchestrator::prepareInput(const AiGenerationParams& p)
{
    GenerationInput in;
    in.params = p;
    const QDir workDir = cache_->workDirFor(p);      // .yave_cache/gen/<uuid>/

    if (p.kind == GenerationKind::Video) {
        switch (p.videoMode) {

        case VideoGenMode::TextToVideo:
            // 参照画像なし。プロンプトのみ。
            break;

        case VideoGenMode::ImageToVideo:
            switch (p.i2vRefMode) {
            case I2VReferenceMode::StartFrameOnly:
                in.startImagePath = resolveImageRef(*p.startReference,
                                                    p.range.start, workDir, "ref_start.png");
                break;

            case I2VReferenceMode::EndFrameOnly:
                in.endImagePath = resolveImageRef(*p.endReference,
                                                  p.range.end() - 1, workDir, "ref_end.png");
                break;

            case I2VReferenceMode::BothEnds:
                // 前後キーフレーム間の補間生成
                in.startImagePath = resolveImageRef(*p.startReference,
                                                    p.range.start, workDir, "ref_start.png");
                in.endImagePath   = resolveImageRef(*p.endReference,
                                                    p.range.end() - 1, workDir, "ref_end.png");
                in.interpolationMode = true;
                break;
            }
            break;

        case VideoGenMode::VideoToVideo: {
            // ソースレイヤーの映像を区間分だけ書き出す
            const VideoReference& vr = *p.videoReference;
            const TimeRange srcRange = vr.sourceRange.isEmpty() ? p.range : vr.sourceRange;
            in.sourceVideoPath = renderTrackSegmentToFile(
                vr.sourceTrackId, srcRange, workDir.filePath("v2v_source.mp4"));
            in.denoiseStrength = vr.denoiseStrength;
            break;
        }
        }

        // 生成尺の決定
        in.targetFrameCount = rescaleFrames(p.range.duration,
                                            project_->timebase(),
                                            p.outputFrameRate,
                                            RoundMode::Nearest);
    }
    else if (p.kind == GenerationKind::Subtitle &&
             p.subtitleMode == SubtitleGenMode::SpeechToText) {
        // 音声トラックの指定区間を 16kHz mono WAV に書き出す (Whisper 系の標準入力形式)
        in.sourceAudioPath = renderAudioTrackToWav(
            p.sourceAudioTrackId, p.range, 16000, 1, workDir.filePath("stt_input.wav"));
    }
    else if (p.kind == GenerationKind::Mask) {
        in.sourceVideoPath = renderTrackSegmentToFile(
            p.maskSourceTrackId, p.range, workDir.filePath("mask_source.mp4"));
        in.maskHints = p.maskHintPoints;
    }

    return in;
}
```

> **`BothEnds` (両端参照補間) の実装上の注意**: この機能をネイティブにサポートする
> モデルは限られる (Wan 2.2 FLF2V など)。サポートしないモデルが選ばれた場合は、
> `ProviderCapability` で事前に検出して UI で選択肢をグレーアウトする。
> フォールバックとして「始点から I2V 生成 → 終点から逆再生 I2V 生成 → 中間でクロスフェード」
> という近似手法を提供するが、これは品質が劣るため明示的にユーザーへ提示する。

## 7.6 IGenerationProvider

```cpp
// src/ai/IGenerationProvider.h
namespace yave::ai {

/// プロバイダが対応可能な機能の宣言
struct ProviderCapability
{
    QString providerId;
    QString displayNameKey;

    std::set<GenerationKind>   kinds;
    std::set<VideoGenMode>     videoModes;
    std::set<I2VReferenceMode> i2vModes;        // BothEnds 対応可否がここで分かる
    std::set<AudioGenMode>     audioModes;
    std::set<SubtitleGenMode>  subtitleModes;

    bool    requiresNetwork  = false;
    bool    supportsProgress = true;
    bool    supportsCancel   = true;
    int64_t estimatedVramMb  = 0;
    QStringList supportedModelIds;
};

struct GenerationInput
{
    AiGenerationParams params;
    QString  startImagePath;
    QString  endImagePath;
    QString  sourceVideoPath;
    QString  sourceAudioPath;
    bool     interpolationMode = false;
    double   denoiseStrength   = 0.6;
    int64_t  targetFrameCount  = 0;
    std::vector<QPointF> maskHints;
    QDir     workDir;
};

struct GenerationOutput
{
    std::vector<QString> producedFiles;   // workDir 内の生成物
    QJsonObject          metadata;        // 実際に使われた seed / モデルバージョン等
};

/// 進捗コールバック。false を返すとプロバイダは中断すべき。
using ProgressFn = std::function<bool(double progress, const QString& statusKey)>;

class IGenerationProvider
{
public:
    virtual ~IGenerationProvider() = default;
    virtual ProviderCapability capability() const = 0;
    virtual bool isAvailable(QString* reasonOut = nullptr) const = 0;
    virtual GenerationOutput generate(const GenerationInput& in, const ProgressFn& progress) = 0;
    virtual void warmUp(const QString& modelId) {}    // モデルの事前ロード
    virtual void releaseResources() {}
};

} // namespace yave::ai
```

### 7.6.1 OnnxLocalProvider

```cpp
// src/ai/providers/OnnxLocalProvider.h
class OnnxLocalProvider : public IGenerationProvider
{
public:
    ProviderCapability capability() const override;
    bool isAvailable(QString* reasonOut) const override;
    GenerationOutput generate(const GenerationInput& in, const ProgressFn& progress) override;

private:
    Ort::Session* sessionFor(const QString& modelId);

    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "yave"};
    std::unordered_map<QString, std::unique_ptr<Ort::Session>> sessions_;
    QMutex sessionMutex_;
};
```

Execution Provider の設定:

```cpp
Ort::SessionOptions makeSessionOptions()
{
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    opts.SetIntraOpNumThreads(std::max(1, QThread::idealThreadCount() / 2));

#if defined(Q_OS_WIN)
    if (cudaAvailable()) {
        OrtCUDAProviderOptions cuda{};
        cuda.device_id = 0;
        opts.AppendExecutionProvider_CUDA(cuda);
    } else {
        // DirectML は任意の DX12 GPU で動く (AMD / Intel 含む)
        opts.DisableMemPattern();
        opts.SetExecutionMode(ORT_SEQUENTIAL);       // DirectML の要求
        OrtSessionOptionsAppendExecutionProvider_DML(opts, 0);
    }
#elif defined(Q_OS_MACOS)
    uint32_t coremlFlags = COREML_FLAG_USE_CPU_AND_GPU;
    OrtSessionOptionsAppendExecutionProvider_CoreML(opts, coremlFlags);
#endif
    return opts;
}
```

**担当タスク**:

| モデル | 用途 | 目安サイズ |
|---|---|---|
| Whisper (small/medium) ONNX | STT | 250MB / 800MB |
| Silero VAD | 発話区間検出 (STT の前処理) | 2MB |
| VITS / Piper | TTS | 60〜100MB |
| MobileSAM / U2Net | マスク生成 | 40MB / 170MB |
| RIFE | フレーム補間 | 20MB |
| Real-ESRGAN | 超解像 | 65MB |

モデルは `%APPDATA%/YAVE/models/` へ初回使用時にダウンロードする
(ハッシュ検証を必須にする)。

### 7.6.2 RemoteHttpProvider

```cpp
class RemoteHttpProvider : public IGenerationProvider
{
public:
    struct Endpoint {
        QString id;
        QString displayName;
        QUrl    baseUrl;
        QString apiKeyRef;         // 実際のキーは QSettings ではなく OS のキーチェーンに保存
        enum class Protocol { ComfyUi, OpenAiCompatible, Replicate, Custom } protocol;
        QString customTemplatePath;  // Custom のときのリクエストテンプレート (JSON)
    };

    explicit RemoteHttpProvider(const Endpoint& ep);
    GenerationOutput generate(const GenerationInput& in, const ProgressFn& progress) override;
};
```

**API キーの扱い**:
- 平文で `settings.ini` に書かない。Windows は DPAPI (`CredWrite`)、
  macOS は Keychain (`SecItemAdd`) に保存する。
- **プロジェクト JSON には API キーを一切保存しない**。エンドポイント ID のみを保存し、
  キーはマシンローカルに置く。プロジェクトを共有してもキーは漏れない。

**進捗取得**: ComfyUI は WebSocket、Replicate はポーリング。
プロトコルごとに `IProgressWatcher` を分ける。

**ネットワーク送信の確認**: リモートプロバイダを初めて使うとき、
「この操作は選択したエンドポイントへ映像/音声データを送信します」という
明示的な確認ダイアログを出す。一度承認したエンドポイントは以後確認しない
(設定で再確認を有効にできる)。

> 参照フレームやソース映像は、ユーザーの著作物であったり被写体が写り込んでいたりする。
> どこへ送られるかを黙って処理するべきではない。

### 7.6.3 SidecarProvider

Python 製の推論スクリプトを子プロセスとして起動し、標準入出力で JSON をやり取りする。
**ローカル完結を保ちながら、ONNX 化されていない大型モデルを扱える。**

```cpp
class SidecarProvider : public IGenerationProvider
{
public:
    struct Config {
        QString id;
        QString executablePath;      // "python" or 同梱の埋め込み Python
        QStringList arguments;       // スクリプトパス等
        QString workingDirectory;
        int     startupTimeoutMs = 60000;
    };

    GenerationOutput generate(const GenerationInput& in, const ProgressFn& progress) override;
};
```

IPC プロトコル (行区切り JSON):

```
--> {"cmd":"generate","input":{...GenerationInput...}}
<-- {"event":"progress","value":0.12,"status":"sampling"}
<-- {"event":"progress","value":0.45,"status":"sampling"}
<-- {"event":"done","files":["out.mp4"],"metadata":{"seed":12345}}
```

```
--> {"cmd":"cancel"}
<-- {"event":"cancelled"}
```

## 7.7 後処理 (`postProcess`)

生成物はそのままでは使えないことが多いため、必ず後処理を通す。

### 7.7.1 動画

| 課題 | 対処 |
|---|---|
| 生成 fps がプロジェクト fps と違う (例: 生成 16fps → プロジェクト 59.94fps) | RIFE によるフレーム補間、または単純な重複フレーム挿入。ユーザー選択 |
| 生成尺が区間尺と違う | 尺が短い: ループ or 最終フレーム保持 or 速度変更。尺が長い: トリム。既定は「速度変更で区間にフィット」 |
| 解像度がプロジェクトと違う | Real-ESRGAN で超解像、または Lanczos 拡大。既定は Lanczos |
| コーデックが編集に不向き (長 GOP) | All-Intra へ再エンコード (プロキシと同じ扱い) |

```cpp
std::vector<GeneratedAsset> postProcessVideo(const AiGenerationParams& p,
                                             const GenerationOutput& out,
                                             const Rational& projectTb)
{
    const QString raw = out.producedFiles.front();

    // (1) fps 合わせ
    QString fpsAdjusted = raw;
    if (p.outputFrameRate != projectTb) {
        fpsAdjusted = interpolateFps(raw, p.outputFrameRate, projectTb,
                                     p.extraParams["fpsMethod"].toString("rife"));
    }

    // (2) 尺合わせ
    const int64_t producedFrames = probeFrameCount(fpsAdjusted);
    QString fitted = fpsAdjusted;
    if (producedFrames != p.range.duration) {
        fitted = fitDuration(fpsAdjusted, producedFrames, p.range.duration,
                             p.extraParams["fitMethod"].toString("speed"));
    }

    // (3) 編集向けコーデックへ
    const QString final = transcodeToAllIntra(fitted);

    GeneratedAsset a;
    a.type           = GeneratedAsset::Type::Video;
    a.path           = final;
    a.durationFrames = p.range.duration;
    a.frameRate      = projectTb;
    a.metadata       = out.metadata;      // seed 等
    return {a};
}
```

### 7.7.2 音声

- サンプルレートをプロジェクトに合わせてリサンプル (libswresample)
- ラウドネス正規化 (EBU R128、目標 -16 LUFS)。ナレーションで音量がバラつくのを防ぐ
- 無音のトリム (先頭・末尾)
- WAV (float32) で保存

### 7.7.3 字幕

`SubtitleClip` 群への変換は [6.10](06-subtitle-engine.md) を参照。
加えて後処理として:

- 1 キューあたりの最大文字数で分割 (既定: 日本語 20 文字 / 英語 42 文字)
- 最小表示時間の強制 (既定 1.0 秒)
- フィラー除去 (「えー」「あの」など、オプション)

### 7.7.4 seed の書き戻し

```cpp
// 再現性のため、実際に使われた seed をパラメータに書き戻す
if (task->params().seed < 0) {
    const int64_t actualSeed = out.metadata["seed"].toVariant().toLongLong();
    task->mutableParams().seed = actualSeed;
}
```

これにより、プロジェクトを開き直して再生成しても同じ結果が得られる
(モデルとプロバイダが同じであれば)。

## 7.8 キャッシュとコミット

### 7.8.1 GenerationCache

```cpp
// src/ai/GenerationCache.h
class GenerationCache
{
public:
    explicit GenerationCache(const QDir& projectCacheDir);

    /// パラメータから決定的なキーを作る。
    /// seed が -1 (ランダム) の場合はキャッシュ対象外 (毎回異なる結果を期待しているため)。
    QString makeKey(const AiGenerationParams& p) const;

    std::optional<std::vector<GeneratedAsset>> lookup(const QString& key) const;
    void store(const QString& key, const std::vector<GeneratedAsset>& assets);

    QDir workDirFor(const AiGenerationParams& p) const;   // .yave_cache/gen/<uuid>/

    qint64 totalSizeBytes() const;
    void   trimTo(qint64 maxBytes);       // LRU で古い生成物を削除
    void   clearAll();
};
```

キーの計算:

```cpp
QString GenerationCache::makeKey(const AiGenerationParams& p) const
{
    if (p.seed < 0) return {};      // ランダムシードはキャッシュしない

    QCryptographicHash h(QCryptographicHash::Sha256);
    QJsonObject o = p.toJson();
    o.remove("targetTrackId");      // 配置先はキーに含めない
    o.remove("createNewTrack");
    o.remove("replaceExistingClips");
    h.addData(QJsonDocument(o).toJson(QJsonDocument::Compact));

    // 参照画像の内容もキーに含める (パスだけでは中身の変更を検出できない)
    if (p.startReference) h.addData(fileHash(resolvePath(*p.startReference)));
    if (p.endReference)   h.addData(fileHash(resolvePath(*p.endReference)));

    return QString::fromLatin1(h.result().toHex());
}
```

### 7.8.2 プレースホルダとコミット

```cpp
// src/core/AiPlaceholderClip.h
class AiPlaceholderClip : public Clip
{
public:
    QUuid    taskId() const { return taskId_; }
    double   progress() const { return progress_; }
    TaskState state() const { return state_; }
    const AiGenerationParams& params() const { return params_; }

    /// プレビュー時の見た目: 進捗バー + プロンプト先頭 + 状態アイコン。
    /// レンダリング時は半透明のプレースホルダ画像を返す。
    LayerItem makeLayerItem(int64_t frame, int z, const Track& t) const override;

private:
    QUuid  taskId_;
    double progress_ = 0.0;
    TaskState state_ = TaskState::Queued;
    AiGenerationParams params_;
};
```

コミットは Undo コマンドとして実行する。

```cpp
// src/core/commands/CommitGeneratedAssetCommand.h
class CommitGeneratedAssetCommand : public QUndoCommand
{
public:
    CommitGeneratedAssetCommand(Timeline* tl,
                                const QUuid& taskId,
                                std::vector<GeneratedAsset> assets,
                                const AiGenerationParams& params);

    void redo() override
    {
        // (1) プレースホルダを取り除く
        placeholder_ = timeline_->takePlaceholder(taskId_);

        // (2) 生成種別に応じてクリップを作る
        switch (params_.kind) {
        case GenerationKind::Video:
        case GenerationKind::Image: {
            const QUuid assetId = timeline_->project()->assetLibrary()
                                      ->registerAsset(assets_.front().path);
            auto clip = std::make_shared<VideoClip>();
            clip->setAssetId(assetId);
            clip->setRange(params_.range);
            clip->setSourceOffset(0);
            clip->setGeneratedByTaskId(taskId_);
            targetTrack()->insertClip(clip);
            inserted_.push_back(clip);
            break;
        }
        case GenerationKind::Audio: {
            /* AudioClip を作って挿入。AudioRenderGraph 再構築を要求 */
            break;
        }
        case GenerationKind::Subtitle: {
            // SubtitleClip 群を挿入 (SRT 取り込みと同じ経路)
            for (auto& sc : subtitleClips_) {
                targetTrack()->insertClip(sc);
                inserted_.push_back(sc);
            }
            break;
        }
        case GenerationKind::Mask: {
            // マスクは映像レイヤーとして AlphaMask ブレンドで挿入
            break;
        }
        case GenerationKind::EffectMetadata: {
            // 対象クリップのエフェクトパラメータを書き換える
            break;
        }
        }
    }

    void undo() override
    {
        for (auto& c : inserted_) targetTrack()->removeClip(c->id());
        inserted_.clear();
        if (placeholder_) timeline_->restorePlaceholder(placeholder_);
    }
};
```

### 7.8.3 生成物の収集 (プロジェクト保存時)

`.yave_cache/` は削除されうるため、保存時に選択肢を出す。

| モード | 動作 |
|---|---|
| **キャッシュ参照** (既定) | JSON にはパラメータのみ保存。キャッシュが消えたら再生成が必要 |
| **プロジェクトへ収集** | 生成物を `<project>/assets/generated/` へコピーし、JSON からそれを参照 |
| **両方** | コピーしつつパラメータも保存 |

「プロジェクトへ収集」を選ぶとファイルサイズが大きくなるが、他マシンへ持ち出しても
そのまま再生できる。**共有前提のプロジェクトでは必須**。

## 7.9 セッション復帰

アプリがクラッシュ / 終了しても、生成タスクを再開できるようにする。

```
<project>/.yave_cache/tasks.json
{
  "schemaVersion": 1,
  "tasks": [
    {
      "id": "…uuid…",
      "state": "Running",
      "retryCount": 0,
      "params": { …AiGenerationParams… },
      "workDir": "gen/…uuid…",
      "startedAt": "2026-08-24T12:34:56Z"
    }
  ]
}
```

起動時 `restoreFromDisk()`:

- `Cached` / `Committed` → そのまま復元
- `Queued` / `Preparing` / `Running` → **`Queued` に戻して再投入**
  (リモート生成の場合は、ジョブ ID が残っていれば結果の取得だけを試みる)
- `Failed` / `Cancelled` → 記録として残すがワーカへは投入しない

## 7.10 UI 設計

### 7.10.1 生成ダイアログ (`AiGenerateDialog.qml`)

タイムライン上で区間を選択して右クリック →「AI 生成」で開く。

```
┌─ AI 生成 ───────────────────────────────────────────────┐
│ 対象トラック: [Video 3        ▼]   区間: 00:01:23:00 - 00:01:31:00 (8.0s) │
│                                                                          │
│ 種別: (●)動画  ( )音声  ( )字幕  ( )マスク  ( )画像                       │
│ ─────────────────────────────────────────────────────  │
│ モード: (●)T2V  ( )I2V  ( )V2V                                            │
│                                                                          │
│  ┌─ I2V 選択時のみ表示 ──────────────────────────────┐                   │
│  │ 参照: (●)開始時点のみ ( )終了時点のみ ( )両端(補間)  │                   │
│  │ 開始画像: [タイムラインから取得 ▼] [ 選択... ]        │                   │
│  │   ┌────────┐  ← 抽出プレビュー                       │                   │
│  │   │        │                                          │                   │
│  │   └────────┘                                          │                   │
│  └──────────────────────────────────────────────┘                   │
│                                                                          │
│ モデル: [Wan 2.2 I2V 14B (リモート: ComfyUI)  ▼]  ⓘ 両端参照に対応         │
│ プロンプト:  ┌──────────────────────────────────┐                  │
│              │                                            │                  │
│              └──────────────────────────────────┘                  │
│ ネガティブ:  [                                          ]                  │
│ Seed: [ -1 (ランダム) ]  Steps: [30]  CFG: [7.5]                          │
│                                                                          │
│ 出力: 1280x720 @ 30fps → 尺合わせ: [速度変更 ▼]  fps 変換: [RIFE ▼]        │
│                                                                          │
│ ⚠ このモデルはリモートエンドポイントへ参照画像を送信します                 │
│                                                                          │
│                                    [ キャンセル ]  [ 生成 ]               │
└──────────────────────────────────────────────────────────┘
```

### 7.10.2 タスク一覧パネル (`AiTaskListPanel.qml`)

```
┌─ AI タスク ──────────────────────────────────┐
│ ▶ [████████░░] 78%  T2V "夕暮れの海岸を歩く人物"  [×]│
│   Video 3 / 00:01:23:00 - 00:01:31:00                 │
├────────────────────────────────────────────┤
│ ✓ 完了  STT "インタビュー音声"    [ 適用 ] [ 破棄 ]   │
│   Subtitle 1 / 42 キュー生成                          │
├────────────────────────────────────────────┤
│ ✗ 失敗  I2V  接続エラー          [ 再試行 ] [ × ]     │
└────────────────────────────────────────────┘
```
