# 2. ディレクトリ構成

[← 目次に戻る](../design.md)

---

## 2.1 全体構造

```
YetAnotherVideoEditor/
├── CMakeLists.txt                  ルートビルド定義 (1.4.3 参照)
├── CMakePresets.json               Win/Mac 用のプリセット
├── vcpkg.json                      Windows 依存 manifest
├── .clang-format                   コーディングスタイル (LLVM ベース / 4 space / 110 col)
├── .clang-tidy
├── README.md
│
├── cmake/                          自作 CMake モジュール
│   ├── FindFFmpeg.cmake
│   ├── PlatformConfig.cmake        コンパイルフラグ・警告レベル・SIMD 設定
│   ├── YaveShaders.cmake           .vert/.frag -> .qsb 変換のヘルパ関数
│   └── YaveDeploy.cmake            windeployqt / macdeployqt ラッパ
│
├── doc/                            本設計書
│   ├── design.md
│   └── design/
│       └── 01..12-*.md
│
├── include/yave/                   公開ヘッダ (SDK として外部に出すもののみ)
│   ├── sdk/
│   │   ├── SubtitleEffectApi.h     字幕エフェクトプラグインの C ABI 定義
│   │   ├── ISubtitleEffect.h       プラグイン作者が実装するインタフェース
│   │   ├── ParameterSchema.h
│   │   └── YaveSdkVersion.h
│   └── YaveExport.h                DLL エクスポートマクロ
│
├── src/
│   ├── CMakeLists.txt
│   │
│   ├── core/                       yave_core : Qt Core のみ依存
│   │   ├── CMakeLists.txt
│   │   ├── Rational.h / .cpp
│   │   ├── TimeRange.h / .cpp
│   │   ├── Clip.h / .cpp
│   │   ├── VideoClip.h / .cpp
│   │   ├── AudioClip.h / .cpp
│   │   ├── AiPlaceholderClip.h / .cpp
│   │   ├── Track.h / .cpp
│   │   ├── Timeline.h / .cpp
│   │   ├── Project.h / .cpp
│   │   ├── AssetLibrary.h / .cpp   素材(ファイル)の登録・参照カウント・プロキシ管理
│   │   ├── RenderSnapshot.h / .cpp UI -> Render の受け渡し構造体
│   │   └── commands/               QUndoCommand 派生
│   │       ├── AddTrackCommand.h
│   │       ├── RemoveTrackCommand.h
│   │       ├── ReorderTrackCommand.h
│   │       ├── AddClipCommand.h
│   │       ├── MoveClipCommand.h
│   │       ├── TrimClipCommand.h
│   │       ├── SplitClipCommand.h
│   │       ├── ImportSubtitleCommand.h
│   │       └── CommitGeneratedAssetCommand.h
│   │
│   ├── media/                      yave_media : FFmpeg
│   │   ├── FFmpegRaii.h            AVFrame/AVPacket/AVCodecContext の unique_ptr Deleter
│   │   ├── MediaProbe.h / .cpp     ファイルのストリーム情報取得
│   │   ├── VideoDecoder.h / .cpp
│   │   ├── AudioDecoder.h / .cpp
│   │   ├── HwDeviceContext.h / .cpp  HW デバイス生成と優先順位決定
│   │   ├── FrameCache.h / .cpp     LRU デコード済みフレームキャッシュ
│   │   ├── FrameQueue.h            SPSC ロックフリーリングバッファ
│   │   ├── DecodeWorkerPool.h / .cpp
│   │   ├── VideoEncoder.h / .cpp
│   │   ├── AudioEncoder.h / .cpp
│   │   ├── Muxer.h / .cpp
│   │   └── ExportJob.h / .cpp      書き出しジョブ (プレビューとは別経路)
│   │
│   ├── render/                     yave_render : QRhi
│   │   ├── RhiContext.h / .cpp     QRhi 生成とバックエンド選択
│   │   ├── RhiCompositor.h / .cpp  レイヤー合成の中核
│   │   ├── TexturePool.h / .cpp
│   │   ├── BlendMode.h
│   │   ├── LayerPass.h / .cpp      1 レイヤー分の描画パス
│   │   ├── ColorSpace.h / .cpp     YUV->RGB 行列、HDR トーンマップ
│   │   └── shaders/
│   │       ├── fullscreen.vert
│   │       ├── layer_blend.frag    ブレンドモード分岐を含む
│   │       ├── yuv_to_rgb.frag
│   │       └── subtitle_glyph.vert / .frag   インスタンシング描画用
│   │
│   ├── audio/                      yave_audio
│   │   ├── IAudioDevice.h          デバイス抽象 (open/start/stop/callback)
│   │   ├── AudioRenderEngine.h / .cpp
│   │   ├── AudioRenderGraph.h / .cpp  RT スレッドが読む POD グラフ
│   │   ├── AudioClock.h            サンプル単位のアトミック再生位置
│   │   ├── LockFreeRingBuffer.h
│   │   ├── DelayCompensator.h / .cpp  PDC
│   │   ├── Resampler.h / .cpp      libswresample ラッパ
│   │   └── MeterBridge.h / .cpp    RT -> UI のレベル通知
│   │
│   ├── subtitle/                   yave_subtitle
│   │   ├── SubtitleClip.h / .cpp
│   │   ├── SubtitleText.h / .cpp   リッチスパン付きテキスト
│   │   ├── SubtitleStyle.h / .cpp
│   │   ├── SubtitleStylePreset.h / .cpp
│   │   ├── SubtitleLayout.h / .cpp QTextLayout -> SubtitleGlyphRun
│   │   ├── SubtitleGlyphRun.h
│   │   ├── GlyphAtlas.h / .cpp     ラスタライズ結果の QRhiTexture 管理
│   │   ├── SubtitleRenderer.h / .cpp
│   │   ├── SubtitleEffectInstance.h / .cpp
│   │   ├── SubtitleEffectRegistry.h / .cpp
│   │   ├── effects/                組み込みエフェクト (ISubtitleEffect 実装)
│   │   │   ├── FadeEffect.cpp
│   │   │   ├── TypewriterEffect.cpp
│   │   │   ├── KaraokeEffect.cpp
│   │   │   ├── SlideInEffect.cpp
│   │   │   ├── PopPerCharEffect.cpp
│   │   │   ├── WaveEffect.cpp
│   │   │   └── BlurEffect.cpp
│   │   └── io/
│   │       ├── SrtParser.h / .cpp
│   │       ├── SrtWriter.h / .cpp
│   │       ├── AssParser.h / .cpp
│   │       ├── AssWriter.h / .cpp
│   │       └── VttParser.h / .cpp
│   │
│   ├── ai/                         yave_ai
│   │   ├── AiGenerationParams.h / .cpp
│   │   ├── AiGenerationTask.h / .cpp
│   │   ├── AiGenerationOrchestrator.h / .cpp
│   │   ├── IGenerationProvider.h
│   │   ├── ProviderRegistry.h / .cpp
│   │   ├── GenerationCache.h / .cpp
│   │   ├── ReferenceFrameExtractor.h / .cpp  I2V/V2V の参照フレーム取り出し
│   │   └── providers/
│   │       ├── OnnxLocalProvider.h / .cpp
│   │       ├── RemoteHttpProvider.h / .cpp
│   │       ├── SidecarProvider.h / .cpp
│   │       └── ProviderCapability.h
│   │
│   ├── plugin/                     yave_plugin
│   │   ├── PluginManager.h / .cpp
│   │   ├── PluginDescriptor.h
│   │   ├── PluginWindow.h / .cpp   QWidget 派生。ポップアップ窓
│   │   ├── vst3/
│   │   │   ├── Vst3Host.h / .cpp
│   │   │   ├── Vst3Registry.h / .cpp
│   │   │   ├── Vst3ComponentHandler.h / .cpp
│   │   │   ├── Vst3RunLoop.h / .cpp        Linux 用フック(将来用)
│   │   │   └── Vst3ProcessorNode.h / .cpp  AudioRenderGraph 上のノード
│   │   ├── subtitle/
│   │   │   ├── SubtitleEffectLoader.h / .cpp   .dll/.dylib の C ABI ロード
│   │   │   └── BuiltinEffectFactory.h / .cpp
│   │   └── aviutl/                 ★ Windows のみビルド対象
│   │       ├── AviUtlHost.h / .cpp
│   │       ├── AviUtlRegistry.h / .cpp
│   │       ├── AviUtlFrameBridge.h / .cpp  RGBA <-> YC48 変換
│   │       ├── AviUtlSubtitleEffectAdapter.h / .cpp
│   │       └── AviUtlWindowProc.h / .cpp
│   │
│   ├── platform/
│   │   ├── PlatformNative.h        共通の前方宣言 (NativeViewHandle 型など)
│   │   ├── win/
│   │   │   ├── D3D11Interop.cpp    ID3D11Texture2D -> QRhiTexture
│   │   │   ├── WasapiDevice.cpp
│   │   │   └── Win32NativeView.cpp
│   │   └── mac/
│   │       ├── MetalInterop.mm     CVPixelBuffer/IOSurface -> MTLTexture
│   │       ├── CoreAudioDevice.cpp
│   │       └── MacNativeView.mm    NSView 取得 (Objective-C++)
│   │
│   ├── io/                         プロジェクト保存
│   │   ├── ProjectSerializer.h / .cpp
│   │   ├── JsonKeys.h              JSON キー文字列の一元定義
│   │   ├── SchemaMigration.h / .cpp
│   │   └── PathResolver.h / .cpp   相対 <-> 絶対パス変換
│   │
│   ├── i18n/
│   │   └── LanguageManager.h / .cpp
│   │
│   ├── app/                        yave_app
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── controllers/
│   │   │   ├── ProjectController.h / .cpp
│   │   │   ├── EditController.h / .cpp
│   │   │   ├── PlaybackController.h / .cpp
│   │   │   ├── AiController.h / .cpp
│   │   │   └── PluginController.h / .cpp
│   │   ├── models/
│   │   │   ├── TimelineModel.h / .cpp     QAbstractItemModel
│   │   │   ├── TrackListModel.h / .cpp
│   │   │   ├── ClipListModel.h / .cpp
│   │   │   ├── EffectStackModel.h / .cpp
│   │   │   └── ParameterModel.h / .cpp    parameterSchema -> UI 自動生成
│   │   ├── items/
│   │   │   └── PreviewItem.h / .cpp       QQuickRhiItem 派生のプレビュー表示
│   │   └── qml/
│   │       ├── MainWindow.qml
│   │       ├── timeline/
│   │       │   ├── TimelineView.qml
│   │       │   ├── TrackHeader.qml
│   │       │   ├── ClipItem.qml
│   │       │   ├── SubtitleClipItem.qml
│   │       │   └── Ruler.qml
│   │       ├── inspector/
│   │       │   ├── InspectorPanel.qml
│   │       │   ├── SubtitleInspector.qml
│   │       │   ├── EffectStackEditor.qml
│   │       │   └── AutoParameterForm.qml  ParameterSchema からの自動生成
│   │       ├── ai/
│   │       │   ├── AiGenerateDialog.qml
│   │       │   └── AiTaskListPanel.qml
│   │       └── common/
│   │           ├── Theme.qml
│   │           └── IconButton.qml
│   │
│   └── util/
│       ├── Log.h / .cpp            カテゴリ付きロギング (QLoggingCategory)
│       ├── Result.h                Result<T, Error> (例外を使わない層のため)
│       ├── ScopeGuard.h
│       └── Hash.h
│
├── i18n/                           翻訳ソース
│   ├── yave_ja.ts
│   └── yave_en.ts
│
├── resources/
│   ├── icons/
│   ├── fonts/
│   └── yave.qrc
│
├── plugins_sdk_example/            字幕エフェクトプラグインのサンプル実装
│   ├── CMakeLists.txt
│   └── GlitchEffect.cpp
│
├── third_party/
│   ├── CMakeLists.txt
│   ├── vst3sdk/                    FetchContent で取得(コミットしない)
│   └── aviutl_sdk/                 ヘッダのみ同梱
│       ├── filter.h
│       └── input.h
│
└── tests/
    ├── CMakeLists.txt
    ├── tst_rational.cpp
    ├── tst_timeline.cpp
    ├── tst_srtparser.cpp
    ├── tst_projectserializer.cpp
    ├── tst_delaycompensator.cpp
    └── data/
        ├── sample.srt
        └── sample_project.yave
```

## 2.2 モジュール依存関係

矢印は「左が右に依存する」。循環依存を作らないこと。

```
yave_app ──┬── yave_render ──┬── yave_media ── yave_core
           │                 └── yave_subtitle ── yave_core
           ├── yave_audio ──── yave_core
           ├── yave_ai ─────── yave_core
           ├── yave_plugin ─┬─ yave_audio     (VST3 processor node のため)
           │                └─ yave_subtitle  (ISubtitleEffect のため)
           └── yave_core

yave_sdk (INTERFACE) : 誰にも依存しない。yave_subtitle と外部プラグインが include する
```

- `yave_core` は **FFmpeg / QRhi / ONNX に一切依存しない**。これによりコアのユニットテストが
  GPU もコーデックも無い CI 上で走る。
- `yave_plugin` が `yave_audio` と `yave_subtitle` の両方に依存するのは、プラグインが
  音声処理ノードにも字幕エフェクトにもなり得るため。逆方向の依存は作らない
  (`yave_subtitle` は `ISubtitleEffect` インタフェースだけを知っていて、
  それを誰がロードするかは知らない)。

## 2.3 命名規約

| 対象 | 規約 | 例 |
|---|---|---|
| クラス / 構造体 | UpperCamelCase | `SubtitleClip` |
| メンバ関数 | lowerCamelCase | `insertClip()` |
| メンバ変数 | lowerCamelCase + 末尾 `_` | `tracks_` |
| ローカル変数 / 引数 | lowerCamelCase | `frameIndex` |
| 定数 / enum 値 | UpperCamelCase | `BlendMode::Multiply` |
| ネームスペース | 小文字 | `yave::subtitle` |
| ファイル名 | クラス名と一致 | `SubtitleClip.h` |
| JSON キー | lowerCamelCase | `"effectStack"` |

ネームスペースは以下:

```cpp
namespace yave           { }   // core
namespace yave::media    { }
namespace yave::render   { }
namespace yave::audio    { }
namespace yave::subtitle { }
namespace yave::ai       { }
namespace yave::plugin   { }
namespace yave::io       { }
namespace yave::sdk      { }   // 外部プラグイン作者に公開する範囲
```

> **`Debug` という名前のネームスペース / クラスは作らない。**
> Qt の `qDebug` や Windows SDK のマクロと衝突しやすい。診断系は `yave::diag` とする。

## 2.4 生成物の配置

```
build/
├── bin/                     実行ファイル・DLL
│   ├── yave.exe / yave.app
│   ├── shaders/*.qsb        (qrc に埋め込むので実行時には不要。デバッグ用)
│   └── plugins/             ビルドした字幕エフェクトサンプル
└── ...

実行時のユーザーデータ:
Windows: %APPDATA%/YAVE/
macOS:   ~/Library/Application Support/YAVE/
├── settings.ini             QSettings
├── plugin_cache.json        プラグイン走査結果のキャッシュ
├── models/                  ダウンロード済み ONNX モデル
└── logs/

プロジェクト固有:
<project_dir>/
├── MyProject.yave           JSON プロジェクトファイル
├── assets/                  収集したメディア(任意)
└── .yave_cache/             再生成可能なデータのみ。VCS 管理外にすること
    ├── gen/<task-uuid>/     AI 生成物
    ├── proxy/               プロキシ(低解像度)メディア
    ├── waveform/            波形サムネイル
    └── thumbs/              クリップサムネイル
```

> `.yave_cache/` は削除しても編集内容が失われないことを保証する。
> AI 生成物もここに置くが、プロジェクト JSON には**生成パラメータが完全に保存されている**ため
> 再生成できる。ただし乱数シードを固定していない生成は同一結果にならないので、
> 保存時に「生成物をプロジェクトへ収集する」オプションを提供する ([7章](07-ai-orchestrator.md) 参照)。
