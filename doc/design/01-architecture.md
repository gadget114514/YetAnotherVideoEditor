# 1. 全体アーキテクチャ設計

[← 目次に戻る](../design.md)

---

## 1.1 レイヤ構造

上位レイヤは下位レイヤにのみ依存する。逆方向の依存は Qt のシグナル / コールバック
インタフェース経由に限定し、コンパイル時依存を作らない。

```
+-----------------------------------------------------------------------+
|  Presentation Layer                                                    |
|    QML (Preview 固定 + ドッキング/タブ可能な 8 パネル -- 1.7)            |
|    QWidget (PluginWindow のみ -- ネイティブ埋め込みが必要なため)         |
+-----------------------------------------------------------------------+
                    | Q_PROPERTY / Q_INVOKABLE / signals
+-----------------------------------------------------------------------+
|  Controller Layer  (QObject)                                           |
|    ProjectController   EditController   PlaybackController             |
|    AiController        StoryboardController  PluginController          |
|    LanguageManager     PanelLayoutController (1.7.3)                   |
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

## 1.7 UI パネル構成 (ドッキング / タブ)

### 1.7.1 タブとして設定可能なパネル

`MainWindow.qml` はドックエリアの集合として構成する。ユーザーがドック位置を変更でき、
**同一ドックエリアに複数入れた場合はタブグループになる**。

タブとして設定可能なパネルは以下の **8 種類に限定**する。この一覧が唯一の正であり、
ここにないビューはタブ化の対象にしない。

| パネル ID (永続化キー) | 表示名 (ja / en) | 内容 | QML |
|---|---|---|---|
| `mediaLibrary` | メディアライブラリ / Media Library | **ユーザーが取り込んだ素材**をフォルダで整理する (1.7.5)。サムネイル・使用箇所・オフライン検出 | `library/LibraryPanel.qml` |
| `fileBrowser` | ファイルブラウザ / File Browser | ファイルシステムを直接辿る。未取り込み素材のプレビューと取り込み | `panels/FileBrowserPanel.qml` |
| `console` | コンソール / Console | `QLoggingCategory` のカテゴリ別ログ、AI 生成・プラグインの警告/エラー | `panels/ConsolePanel.qml` |
| `inspector` | インスペクタ / Inspector | 選択中のクリップ / トラック / エフェクトのパラメータ編集 | `inspector/InspectorPanel.qml` |
| `effectLibrary` | エフェクトライブラリ / Effect Library | **組み込み / プラグインが提供するカタログ**をフォルダで整理する (1.7.5)。トランジション / タイトル / 字幕 / フィルター / エフェクト の 5 カテゴリ | `library/LibraryPanel.qml` |
| `timeline` | タイムライン / Timeline | タイムライン編集ビュー | `timeline/TimelineView.qml` |
| `aiTasks` | AI タスク一覧 / AI Tasks | AI 生成タスクの進捗・エラー・取り消し。バッチ単位でグループ化 ([13.11.5](13-ai-track.md)) | `ai/AiTaskListPanel.qml` |
| `storyboardBoard` | 絵コンテボード / Storyboard | AIトラックのカットを絵コンテ表として一覧・編集 ([13.11.1](13-ai-track.md)) | `ai/StoryboardBoardPanel.qml` |

タブ化の対象外となる主なビュー:

| ビュー | 扱い |
|---|---|
| プレビュー | 常時表示の固定領域。閉じることもタブへ入れることもできない (再生の主目的であるため) |
| カットインスペクタ (`CutInspector.qml`) / 字幕インスペクタ (`SubtitleInspector.qml`) | 独立パネルにせず、選択対象に応じて `inspector` パネル内へ出し分ける |
| 各種ダイアログ (`AiGenerateDialog` / `StoryboardPlanDialog` / `BatchGenerateDialog` ほか) | モーダル / モードレスのウィンドウ |
| プラグイン窓 | `QWidget` によるネイティブ埋め込み ([8章](08-plugin-host.md))。QML のドック体系に載せない |

### 1.7.2 ドッキングとタブの規則

| 規則 | 内容 |
|---|---|
| インスタンスは 1 パネル 1 個 | 同じパネルを 2 箇所に同時に置くことはできない。既に開いているパネルを再度開く操作は、そのパネルへのフォーカス移動として扱う |
| 配置先 | 左上 / 左下 / 右 / 下 / 中央 の各ドックエリア、またはフローティングウィンドウ |
| タブ化 | 同一ドックエリアに 2 つ以上入れると自動的にタブグループになる。タブ順はドラッグで入れ替えられる |
| 閉じる | 8 種すべて閉じられる。**タイムラインも閉じられる**。復帰は「表示」メニュー(パネル ID ごとのチェック項目)から行う |
| パネル ID | 永続化キーであり翻訳しない。表示名のみ `qsTr()` を通す ([10章](10-i18n.md)) |
| 最小サイズ | 各パネルは `minimumWidth` / `minimumHeight` を持ち、ドックエリアの縮小はそれを下限とする |

> **タイムラインをタブ扱いにする理由**: 絵コンテ作業やアセット整理の局面では、
> タイムラインより一覧系パネルを広く使いたい。タイムラインだけを固定領域にすると
> 「畳めるが消せない」中途半端な状態が残り、他パネルと規則が二重化する。

### 1.7.3 レイアウトの永続化

パネル配置・タブ構成・各ドックエリアのサイズは、**アプリケーション固有の設定
(`QSettings` の `ui/layout/*`) に保存し、プロジェクト JSON には含めない**。
永続化と復元は `PanelLayoutController` が担当する。

> **理由**: プロジェクトファイルは Windows / macOS 間で持ち運ぶ ([design.md §3.4](../design.md))。
> 画面解像度もマルチモニタ構成も異なる環境へ UI 配置を持ち込むと、パネルが画面外に出る等の
> 復元不能な状態を生む。UI 状態はマシン側の属性であってプロジェクトの属性ではない。

保存キーの形は次のとおり。未知のパネル ID (将来の追加・削除) は読み込み時に無視し、
既定レイアウトの値で補う。

```
ui/layout/version           … レイアウトスキーマ版。不一致なら既定レイアウトへフォールバック
ui/layout/docks/<area>      … そのエリアに入るパネル ID の配列 (タブ順)
ui/layout/docks/<area>/size … エリアのピクセルサイズ
ui/layout/active/<area>     … タブグループ内でアクティブなパネル ID
ui/layout/floating/<panelId>… フローティング時のジオメトリ
ui/layout/closed            … 閉じているパネル ID の配列
```

### 1.7.4 既定レイアウト

| ドックエリア | パネル |
|---|---|
| 左上 | `mediaLibrary` / `fileBrowser` (タブグループ、既定は `mediaLibrary`) |
| 左下 | `effectLibrary` |
| 中央 | プレビュー (固定領域) |
| 右 | `inspector` / `aiTasks` (タブグループ、既定は `inspector`) |
| 下 | `timeline` / `storyboardBoard` / `console` (タブグループ、既定は `timeline`) |

> **2 つのライブラリを既定で同時に見せる理由**: 素材の取り込みと、効果の適用は
> 編集中に交互に起きる。同じタブグループへ入れると毎回タブを往復することになるため、
> 既定では左を上下に分ける。1 枚で足りるユーザーは片方を閉じるか同じエリアへまとめられる。

「表示 > レイアウトを既定に戻す」で、`ui/layout/*` を破棄してこの構成へ復帰できる。

### 1.7.5 ライブラリパネルの共通仕様

`mediaLibrary` と `effectLibrary` は**同一の QML 実装 (`library/LibraryPanel.qml`) を
担当カテゴリだけ変えて 2 インスタンス**使う。Windows エクスプローラに倣い、
左にフォルダツリー、右にアイテムビューを置く。

| パネル | 担当カテゴリ | アイテムの供給元 |
|---|---|---|
| `mediaLibrary` | メディア | `AssetLibrary` (ユーザーが取り込んだ素材) |
| `effectLibrary` | トランジション / タイトル / 字幕 / フィルター / エフェクト | 組み込みテーブル + プラグイン走査結果 ([8章](08-plugin-host.md)) |

> **2 枚に分ける理由**: 前者は**プロジェクトが所有する可変のデータ**で、内容も
> フォルダ構成もプロジェクトごとに違う。後者は**アプリが所有する固定のカタログ**で、
> 全プロジェクトで同じものが並ぶ。所有者も永続化先も寿命も違うものを 1 つの木に
> 混ぜると、「プロジェクトを開き直したらエフェクトの整理も消えた」類の事故になる。

#### フォルダ

| 操作 | 規則 |
|---|---|
| 作成 | 任意の階層に作れる。同一親の下で名前は一意 (衝突時は `名前 (2)`) |
| 名前の変更 | `F2` またはコンテキストメニュー。カテゴリ直下のルート 6 種は改名できない |
| 削除 | `Delete`。**中のアイテムは親フォルダへ移動し、アイテム自体は消さない**。サブフォルダも同様に親へ繰り上げる |
| 移動 | アイテムもフォルダも D&D で移動できる。カテゴリをまたぐ移動は禁止 |

> **削除でアイテムを消さない理由**: カタログ側の組み込みアイテムは「フォルダの中身」
> ではなく**フォルダに割り当てられているだけ**であり、消す対象が存在しない。
> メディア側だけ「フォルダを消すと素材も消える」挙動にすると規則が二重化する。
> 素材そのものの削除はアイテムに対する明示的な操作 (`Delete` をアイテムに対して実行) とする。

#### 表示モード

| モード | 内容 |
|---|---|
| 一覧 (List) | 名前のみ。列方向に折り返す |
| 小アイコン (SmallIcons) | 小アイコン + 名前の 1 行 |
| 大アイコン (Grid) | アイコン + 名前のグリッド |
| 詳細 (Details) | 名前 / 種別 / 尺 の列 |

アイコンサイズは 32〜160px のスライダーと `Ctrl` + ホイールで変更でき、
大アイコン / 小アイコンモードに効く。モードとサイズは**パネルごとに**保持する。

#### アイコンの解決順序

```
1. ユーザーが割り当てたアイコン (ui/library/icons/<itemId>)
2. サムネイル (image://yave-thumb/<assetId>) … 動画 / 画像アセットのみ
3. 組み込み SVG (qrc の種別アイコン)
4. 種別プレースホルダ (単色 + 頭文字)
```

サムネイルは**先頭フレームではなく 1 秒地点**を優先して取り、失敗したら先頭へ落とす
(先頭が黒フレームやフェードインで、一覧の見分けがつかない素材が多いため)。
生成はデコードワーカー上で非同期に行い、UI スレッドを止めない。
ユーザーによるアイコンの割り当ては全カテゴリで可能とし、`QSettings` に保存する。

#### 永続化

| 対象 | 保存先 | 理由 |
|---|---|---|
| メディアのフォルダ構成とアイテムの所属 | プロジェクト JSON の `library` ([9章](09-project-io.md)) | プロジェクトの内容そのもの。共同編集者にも引き継がれるべき |
| その他 5 カテゴリのフォルダ構成 | `QSettings` `ui/library/<category>/folders` | アプリ側のカタログの整理であり、プロジェクトに属さない |
| 表示モード / アイコンサイズ / アイコン割り当て | `QSettings` `ui/library/*` | UI 状態 (1.7.3 と同じ理由) |

#### タイムラインへのドラッグ & ドロップ

ドラッグ時の MIME は `yave/library-item` (JSON: `category` / `itemId` / `assetId` / `name`)。
メディアのみ後方互換で `yave/asset-id` も併せて載せる。
ドロップの受け口ごとの動作は次のとおりで、**すべて `QUndoCommand` として積む** (3.2)。

| ドロップ先 | 受け取るカテゴリ | 動作 |
|---|---|---|
| トラックの空き | メディア | アセットクリップを追加 |
| 映像トラックの空き | タイトル | `TitleClip` を生成 ([3.9](03-timeline-render.md)) |
| 字幕トラックの空き | 字幕 | `SubtitleClip` を生成 (スタイルプリセット適用) |
| クリップの上 | フィルター | そのクリップのフィルタースタックへ追加 ([3.9](03-timeline-render.md)) |
| 字幕 / タイトルクリップの上 | エフェクト | 字幕エフェクトスタックへ追加 ([6章](06-subtitle-engine.md)) |
| クリップ境界 (±12px) | トランジション | 境界にトランジションを置く ([3.10](03-timeline-render.md)) |
| 上記以外の組み合わせ | — | ドロップ不可カーソル。理由を `console` パネルへ 1 行出す |

編集ロジックは QML に持たせず、`EditController::dropLibraryItem()` に集約する。
QML 側は「どこへ落ちたか」だけを伝える。
