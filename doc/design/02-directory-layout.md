# 2. ディレクトリ構成

[← 目次に戻る](../design.md)

---

## 2.1 全体構造

YAVE は `pnpm workspace` を用いた monorepo 構成を採用する。

```
/
├── apps/
│   ├── web/                     Vite エントリ (ブラウザ / GitHub Pages 向け)
│   │   ├── index.html
│   │   ├── src/
│   │   │   ├── main.tsx
│   │   │   └── coi-serviceworker.ts
│   │   ├── vite.config.ts
│   │   └── tsconfig.json
│   │
│   └── electron/                Electron エントリ (デスクトップ向け)
│       ├── main/
│       │   ├── main.ts
│       │   ├── window.ts
│       │   ├── menu.ts
│       │   └── ipc/
│       │       ├── projectIpc.ts
│       │       ├── assetIpc.ts
│       │       ├── secretsIpc.ts
│       │       └── dialogIpc.ts
│       ├── preload/
│       │   └── preload.ts
│       ├── electron-builder.yml
│       ├── vite.config.ts
│       └── tsconfig.json
│
├── packages/
│   ├── core/                    ドメイン層 (UI・DOM・Node 非依存)
│   │   ├── src/
│   │   │   ├── time/
│   │   │   │   ├── Rational.ts
│   │   │   │   └── TimeRange.ts
│   │   │   ├── id/
│   │   │   │   └── Uuid.ts
│   │   │   ├── model/
│   │   │   │   ├── Clip.ts
│   │   │   │   ├── VideoClip.ts
│   │   │   │   ├── AudioClip.ts
│   │   │   │   ├── ImageClip.ts
│   │   │   │   ├── ColorClip.ts
│   │   │   │   ├── TitleClip.ts
│   │   │   │   ├── SubtitleClip.ts
│   │   │   │   ├── AiPlaceholderClip.ts
│   │   │   │   ├── CutClip.ts             (13章 AI トラック)
│   │   │   │   ├── Track.ts
│   │   │   │   ├── Timeline.ts
│   │   │   │   ├── Project.ts
│   │   │   │   ├── StoryBible.ts
│   │   │   │   ├── Transition.ts
│   │   │   │   └── VideoFilter.ts
│   │   │   ├── snapshot/
│   │   │   │   └── RenderSnapshot.ts
│   │   │   ├── commands/
│   │   │   │   ├── Command.ts
│   │   │   │   ├── CommandStack.ts
│   │   │   │   ├── AddTrackCommand.ts
│   │   │   │   ├── RemoveTrackCommand.ts
│   │   │   │   ├── ReorderTrackCommand.ts
│   │   │   │   ├── AddClipCommand.ts
│   │   │   │   ├── MoveClipCommand.ts
│   │   │   │   ├── TrimClipCommand.ts
│   │   │   │   ├── SplitClipCommand.ts
│   │   │   │   ├── RippleDeleteCommand.ts
│   │   │   │   ├── AddFilterCommand.ts
│   │   │   │   ├── AddTransitionCommand.ts
│   │   │   │   ├── CutCommands.ts
│   │   │   │   └── CommitGeneratedAssetCommand.ts
│   │   │   └── ai/
│   │   │       ├── AiGenerationParams.ts
│   │   │       └── OutputBinding.ts
│   │   ├── test/
│   │   │   ├── rational.test.ts
│   │   │   ├── timeline.test.ts
│   │   │   └── commands.test.ts
│   │   ├── tsconfig.json
│   │   └── package.json
│   │
│   ├── io/                      プロジェクト保存・ファイルフォーマット
│   │   ├── src/
│   │   │   ├── ProjectSerializer.ts
│   │   │   ├── SchemaMigration.ts
│   │   │   ├── JsonKeys.ts
│   │   │   ├── EnumMapping.ts
│   │   │   ├── PathResolver.ts
│   │   │   ├── srt/
│   │   │   │   ├── SrtParser.ts
│   │   │   │   └── SrtWriter.ts
│   │   │   ├── vtt/
│   │   │   │   └── VttWriter.ts
│   │   │   └── archive/
│   │   │       └── YavezArchive.ts       (.yavez zip 読み書き)
│   │   ├── schema/
│   │   │   └── project.v4.schema.json
│   │   ├── test/
│   │   │   ├── projectserializer.test.ts
│   │   │   └── srtparser.test.ts
│   │   ├── tsconfig.json
│   │   └── package.json
│   │
│   ├── engine/                  メディア・描画・音声・AI 実行
│   │   ├── src/
│   │   │   ├── media/
│   │   │   │   ├── demux/
│   │   │   │   │   ├── Mp4Demuxer.ts
│   │   │   │   │   └── WebmDemuxer.ts
│   │   │   │   ├── workers/
│   │   │   │   │   ├── decode.worker.ts
│   │   │   │   │   ├── waveform.worker.ts
│   │   │   │   │   └── thumbnail.worker.ts
│   │   │   │   ├── FrameCache.ts
│   │   │   │   ├── VideoDecoderPool.ts
│   │   │   │   ├── AudioDecoderPool.ts
│   │   │   │   ├── ProxyGenerator.ts     (ffmpeg.wasm)
│   │   │   │   └── export/
│   │   │   │       ├── ExportJob.ts
│   │   │   │       └── MuxerBridge.ts
│   │   │   ├── render/
│   │   │   │   ├── WebGlCompositor.ts
│   │   │   │   ├── TexturePool.ts
│   │   │   │   ├── FboPool.ts
│   │   │   │   ├── LayerPass.ts
│   │   │   │   ├── FilterPass.ts
│   │   │   │   ├── TransitionPass.ts
│   │   │   │   └── shaders/
│   │   │   │       ├── fullscreen.vert.glsl
│   │   │   │       ├── layer_blend.frag.glsl
│   │   │   │       ├── filter_color.frag.glsl
│   │   │   │       ├── filter_blur.frag.glsl
│   │   │   │       └── transition.frag.glsl
│   │   │   ├── audio/
│   │   │   │   ├── AudioEngine.ts
│   │   │   │   ├── AudioClock.ts
│   │   │   │   ├── DelayCompensator.ts
│   │   │   │   ├── worklets/
│   │   │   │   │   └── mixer.worklet.ts
│   │   │   │   └── MeterBridge.ts
│   │   │   └── ai/
│   │   │       ├── AiOrchestrator.ts
│   │   │       ├── GenerationCache.ts
│   │   │       ├── workers/
│   │   │       │   └── onnx.worker.ts
│   │   │       └── providers/
│   │   │           ├── OnnxWebProvider.ts
│   │   │           ├── RemoteHttpProvider.ts
│   │   │           └── SidecarProvider.ts (Electron only)
│   │   ├── test/
│   │   │   ├── delaycompensator.test.ts
│   │   │   └── audiograph.test.ts
│   │   ├── tsconfig.json
│   │   └── package.json
│   │
│   ├── platform/                PlatformHost 定義とアダプタ
│   │   ├── src/
│   │   │   ├── PlatformHost.ts
│   │   │   ├── web/
│   │   │   │   ├── WebPlatformHost.ts
│   │   │   │   ├── FileSystemAccessAdapter.ts
│   │   │   │   └── OpfsStorage.ts
│   │   │   └── electron-renderer/
│   │   │       ├── ElectronPlatformHost.ts
│   │   │       └── IpcClient.ts
│   │   ├── tsconfig.json
│   │   └── package.json
│   │
│   ├── plugin-sdk/              サードパーティ向け JS プラグイン公開 API
│   │   ├── src/
│   │   │   ├── index.ts
│   │   │   ├── manifest.ts
│   │   │   ├── subtitleEffect.ts
│   │   │   ├── videoFilter.ts
│   │   │   └── parameterSchema.ts
│   │   ├── tsconfig.json
│   │   └── package.json
│   │
│   └── ui/                      React UI コンポーネント群
│       ├── src/
│       │   ├── App.tsx
│       │   ├── layout/
│       │   │   └── FlexLayoutShell.tsx
│       │   ├── panels/
│       │   │   ├── MediaLibraryPanel.tsx
│       │   │   ├── FileBrowserPanel.tsx
│       │   │   ├── ConsolePanel.tsx
│       │   │   ├── InspectorPanel.tsx
│       │   │   ├── EffectLibraryPanel.tsx
│       │   │   ├── TimelinePanel.tsx
│       │   │   ├── AiTaskListPanel.tsx
│       │   │   └── StoryboardBoardPanel.tsx
│       │   ├── timeline/
│       │   │   ├── TimelineView.tsx
│       │   │   ├── TrackHeader.tsx
│       │   │   ├── ClipItem.tsx
│       │   │   └── PlayheadRuler.tsx
│       │   ├── inspector/
│       │   │   ├── ClipInspector.tsx
│       │   │   ├── SubtitleInspector.tsx
│       │   │   └── AutoParameterForm.tsx
│       │   ├── preview/
│       │   │   └── PreviewCanvas.tsx
│       │   ├── stores/
│       │   │   ├── useProjectStore.ts
│       │   │   ├── usePlaybackStore.ts
│       │   │   └── useSelectionStore.ts
│       │   └── i18n/
│       │       └── index.ts
│       ├── tsconfig.json
│       └── package.json
│
├── locales/                     i18n 辞書 (JSON)
│   ├── ja.json
│   └── en.json
│
├── examples/plugins/            JS プラグインのサンプル
│   └── glitch-effect/
│       ├── yave-plugin.json
│       └── index.ts
│
├── tests/                       結合・E2E テスト
│   ├── e2e/
│   │   ├── web.spec.ts          (Playwright - Chromium)
│   │   └── electron.spec.ts     (Playwright - Electron launch)
│   └── fixtures/
│       ├── sample.srt
│       ├── lyrics.srt
│       └── sample_project.yave
│
├── doc/                         本設計書
├── .github/workflows/
│   ├── ci.yml
│   ├── pages.yml
│   └── release-electron.yml
│
├── package.json
├── pnpm-workspace.yaml
├── tsconfig.base.json
└── eslint.config.js
```

---

## 2.2 モジュール依存関係表

矢印は「行が列を import してよいか」を示す。循環参照は全面的に禁止する。

| パッケージ | `core` | `io` | `engine` | `platform` | `plugin-sdk` | `ui` |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| `core` | — | ✗ | ✗ | ✗ | ✗ | ✗ |
| `io` | **✓** | — | ✗ | ✗ | ✗ | ✗ |
| `engine` | **✓** | **✓** | — | **✓** | **✓** | ✗ |
| `platform` | **✓** (型のみ) | ✗ | ✗ | — | ✗ | ✗ |
| `plugin-sdk` | ✗ | ✗ | ✗ | ✗ | — | ✗ |
| `ui` | **✓** | **✓** | **✓** | **✓** | **✓** | — |
| `apps/web` | **✓** | **✓** | **✓** | **✓** | **✓** | **✓** |
| `apps/electron` | **✓** | **✓** | **✓** | **✓** | **✓** | **✓** |

- **`packages/core` の独立性**:
  `packages/core` は何者にも依存しない。ブラウザの DOM API、React、Node.js 組み込みモジュール (`fs`, `path` 等) の import は ESLint ルールで弾く。これにより、純粋な単体テストがヘッドレス環境でミリ秒単位で高速実行できる。
- **`packages/plugin-sdk` の独立性**:
  サードパーティプラグイン作者が npm 等から単体で導入できるよう、リポジトリ内の他パッケージに依存しないプレーンな型定義ライブラリとする。
- **`packages/platform` の境界性**:
  `packages/platform` は `PlatformHost` インタフェースおよび各ホスト実装を持つが、UI コンポーネントやレンダリングエンジンには依存しない。

---

## 2.3 命名規約

| 対象 | 規約 | 例 |
|---|---|---|
| ファイル名 (クラス / 型定義) | PascalCase.ts | `VideoClip.ts`, `ProjectSerializer.ts` |
| ファイル名 (React コンポーネント) | PascalCase.tsx | `TimelinePanel.tsx`, `InspectorPanel.tsx` |
| ファイル名 (React Hook) | camelCase (`use` 接頭辞) | `useProjectStore.ts`, `useLanguage.ts` |
| ファイル名 (Web Worker) | `*.worker.ts` | `decode.worker.ts`, `waveform.worker.ts` |
| ファイル名 (AudioWorklet) | `*.worklet.ts` | `mixer.worklet.ts` |
| ファイル名 (シェーダ) | `*.glsl` (`.vert.glsl`, `.frag.glsl`) | `layer_blend.frag.glsl` |
| クラス名 / インタフェース / 型名 | PascalCase | `SubtitleClip`, `CommandStack`, `Rational` |
| 関数 / メソッド | lowerCamelCase | `insertClip()`, `secondsToFrames()` |
| プロパティ / 変数 | lowerCamelCase | `frameIndex`, `trackCount` |
| 定数 | UPPER_SNAKE_CASE | `MAX_GPU_FRAMES`, `DEFAULT_FPS` |
| JSON キー | lowerCamelCase | `"effectStack"`, `"storyBible"` |
| i18n 翻訳キー | ドット区切り lowerCamelCase | `common.ok`, `timeline.addTrack` |

> **`cut` という名前のメソッドの禁止 (継承ルール)**:
> タイムライン操作やクリップボード操作において、単語 `cut` をメソッド名に用いない (`copySelection`, `removeSelection` 等とする)。AI トラックの「カット」(`CutClip`) との混同を防ぐため。

---

## 2.4 生成物の配置

ビルド成果物はすべてリポジトリルートの `.gitignore` で追跡対象外とする。

```
/
├── apps/web/dist/               GitHub Pages デプロイ用の静的ファイル群 (HTML, JS, CSS, WASM)
├── apps/electron/dist/          Electron レンダラービルド成果物
├── apps/electron/release/       electron-builder が出力するインストーラ (exe, dmg, deb)
└── coverage/                    テストカバレッジレポート
```

### 2.4.1 実行時ユーザーデータ配置

| 環境 | ディレクトリ | 用途 |
|---|---|---|
| Web (ブラウザ) | OPFS (`/yave/`) | 一時デコードフレーム、プロキシ動画、波形キャッシュ |
| Web (ブラウザ) | IndexedDB (`yave_store`) | プロジェクト自動保存、UI 設定、シークレット |
| Electron | `app.getPath('userData')` | 設定 JSON (`settings.json`)、キャッシュ (`cache/`)、プラグイン (`plugins/`) |
