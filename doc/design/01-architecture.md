# 1. 全体アーキテクチャ設計

[← 目次に戻る](../design.md)

---

## 1.1 レイヤ構造

上位レイヤは下位レイヤにのみ依存する。逆方向の依存はコールバック、リスナー、イベント通知経由に限定し、直接のモジュールインポートを作らない。

```
+-----------------------------------------------------------------------+
|  Application Layer                                                    |
|    apps/web: Vite SPA エントリ (index.html, main.tsx, coi-serviceworker)|
|    apps/electron: main/ (Node.js, window 管理, IPC) + preload/ (bridge)|
+-----------------------------------------------------------------------+
                                    |
+-----------------------------------------------------------------------+
|  Presentation Layer (packages/ui)                                     |
|    React 18 + CSS Modules                                             |
|    flexlayout-react (ドッキング & タブ可能な 8 パネル -- 1.7)         |
|    Zustand stores (UI 状態とドメインイベントのブリッジ)               |
+-----------------------------------------------------------------------+
                    |                                |
                    v                                v
+------------------------------------+   +------------------------------+
|  Engine Layer (packages/engine)    |   | Platform Adapter             |
|    render/ : WebGL2 Compositor     |   |   (packages/platform)        |
|    media/  : WebCodecs デコーダ/   |   |   PlatformHost インタフェース|
|              エンコーダ, demux     |   |   + web 実装 (FS Access/OPFS)|
|    audio/  : AudioWorklet, PDC     |   |   + electron 実装 (IPC 経由) |
|    ai/     : onnxruntime-web, HTTP |   +------------------------------+
+------------------------------------+                  ^
                    |                                   |
                    v                                   |
+------------------------------------+                  |
|  Core Layer (packages/core)        |                  |
|    Rational, TimeRange, Uuid       |                  |
|    Timeline, Track, Clip 派生群    |------------------+
|    Project, CommandStack, Commands |
+------------------------------------+
```

### 1.1.1 各パッケージの責務

| パッケージ | 責務 | 依存関係の制約 |
|---|---|---|
| `packages/core` | タイムライン、トラック、クリップ、時間、プロジェクト、Undo コマンド、AI トラック演出モデル | **DOM, React, Node.js API の import を完全禁止**。純粋な TypeScript のみ |
| `packages/io` | プロジェクト JSON のシリアライズ/デシリアライズ、SRT/VTT パーサ、スキーママイグレーション | `packages/core` のみに依存 |
| `packages/engine` | メディア demux、WebCodecs によるデコード/エンコード、WebGL2 レイヤー合成、AudioWorklet 音声処理、AI 推論 | `packages/core`, `packages/io`, `packages/platform` に依存 |
| `packages/platform` | `PlatformHost` の抽象インタフェース定義、および Web 実装・Electron preload クライアント実装 | `packages/core` のデータ型のみ依存可能 |
| `packages/plugin-sdk` | サードパーティ製 JS プラグイン作者向けの型定義公開パッケージ | 外部ライブラリ非依存 |
| `packages/ui` | React コンポーネント群、flexlayout-react パネル、Zustand store、i18n 設定 | `packages/core`, `packages/engine`, `packages/platform`, `packages/io` に依存 |
| `apps/web` | ブラウザ版の Vite ビルド、Service Worker 登録 | `packages/ui`, `packages/platform` を統合 |
| `apps/electron` | デスクトップ版の Electron メインプロセス・preload スクリプト | `packages/platform` の main 側 IPC ハンドラを実装 |

---

## 1.2 コンポーネント関連図

```
                          +---------------------------+
                          |     ProjectController     |
                          | (React / Zustand 経由操作)|
                          +------------+--------------+
                                       | owns
                                       v
   +-----------------------------------+------------------------------------+
   |                              Project                                   |
   |   timebase: Rational      canvasSize: {width, height}                  |
   |   commandStack: CommandStack   stylePresets      pluginStates          |
   |   library: LibraryStore   storyBible: StoryBible                       |
   +-----------------------------------+------------------------------------+
                                       | owns
                                       v
   +------------------------------------------------------------------------+
   |                             Timeline                                    |
   |   tracks: Track[]                        <-- 無限レイヤー               |
   |   index 0 = 最背面 ... index N-1 = 最前面 (Z オーダー = 配列インデックス順)|
   +---+--------------+---------------+---------------+---------------------+
       |              |               |               |
       v              v               v               v
  +---------+   +-----------+   +-----------+   +--------------+   +-------------+
  | Track   |   |  Track    |   |  Track    |   |   Track      |   |   Track     |
  | Video   |   |  Audio    |   |  Subtitle |   |  AiGenerated |   | Storyboard  |
  +----+----+   +-----+-----+   +-----+-----+   +------+-------+   +------+------+
       |              |               |                |                  |
       |              |               |                |       CutClip (演出指示。13章)
       |              |               |                |       合成に参加しない
       |              |               |                |                  |
   clips: Clip[] (start フレーム番号昇順でソート済み不変条件)             |
       |              |               |                |                  |
       v              v               v                v                  v
  +----------+  +----------+   +--------------+  +-------------------+ +---------+
  |VideoClip |  |AudioClip |   |SubtitleClip  |  |AiPlaceholderClip  | | CutClip |
  |assetId   |  |assetId   |   |text/style/   |  | taskId            | +---------+
  |sourceOff |  |sourceOff |   |effectStack   |  | (生成完了後に置換) |
  +----------+  +----------+   +--------------+  +-------------------+


  ---------- 再生 / 描画 経路 --------------------------------------------

  AudioEngine  ==(master clock: sample position via SAB / timestamp)==>  PlaybackController
        |                                                                      |
        | AudioWorklet が pull                                                 | frame N を要求
        v                                                                      v
   +----+----------------+                                           +-------------------+
   | AudioClip / WAM     |                                           | WebGlCompositor   |
   | チェーン + PDC       |                                           | レイヤーを下から合成|
   +---------------------+                                           +---+---+---+---+---+
                                                                         |   |   |   |
                        +------------------------------------------------+   |   |   +--------+
                        v                                                    v   v            v
              +-------------------+                                 +----------+ +------------+
              | VideoEngine       |                                 | Subtitle | | Transition |
              | WebCodecs Decoder |                                 | Engine   | | / Filter   |
              | (Worker プール)   |                                 | Canvas2D | | (GLSL ES3) |
              | -> VideoFrame     |                                 | -> Atlas | +------------+
              +-------------------+                                 +----------+


  ---------- AI 生成 経路 ------------------------------------------------

  StoryboardController --> StoryboardBatchJob     (13章: カット群を依存 DAG 順に投入)
        |                        |
        |  RoleTrackResolver     |  ParamCascade / CutPromptComposer
        |  (役割 -> 配置先トラック) |  (Bible -> トラック -> カット -> 出力)
        v                        v
  AiController --> AiOrchestrator
                        |  (実行レーン管理: LocalGpu / LocalCpu / Remote / Sidecar)
                        +--> AiGenerationTask (Queued -> Running -> Cached)
                                  |
                                  +--> IGenerationProvider
                                  |       +-- OnnxWebProvider    (onnxruntime-web / Worker)
                                  |       +-- RemoteHttpProvider (ComfyUI / OpenAI 互換 / fetch)
                                  |       +-- SidecarProvider    (Electron main 経由 child_process)
                                  |
                                  v
                        生成物を OPFS または userData キャッシュへ
                                  |
                                  v
                        CommitGeneratedAssetCommand (CommandStack)
                                  |
                                  v
                        Timeline へ実クリップ挿入
                        (cutRef があれば OutputBinding へ結果を書き戻す)


  ---------- プラグイン 経路 ----------------------------------------------

  PluginRegistry
     +-- SubtitleEffectRegistry --> ISubtitleEffect 実装 (JS ES module: 組み込み + 外部)
     +-- VideoFilterRegistry    --> GLSL シェーダ + パラメータハンドラ
     +-- TransitionRegistry     --> トランジションシェーダ
     +-- AudioModuleRegistry    --> 将来の Web Audio Modules (WAM 2.0)
```

---

## 1.3 スレッドモデル

ブラウザのシングルスレッド制約を回避し、60fps の滑らかな描画とリアルタイム音声を担保するため、役割ごとに実行コンテキストを分離する。

```
 [Main Thread] -------------------------------------------------------------
   React 18 レンダリング、ユーザー入力、CommandStack (Undo/Redo)、Timeline 構造管理。
   Timeline 構造を直接変更してよい唯一のスレッド。
   WebGL2 の描画コマンド (`gl.drawArrays` / `gl.drawElements`) を requestAnimationFrame で発行。

 [Decode Worker Pool] (2〜4 スレッド) ---------------------------------------
   mp4box.js / web-demuxer で demux、WebCodecs `VideoDecoder` でデコード。
   前方先読み (look-ahead) とフレームキャッシュ充填を担当。
   デコード済み `VideoFrame` は Transferable としてメインスレッドへ転送。

 [Waveform & Thumbnail Worker] (1〜2 スレッド) ------------------------------
   音声ファイルの事前デコード、ピーク計算、波形キャッシュ生成。
   動画素材の 1 秒地点サムネイル切り出し (OffscreenCanvas 経由)。

 [AI Worker] (1 スレッド) --------------------------------------------------
   onnxruntime-web によるローカル推論 (WebGPU または WASM バックエンド)。
   メインスレッドのフレームレートを一切落とさずにバックグラウンド推論を実行。

 [AudioWorklet Thread] (リアルタイム専用スレッド) ----------------------------
   Web Audio API の音声処理スレッド。128 サンプルごとに `process()` コールバック。
   禁止事項: ガベージコレクションを誘発するメモリアロケーション、重いループ、同期 fetch。
   Timeline には触らず、メインスレッドから送られたイミュータブルな `AudioRenderGraph` のみを評価。
   再生クロック (累積サンプル数) は `SharedArrayBuffer` の `Int32Array` アトミック変数で公開。

 [Electron Main Process] (デスクトップ版のみ) -------------------------------
   ネイティブダイアログ、ファイルシステムの任意パス読み書き、`safeStorage` 暗号化、
   CORS 制約のないバックエンド通信、ffmpeg 実行ファイルによるネイティブ書き出し。
```

### 1.3.1 スレッド間データ受け渡しの原則

| 経路 | 方式 |
|---|---|
| Main → Render (WebGL2) | `RenderSnapshot` (不変オブジェクトの shallow clone。1 フレーム分の描画情報のみ) |
| Decode Worker → Main | `postMessage([videoFrame], [videoFrame])` (Transferable によるゼロコピー受け渡し) |
| Main → AudioWorklet | `port.postMessage(audioGraphMessage)` (イミュータブルなグラフ記述の転送) |
| AudioWorklet → Main | `SharedArrayBuffer` 上のアトミック変数 (累積サンプル数)、または `port.postMessage` (レベルメーター) |
| Waveform Worker → Main | `postMessage(waveformArrayBuffer, [waveformArrayBuffer])` (Transferable) |
| Main ↔ Electron Main | IPC (`ipcRenderer.invoke` / `ipcMain.handle`) |

> **`RenderSnapshot` を採る理由**:
> タイムラインのデータツリーを直接 WebGL 合成ループが参照すると、ユーザーがタイムラインをドラッグ移動した瞬間にクリップの座標や個数が不整合を起こし、画面がチラつく。1 フレームに必要な可視レイヤーの情報だけを抽出したプレーンな不変オブジェクト (`RenderSnapshot`) を渡すことで、ロックフリーかつ安全に描画を継続できる。

---

## 1.4 ビルドターゲットと pnpm workspace

リポジトリは `pnpm workspace` による monorepo 構成とする。

```
/
├── apps/
│   ├── web/                     GitHub Pages 用 Vite SPA
│   └── electron/                Electron main / preload / electron-builder
└── packages/
    ├── core/                    ドメインロジック (UI/DOM/Node 非依存)
    ├── io/                      JSON / SRT / VTT 入出力
    ├── engine/                  WebCodecs, WebGL2, Web Audio, AI
    ├── platform/                PlatformHost インタフェースと実装
    ├── plugin-sdk/              サードパーティ向け JS プラグイン API
    └── ui/                      React コンポーネント, flexlayout-react, Zustand
```

### 1.4.1 パッケージ依存の有向グラフ

```
apps/web ───────+──> packages/ui ──+──> packages/engine ──+──> packages/core
                |                  |                      |
apps/electron ──+                  +──> packages/platform +──> packages/io ──> packages/core
                                   |                      |
                                   +──────────────────────+
packages/plugin-sdk (スタンドアロン。他パッケージを import しない)
```

循環参照は ESLint および TypeScript プロジェクト参照 (`references`) で禁止する。

---

## 1.5 起動シーケンス

### 1.5.1 Web 版 (GitHub Pages)

```
1. ブラウザが index.html をロード
2. coi-serviceworker が登録されているか確認
   - 未登録または crossOriginIsolated が false の場合:
     coi-serviceworker.js を Service Worker として登録し、ページを自動リロード
   - 既に crossOriginIsolated が true の場合:
     そのまま初期化処理へ進む
3. PlatformHost (Web 実装: WebPlatformHost) を生成
   - File System Access API, OPFS, WebCodecs, WebGL2 の利用可否を判定
4. i18n の初期化
   - localStorage の設定または navigator.languages から言語 (ja / en) を決定し、locales/*.json をロード
5. AudioContext を生成 (ユーザーの初回到達時は suspended 状態)
6. React アプリケーション (packages/ui) を #root にマウント
7. flexlayout-react の初期レイアウトを PlatformHost.settings (localStorage) から復元
8. 組み込みエフェクト・トランジション・フィルターのカタログを登録
9. ユーザーの初操作 (クリック等) で AudioContext.resume() を実行
```

### 1.5.2 Electron 版 (デスクトップ)

```
1. Electron main プロセス起動 (apps/electron/main/main.ts)
2. 単一インスタンスロックの確認 (app.requestSingleInstanceLock)
3. IPC ハンドラの登録 (PlatformHost の全メソッドに対応する ipcMain.handle)
4. BrowserWindow を生成
   - webPreferences: { contextIsolation: true, nodeIntegration: false, sandbox: true, preload: ... }
   - session.webRequest で COOP / COEP ヘッダを強制付与 (crossOriginIsolated を保証)
5. preload スクリプトが contextBridge.exposeInMainWorld('yaveHost', ...) を実行
6. レンダラープロセスが index.html をロード
7. ElectronPlatformHost が window.yaveHost をラップして生成
8. 以降は Web 版のステップ 4〜8 と同様に React UI を初期化
9. コマンドライン引数に *.yave のファイルパスがあれば自動で openProject を実行
```

---

## 1.6 シャットダウンシーケンス

リソースの解放漏れやファイル破損を防ぐため、以下の順序で終了処理を行う。

```
1. 編集中のプロジェクトに変更があるか確認 (CommandStack.isClean)
   - 未保存の場合:
     Web: beforeunload イベントでブラウザ標準ダイアログを表示
     Electron: dialog.showMessageBox で「保存しますか?」ダイアログを表示
2. 再生ループの停止 (PlaybackController.stop())
3. 実行中の AI タスクの安全なキャンセル (AiOrchestrator.cancelAll())
4. オーディオの停止と解放:
   - AudioContext.close()
5. WebGL2 リソースの解放:
   - テクスチャプール、FBO プール、シェーダプログラムの破棄
6. 全デコードフレームの確実なクローズ:
   - キャッシュ内のすべての VideoFrame に対して close() を呼ぶ
7. Web Worker プールの停止:
   - 各 Worker に対して worker.terminate() を発行
8. Electron main プロセスの終了 (app.quit())
```

---

## 1.7 UI パネル構成 (ドッキング / タブ)

### 1.7.1 タブとして設定可能なパネル

メインウィンドウは `flexlayout-react` によるドッキングエリアの集合として構成する。ユーザーがドック位置を自由に変更でき、**同一ドックエリアに複数入れた場合はタブグループになる**。

タブとして設定可能なパネルは以下の **8 種類に限定** する。この一覧が唯一の正であり、ここにないビューはタブ化の対象にしない。

| パネル ID (永続化キー) | 表示名 (ja / en) | 内容 | React コンポーネント |
|---|---|---|---|
| `mediaLibrary` | メディアライブラリ / Media Library | **ユーザーが取り込んだ素材** をフォルダで整理する (1.7.5)。サムネイル・使用箇所・オフライン検出 | `panels/MediaLibraryPanel.tsx` |
| `fileBrowser` | ファイルブラウザ / File Browser | ファイルシステムを直接辿る。未取り込み素材のプレビューと取り込み | `panels/FileBrowserPanel.tsx` |
| `console` | コンソール / Console | ログ出力、AI 生成・プラグインの警告/エラー一覧 | `panels/ConsolePanel.tsx` |
| `inspector` | インスペクタ / Inspector | 選択中のクリップ / トラック / エフェクトのパラメータ編集 | `panels/InspectorPanel.tsx` |
| `effectLibrary` | エフェクトライブラリ / Effect Library | **組み込み / プラグインが提供するカタログ** をフォルダで整理する (1.7.5)。トランジション / タイトル / 字幕 / フィルター / エフェクト の 5 カテゴリ | `panels/EffectLibraryPanel.tsx` |
| `timeline` | タイムライン / Timeline | タイムライン編集ビュー | `panels/TimelinePanel.tsx` |
| `aiTasks` | AI タスク一覧 / AI Tasks | AI 生成タスクの進捗・エラー・取り消し。バッチ単位でグループ化 ([13.11.5](13-ai-track.md)) | `panels/AiTaskListPanel.tsx` |
| `storyboardBoard` | 絵コンテボード / Storyboard | AIトラックのカットを絵コンテ表として一覧・編集 ([13.11.1](13-ai-track.md)) | `panels/StoryboardBoardPanel.tsx` |

タブ化の対象外となる主なビュー:

| ビュー | 扱い |
|---|---|
| プレビュー | 常時表示の固定領域 (または中央ドック固定)。閉じることも別ウィンドウへ分離することもできない |
| カットインスペクタ / 字幕インスペクタ | 独立パネルにせず、選択対象に応じて `inspector` パネル内へ出し分ける |
| 各種ダイアログ (`AiGenerateDialog`, `StoryboardPlanDialog` 等) | モーダル / モードレスの React ダイアログ (Portal 描画) |

### 1.7.2 ドッキングとタブの規則

| 規則 | 内容 |
|---|---|
| インスタンスは 1 パネル 1 個 | 同じパネルを 2 箇所に同時に置くことはできない。既に開いているパネルを再度開く操作は、そのパネルへのフォーカス移動として扱う |
| 配置先 | flexlayout-react の各分割エリア (left, right, bottom, center)、またはポップアウトウィンドウ |
| タブ化 | 同一エリアに 2 つ以上入れると自動的にタブグループになる。タブ順はドラッグで入れ替えられる |
| 閉じる | 8 種すべて閉じられる。**タイムラインも閉じられる**。復帰は「表示」メニューから行う |
| パネル ID | 永続化キーであり翻訳しない。表示名のみ `t()` を通す ([10章](10-i18n.md)) |
| 最小サイズ | 各パネルコンポーネントは min-width / min-height を指定し、エリア縮小の下限とする |

> **タイムラインをタブ扱いにする理由**:
> 絵コンテ作業やアセット整理の局面では、タイムラインより一覧系パネル (絵コンテボードやライブラリ) を広く使いたい。タイムラインだけを固定領域にすると「畳めるが消せない」中途半端な状態が残り、他パネルと規則が二重化する。

### 1.7.3 レイアウトの永続化

パネル配置・タブ構成・各エリアのサイズは、**アプリケーション固有の設定 (`PlatformHost.settings` の `ui.layout.*`) に保存し、プロジェクト JSON には含めない**。

> **理由**:
> プロジェクトファイルは Web / Electron、および異なる画面解像度の環境間で持ち運ぶ ([design.md §3.4](../design.md))。UI レイアウトはマシン側の属性であってプロジェクトの属性ではない。

保存キー構造:
```
ui.layout.version           … レイアウトスキーマ版。不一致時は既定レイアウトへフォールバック
ui.layout.model             … flexlayout-react の JSON 表現文字列
ui.layout.closedPanels      … 閉じているパネル ID の配列
```

### 1.7.4 既定レイアウト

flexlayout-react の既定レイアウト構成:

| エリア | パネル構成 |
|---|---|
| 左上 (left-top) | `mediaLibrary` / `fileBrowser` (タブグループ、既定は `mediaLibrary`) |
| 左下 (left-bottom) | `effectLibrary` |
| 中央 (center) | プレビュー (固定) |
| 右 (right) | `inspector` / `aiTasks` (タブグループ、既定は `inspector`) |
| 下 (bottom) | `timeline` / `storyboardBoard` / `console` (タブグループ、既定は `timeline`) |

> **2 つのライブラリを既定で同時に見せる理由**:
> 素材の取り込みと、効果の適用は編集中に交互に起きる。同じタブグループへ入れると毎回タブを往復することになるため、既定では左を上下に分ける。

### 1.7.5 ライブラリパネルの共通仕様

`mediaLibrary` と `effectLibrary` は同一のライブラリ UI 基盤 (`packages/ui/src/panels/library/LibraryComponent.tsx`) を担当カテゴリだけ変えて 2 インスタンス使う。左にフォルダツリー、右にアイテム一覧を置く。

| パネル | 担当カテゴリ | アイテムの供給元 |
|---|---|---|
| `mediaLibrary` | メディア (動画/音声/画像) | `project.library` (ユーザーが取り込んだ素材) |
| `effectLibrary` | トランジション / タイトル / 字幕 / フィルター / エフェクト | 組み込みカタログ + プラグイン走査結果 ([8章](08-plugin-host.md)) |

> **2 枚に分ける理由**:
> 前者は**プロジェクトが所有する可変データ**で、内容もフォルダ構成もプロジェクトごとに異なる。後者は**アプリが所有する固定カタログ**で、全プロジェクト共通である。所有者も永続化先も異なるものを 1 つの木に混ぜると破綻する。

#### フォルダ操作
- **作成**: 任意の階層に作成可能。同一親の下で名前は一意 (衝突時は `名前 (2)`)。
- **改名**: `F2` またはコンテキストメニュー。カテゴリ直下のルートは改名不可。
- **削除**: `Delete`。**中のアイテムは親フォルダへ移動し、アイテム自体は消さない**。
- **移動**: アイテムもフォルダもドラッグ & ドロップで移動可能。カテゴリをまたぐ移動は禁止。

#### 表示モード
- **一覧 (List)**: 名前のみ
- **小アイコン (SmallIcons)**: 1 行表示
- **大アイコン (Grid)**: グリッド表示 (32〜160px スライダー変更対応)
- **詳細 (Details)**: 名前 / 種別 / 尺 / 解像度 / コーデック

#### アイコンの解決順序
1. ユーザーが割り当てたカスタムアイコン
2. サムネイル (Worker で生成した 1 秒地点の画像)
3. 組み込み SVG アイコン
4. 種別プレースホルダ (単色 + 頭文字)

#### タイムラインへのドラッグ & ドロップ
HTML5 Drag and Drop API を使用。ドラッグ時の MIME タイプは `application/x-yave-library-item` (JSON 文字列: `{ category, itemId, assetId, name }`)。
受け入れ時の処理はすべて `Command` として `CommandStack` に積む。

| ドロップ先 | 受け取るカテゴリ | 動作 |
|---|---|---|
| トラックの空き | メディア | 対応するクリップ (`VideoClip`, `AudioClip`, `ImageClip`) を追加 |
| 映像トラックの空き | タイトル | `TitleClip` を生成 ([3.11](03-timeline-render.md)) |
| 字幕トラックの空き | 字幕 | `SubtitleClip` を生成 (スタイルプリセット適用) |
| クリップの上 | フィルター | そのクリップのフィルタースタックへ追加 ([3.9](03-timeline-render.md)) |
| 字幕 / タイトルクリップの上 | エフェクト | 字幕エフェクトスタックへ追加 ([6章](06-subtitle-engine.md)) |
| クリップ境界 (±12px) | トランジション | 境界にトランジションを配置 ([3.10](03-timeline-render.md)) |
| 上記以外の組み合わせ | — | ドロップ不可カーソルを表示し、理由を `console` パネルへ 1 行出力 |

---

## 1.8 PlatformHost 設計

Web と Electron の環境差異はすべて `PlatformHost` インタフェースの 2 実装に閉じ込める。UI、core、engine 層は `PlatformHost` のみを参照する。

```ts
// packages/platform/src/PlatformHost.ts
export interface ProjectHandle {
  readonly kind: 'file-system-handle' | 'electron-path' | 'memory-zip';
  readonly name: string;
}

export interface AssetSource {
  readonly id: string;
  readonly name: string;
  readonly pathOrUri: string;
  readonly file?: File;
}

export interface KeyValueStore {
  get(key: string): Promise<string | null>;
  set(key: string, value: string): Promise<void>;
  delete(key: string): Promise<void>;
  keys(): Promise<string[]>;
}

export interface SecretStore {
  getSecret(key: string): Promise<string | null>;
  setSecret(key: string, secret: string): Promise<void>;
  deleteSecret(key: string): Promise<void>;
}

export interface PlatformCapabilities {
  readonly nativeFileSystem: boolean;    // 任意パスの読み書き
  readonly crossOriginIsolated: boolean; // SharedArrayBuffer 可否
  readonly nativeFfmpeg: boolean;        // Electron 同梱 ffmpeg 利用可否
  readonly corsFreeFetch: boolean;       // Electron main 経由の CORS フリー fetch
}

export interface PlatformHost {
  readonly kind: 'web' | 'electron';
  readonly capabilities: PlatformCapabilities;

  // プロジェクトファイル I/O
  openProject(): Promise<{ handle: ProjectHandle; content: string } | null>;
  saveProject(handle: ProjectHandle, json: string): Promise<void>;
  saveProjectAs(json: string, suggestedName: string): Promise<ProjectHandle | null>;

  // アセット管理
  pickMediaFiles(): Promise<AssetSource[]>;
  readAsset(handle: ProjectHandle, relativePath: string): Promise<Blob>;
  resolveRelative(handle: ProjectHandle, source: AssetSource): string | null;

  // 書き出し
  createExportSink(suggestedName: string): Promise<WritableStream<Uint8Array>>;

  // 設定・秘密情報
  readonly settings: KeyValueStore;
  readonly secrets: SecretStore;

  // ネットワーク
  fetch(input: RequestInfo | URL, init?: RequestInit): Promise<Response>;

  // ウィンドウ・システム制御
  setTitle(title: string): void;
  onBeforeClose(handler: () => Promise<boolean>): void;
}
```

### 1.8.1 Web と Electron の実装差分

| 項目 | Web (WebPlatformHost) | Electron (ElectronPlatformHost) |
|---|---|---|
| プロジェクト保存 | File System Access API の `FileSystemFileHandle.createWritable()` (完了時に原子的置換)。非対応環境は zip ダウンロード | main プロセス経由で一時ファイル保存 → `fs.rename` (原子的) |
| アセット読み込み | `FileSystemDirectoryHandle.getFileHandle()` または OPFS | Node.js `fs.promises.readFile` |
| パス解決 | ディレクトリハンドル基準で相対化。別起点は OPFS / プロジェクト配下へコピー提案 | 同一ドライブなら相対パス化、別ドライブならプロジェクト配下へ収集提案 |
| 書き出し先 | `showSaveFilePicker()` によるストリーム書き込み | `dialog.showSaveDialog` + `fs.createWriteStream` |
| 設定保存 | `localStorage` + IndexedDB | `%APPDATA%/YAVE/settings.json` (Node.js `fs`) |
| 秘密情報 (API キー) | IndexedDB (平文相当である旨を UI で警告) | Electron `safeStorage` API で OS 暗号化 |
| 外部 API 通信 | ブラウザ `fetch` (相手側サーバーの CORS 許可が必須) | main プロセス経由の `net.fetch` (CORS 制約なし) |
| crossOriginIsolated | `coi-serviceworker` による Service Worker リロードで付与 | BrowserWindow レスポンスヘッダで付与 |
