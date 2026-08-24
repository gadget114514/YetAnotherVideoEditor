# 1. 全体アーキテクチャ設計

[← 目次に戻る](../design.md)

---

## 1.1 レイヤ構造

上位レイヤは下位レイヤにのみ依存する。逆方向の依存は Qt のシグナル / コールバック
インタフェース経由に限定し、コンパイル時依存を作らない。

```
+-----------------------------------------------------------------------+
|  Presentation Layer                                                    |
|    QML (Timeline / Preview / Inspector / Library)                      |
|    QWidget (PluginWindow のみ -- ネイティブ埋め込みが必要なため)         |
+-----------------------------------------------------------------------+
                    | Q_PROPERTY / Q_INVOKABLE / signals
+-----------------------------------------------------------------------+
|  Controller Layer  (QObject)                                           |
|    ProjectController   EditController   PlaybackController             |
|    AiController        StoryboardController  PluginController          |
|    LanguageManager                                                     |
|    TimelineModel (QAbstractItemModel : QML へのブリッジ)                |
+-----------------------------------------------------------------------+
                    | 直接呼び出し (非 QObject / 純粋 C++)
+-----------------------------------------------------------------------+
|  Core Layer  (Qt 非依存にできる部分は非依存にする)                        |
|    Timeline  Track  Clip  Rational  TimeRange                          |
|    Project   UndoStack (QUndoStack)  AssetLibrary                      |
+-----------------------------------------------------------------------+
                    |
+-----------------------------------------------------------------------+
|  Engine Layer                                                          |
|    RhiCompositor   VideoEngine   AudioRenderEngine                     |
|    SubtitleEngine  AiGenerationOrchestrator  PluginManager             |
+-----------------------------------------------------------------------+
                    |
+-----------------------------------------------------------------------+
|  Platform Layer                                                        |
|    win/ : D3D11Interop  WasapiDevice  AviUtlHost  Win32PluginWindow    |
|    mac/ : MetalInterop   CoreAudioDevice        MacPluginWindow        |
|    common/ : FFmpeg wrappers  OnnxRuntime wrapper  HttpClient          |
+-----------------------------------------------------------------------+
```

## 1.2 コンポーネント関連図

```
                          +---------------------------+
                          |     ProjectController     |
                          |  open / save / new        |
                          +------------+--------------+
                                       | owns
                                       v
   +-----------------------------------+------------------------------------+
   |                              Project                                   |
   |   timebase: Rational      resolution: QSize     assetRoot: QDir        |
   |   undoStack: QUndoStack   stylePresets          pluginStates           |
   +-----------------------------------+------------------------------------+
                                       | owns
                                       v
   +------------------------------------------------------------------------+
   |                             Timeline                                    |
   |   std::vector<std::unique_ptr<Track>>  tracks_   <-- 無限レイヤー        |
   |   index 0 = 最背面 ... index N-1 = 最前面 (Z オーダー = vector 順)        |
   +---+--------------+---------------+---------------+---------------------+
       |              |               |               |
       v              v               v               v
  +---------+   +-----------+   +-----------+   +--------------+   +-------------+
  | Track   |   |  Track    |   |  Track    |   |   Track      |   |   Track     |
  | Video   |   |  Audio    |   |  Subtitle |   |  AiGenerated |   | Storyboard  |
  +----+----+   +-----+-----+   +-----+-----+   +------+-------+   +------+------+
                                                                          |
                                                            CutClip (演出指示。13章)
                                                            合成に参加しない
       |              |               |                |
   std::vector<std::shared_ptr<Clip>> clips_ (開始時刻でソート済み)
       |              |               |                |
       v              v               v                v
  +----------+  +----------+   +--------------+  +-------------------+
  |VideoClip |  |AudioClip |   |SubtitleClip  |  |AiPlaceholderClip  |
  |assetId   |  |assetId   |   |text/style/   |  | taskId            |
  |srcRange  |  |srcRange  |   |effectStack   |  | (生成完了後に置換) |
  +----------+  +----------+   +--------------+  +-------------------+


  ---------- 再生 / 描画 経路 --------------------------------------------

  AudioRenderEngine  ==(master clock: sample position)==>  PlaybackController
        ^                                                          |
        | pull PCM                                                 | frame N を要求
        |                                                          v
   +----+----------------+                             +-----------------------+
   | AudioClip / VST3    |                             |    RhiCompositor      |
   | チェーン + PDC       |                             |  レイヤーを下から合成 |
   +---------------------+                             +-----+-----+-----+-----+
                                                             |     |     |
                        +------------------------------------+     |     +--------+
                        v                                          v              v
              +-------------------+                   +-------------------+  +-----------+
              |   VideoEngine     |                   |  SubtitleEngine   |  |AviUtlHost |
              | FFmpeg + HW decode|                   | glyph atlas +     |  | (Win only)|
              | -> QRhiTexture    |                   | ISubtitleEffect   |  | filter    |
              +-------------------+                   +-------------------+  +-----------+


  ---------- AI 生成 経路 ------------------------------------------------

  StoryboardController --> StoryboardBatchJob     (13章: カット群を依存順に投入)
        |                        |
        |  RoleTrackResolver     |  ParamCascade / CutPromptComposer
        |  (役割 -> 配置先トラック) |  (Bible -> トラック -> カット -> 出力)
        v                        v
  AiController --> AiGenerationOrchestrator
                        |  (実行レーンごとの QThreadPool / QFuture)
                        +--> AiGenerationTask (Queued->Running->Cached)
                                  |
                                  +--> IGenerationProvider
                                  |       +-- OnnxLocalProvider   (DirectML/CUDA/CoreML)
                                  |       +-- RemoteHttpProvider  (ComfyUI / OpenAI互換)
                                  |       +-- SidecarProvider     (子プロセス + IPC)
                                  |
                                  v
                        生成物を .yave_cache/gen/<uuid>/ へ
                                  |
                                  v
                        CommitGeneratedAssetCommand (QUndoCommand)
                                  |
                                  v
                              Timeline へクリップ挿入
                              (cutRef があれば OutputBinding へ結果を書き戻す)


  ---------- プラグイン 経路 ----------------------------------------------

  PluginManager
     +-- Vst3Registry           --> Vst3Host  --> PluginWindow (HWND / NSView)
     +-- SubtitleEffectRegistry --> ISubtitleEffect 実装 (組み込み + 外部 .dll/.dylib)
     +-- AviUtlRegistry (Win)   --> AviUtlHost --> PluginWindow (HWND)
```

## 1.3 スレッドモデル

```
 [UI Thread] ---------------------------------------------------------------
   QML シーングラフ、ユーザー入力、Undo スタック操作、Timeline の構造変更。
   Timeline の構造(トラック/クリップの追加削除)を書き換えてよい唯一のスレッド。

 [Render Thread] -----------------------------------------------------------
   QRhi のコマンド構築と submit。Timeline は「読み取り専用スナップショット」
   (RenderSnapshot) 越しにのみ参照する。UI スレッドがフレーム毎にスナップショットを
   生成して渡す。これにより Timeline 本体にロックを持たせない。

 [Decode Worker Pool] ------------------------------------------------------
   QThreadPool。1 デコーダインスタンス = 1 タスク。前方先読み (look-ahead) と
   フレームキャッシュ充填を担当。結果は lock-free キュー経由で Render Thread へ。
   推奨スレッド数 = min(物理コア数, 同時可視ビデオトラック数 + 2)

 [Audio RT Thread] ---------------------------------------------------------
   OS が所有するリアルタイムコールバックスレッド (WASAPI / CoreAudio)。
   禁止事項: malloc / free、ロック取得、ファイル I/O、Qt シグナル発火、例外送出。
   Timeline へは触らない。事前に UI スレッドが構築した AudioRenderGraph
   (プレーンな POD グラフ) のみを読む。グラフ差し替えは RCU 方式のポインタ交換。

 [AI Worker Pool] ----------------------------------------------------------
   実行レーンごとに QThreadPool を分ける (7.4.3 / 13.6.4)。
     LocalGpu = 1                       ONNX / ローカル推論。VRAM のため 1 本
     LocalCpu = max(1, cores / 2)       前処理 / 後処理 / ffmpeg サブ処理
     Remote   = 6                       HTTP 通信。エンドポイント毎に追加制限
     Sidecar  = 1 (設定可)              子プロセス IPC
   優先度は Interactive > Batch。単発生成がバッチの後ろで飢えないようにする。
   完了通知は queued connection で UI スレッドへ。

 [Plugin GUI Thread] -------------------------------------------------------
   VST3 / AviUtl の GUI は UI スレッド上で動かす (両 SDK ともメインスレッド前提)。
   音声処理のみ Audio RT Thread 上。パラメータ受け渡しはロックフリーキュー。
```

### 1.3.1 スレッド間データ受け渡しの原則

| 経路 | 方式 |
|---|---|
| UI → Render | `RenderSnapshot` (値のコピー。1 フレーム分の描画に必要な情報のみ) |
| Decode → Render | SPSC ロックフリーリングバッファ (`FrameQueue`) |
| UI → Audio RT | RCU (`std::atomic<AudioRenderGraph*>` の差し替え + 遅延解放) |
| Audio RT → UI | ロックフリーリングバッファ (メーター値、再生位置) |
| AI Worker → UI | `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` |

> **`RenderSnapshot` を採る理由**: Timeline に読み書きロックを掛ける設計だと、
> UI スレッドでのドラッグ操作中に Render Thread が待たされ、プレビューがカクつく。
> 1 フレーム分の可視クリップ情報は数百バイト程度なのでコピーの方が安い。

## 1.4 ビルドターゲットと CMake 切り分け方針

### 1.4.1 ターゲット構成

| ターゲット | 種別 | 内容 |
|---|---|---|
| `yave_core` | STATIC | Timeline / Clip / Rational など。Qt Core のみに依存 |
| `yave_media` | STATIC | FFmpeg ラッパ、デコード / エンコード |
| `yave_render` | STATIC | QRhi 合成、シェーダ |
| `yave_audio` | STATIC | オーディオデバイス、ミキサ、PDC |
| `yave_subtitle` | STATIC | 字幕レイアウト / ラスタライズ / エフェクト |
| `yave_ai` | STATIC | 生成オーケストレータ、プロバイダ |
| `yave_plugin` | STATIC | VST3 ホスト、字幕エフェクトローダ、(Win) AviUtl ホスト |
| `yave_app` | EXECUTABLE | QML UI とエントリポイント |
| `yave_sdk` | INTERFACE | 字幕エフェクトプラグイン作者向け公開ヘッダのみ |
| `yave_tests` | EXECUTABLE | Qt Test |

### 1.4.2 プラットフォーム切り分けの原則

1. **`if(WIN32)` / `if(APPLE)` はソースの追加とライブラリのリンクにのみ使う。**
   コンパイル定義でロジックを分岐させるのは最小限にする。
2. **Windows 専用機能はオプション化し、既定値をプラットフォームで決める。**
   `option()` の既定値に直接プラットフォーム判定を書かず、一度変数に落としてから使う
   (ユーザーがキャッシュで OFF にできるようにするため)。
3. **AviUtl 関連は `yave_plugin` の中でソース単位で切り離す。**
   非 Windows ではファイル自体をコンパイル対象に入れない。`#ifdef` で全体を囲む方式は
   ヘッダの include 忘れやリンクエラーの温床になるため採らない。

### 1.4.3 ルート CMakeLists.txt の骨格

```cmake
cmake_minimum_required(VERSION 3.24)
project(YAVE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---- プラットフォーム既定値 ---------------------------------------------
if(WIN32)
    set(YAVE_AVIUTL_DEFAULT ON)
    set(YAVE_QSV_DEFAULT    ON)
    set(YAVE_CUDA_DEFAULT   ON)
    set(YAVE_VT_DEFAULT     OFF)
elseif(APPLE)
    set(YAVE_AVIUTL_DEFAULT OFF)
    set(YAVE_QSV_DEFAULT    OFF)
    set(YAVE_CUDA_DEFAULT   OFF)
    set(YAVE_VT_DEFAULT     ON)
endif()

option(YAVE_ENABLE_AVIUTL       "Enable AviUtl x64 plugin host (Windows only)" ${YAVE_AVIUTL_DEFAULT})
option(YAVE_ENABLE_QSV          "Enable Intel QuickSync"                       ${YAVE_QSV_DEFAULT})
option(YAVE_ENABLE_CUDA         "Enable NVDEC/NVENC"                           ${YAVE_CUDA_DEFAULT})
option(YAVE_ENABLE_VIDEOTOOLBOX "Enable VideoToolbox"                          ${YAVE_VT_DEFAULT})
option(YAVE_ENABLE_ONNX_LOCAL   "Enable local ONNX Runtime inference"          ON)
option(YAVE_BUILD_TESTS         "Build unit tests"                             ON)

# 非 Windows で AviUtl を要求されたら明示的に落とす(黙って無視しない)
if(YAVE_ENABLE_AVIUTL AND NOT WIN32)
    message(FATAL_ERROR
        "YAVE_ENABLE_AVIUTL is only supported on Windows. Configure with -DYAVE_ENABLE_AVIUTL=OFF.")
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Qml Network Concurrent ShaderTools LinguistTools)
qt_standard_project_setup(REQUIRES 6.6)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil swscale swresample avfilter)

add_subdirectory(third_party)
add_subdirectory(src)
if(YAVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

> AviUtl の分離実装の詳細は [12章 スニペット 1](12-snippets.md) を参照。

### 1.4.4 依存ライブラリの調達方針

| ライブラリ | Windows | macOS |
|---|---|---|
| Qt 6 | 公式インストーラ / aqt。`CMAKE_PREFIX_PATH` で指定 | 同左 |
| FFmpeg | vcpkg manifest (`ffmpeg[nvcodec,qsv,...]`) | Homebrew `ffmpeg` (VideoToolbox 有効) |
| VST3 SDK | `FetchContent` (Steinberg 公式リポジトリ) | 同左 |
| ONNX Runtime | vcpkg または公式 zip (DirectML 版) | 公式 pkg (CoreML EP 有効) |
| AviUtl SDK ヘッダ | `third_party/aviutl_sdk/` に同梱 (ヘッダのみ) | 使用しない |

`vcpkg.json` (manifest mode) をリポジトリルートに置き、Windows では
`-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake` で構成する。
macOS では `cmake/FindFFmpeg.cmake` が Homebrew の prefix を自動探索する。

### 1.4.5 ビルドコマンド例

Windows:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2019_64
```

macOS:

```bash
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt6);$(brew --prefix ffmpeg)" -DYAVE_ENABLE_AVIUTL=OFF
```

## 1.5 起動シーケンス

```
main()
 |
 +- QApplication 生成 (QGuiApplication ではない。QWidget 系のプラグイン窓を使うため)
 +- LanguageManager::instance().initialize()      … QSettings からロケール決定 + .qm ロード
 +- RhiBackendSelector::select()                  … D3D11 / D3D12 / Metal を決定
 +- PluginManager::instance().scanAsync()         … プラグイン走査をバックグラウンドで開始
 +- AudioRenderEngine::instance().openDevice()    … 既定デバイスをオープン
 +- QQmlApplicationEngine で MainWindow.qml をロード
 +- (プラグイン走査完了) -> QML へ signal -> ライブラリ表示更新
 +- コマンドライン引数にプロジェクトパスがあれば ProjectController::open()
 +- app.exec()
```

> プラグイン走査を同期で行わない理由: VST3 / AviUtl のスキャンは 1 プラグインあたり
> DLL ロードとファクトリ問い合わせを伴い、数十個入っている環境では起動が数秒単位で遅くなる。
> 走査中でも編集を開始できるようにする。

## 1.6 シャットダウンシーケンス

停止順序を誤ると解放済みリソースへのアクセスでクラッシュするため、順序を固定する。

```
1. AudioRenderEngine::stop()          … RT スレッドを先に止める(最優先)
2. PlaybackController::stop()
3. AiGenerationOrchestrator::cancelAll() + waitForDone(timeout=5s)
4. DecodeWorkerPool::shutdown()
5. RhiCompositor::releaseResources()  … Render Thread を join
6. PluginManager::unloadAll()         … VST3 -> terminate(), AviUtl -> FreeLibrary
7. Project の破棄
```

> 1 を最優先にする理由: オーディオ RT スレッドは `AudioRenderGraph` 経由で VST3 プラグインの
> `process()` を呼んでいる。プラグインを先に unload すると RT スレッドが解放済み関数を呼ぶ。
