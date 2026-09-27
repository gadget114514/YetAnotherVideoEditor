# YetAnotherVideoEditor (YAVE) 詳細設計書

TypeScript / React / WebCodecs / WebGL2 / Web Audio API ベースの、クロスプラットフォーム (GitHub Pages 上のブラウザ版 + Electron デスクトップ版) 動画編集アプリケーションの詳細設計書。

- **バージョン**: 2.0
- **最終更新**: 2026-09-27
- **対象読者**: 本アプリケーションの実装担当者
- **前提**: 本書のコードはそのまま実装の出発点として使える粒度で記述している
- **旧版参照**: v1.x (Qt/C++ 版) はタグ `legacy-qt` を参照

---

## 目次

| 章 | ファイル | 内容 |
|---|---|---|
| 1 | [全体アーキテクチャ設計](design/01-architecture.md) | レイヤ構造・コンポーネント図・スレッドモデル・pnpm workspace・PlatformHost・UI パネル構成 |
| 2 | [ディレクトリ構成](design/02-directory-layout.md) | monorepo 構造・モジュール依存関係・命名規約・成果物配置 |
| 3 | [タイムライン & レンダリングエンジン](design/03-timeline-render.md) | 有理数時間・無限レイヤー構造・WebGL2 合成パイプライン・フィルター・トランジション・タイトル |
| 4 | [ビデオエンジン (WebCodecs + GPU)](design/04-video-engine.md) | WebCodecs デコード/エンコード・VideoFrame ライフサイクル・フレームキャッシュ・プロキシ・書き出し |
| 5 | [オーディオエンジン](design/05-audio-engine.md) | AudioWorklet オーディオファースト同期・PDC・事前デコード・波形生成・オフラインレンダリング |
| 6 | [字幕エンジン](design/06-subtitle-engine.md) | SRT 取り込み・OffscreenCanvas レイアウト・グリフアトラス・ISubtitleEffect (JS プラグイン) |
| 7 | [マルチモーダル生成AIエンジン](design/07-ai-orchestrator.md) | onnxruntime-web / リモート HTTP / Electron sidecar・タスクライフサイクル・キャッシュ・コミット |
| 8 | [プラグインシステム](design/08-plugin-host.md) | VST3/AviUtl 廃止と経緯・ES module プラグイン・Worker サンドボックス・plugin-sdk・WAM (将来) |
| 9 | [プロジェクト保存 (JSON)](design/09-project-io.md) | スキーマ v4・マイグレーション・原子的保存・IndexedDB 自動保存・JsonKeys / EnumMapping |
| 10 | [国際化 (日英切替)](design/10-i18n.md) | i18next ランタイム切替・規約・複数形・タイムコード非依存性・フォント管理 |
| 11 | [型定義リファレンス](design/11-type-reference.md) | 主要ドメイン・エンジン・UI 型の TypeScript 完全定義 |
| 12 | [ビルド・デプロイ設計](design/12-build-deploy.md) | Vite / Electron / GitHub Pages (coi-serviceworker) / GitHub Actions / セキュリティ |
| 13 | [AIトラック (演出指示 / 絵コンテ)](design/13-ai-track.md) | CutClip・StoryBible・カスケード・プロンプト合成・バッチ DAG・L1 契約・アニマティック |
| 14 | [移行ガイド & テスト対応表](design/14-migration.md) | 旧 Qt/C++ からの新スタック対応表・廃止理由・単体テスト 11 本の移植表・M0〜M7 マイルストーン |

---

## 1. プロダクト概要

YAVE は以下を同時に満たすことを目標とする。

1. **トラック数無制限**のノンリニア編集。ビデオ / オーディオ / 字幕 / AI生成 / 絵コンテ の各トラックを動的に追加・削除・並び替えできる。
2. **1080p (1920x1080) / 60fps のリアルタイムプレビュー** (4K はプロキシ対応)。複数レイヤーの WebGL2 GPU 合成を含む。
3. **WebCodecs / WebGL2 によるブラウザ内メディアエンジン**。mp4box.js / web-demuxer による demux と、HW デコード支援を第一級で使う。
4. **マルチモーダル生成AI** をタイムラインの区間指定で呼び出せる。さらに **AIトラック** の各区間にカット単位の演出指示を置き、そこから必要なトラック (映像 / ナレーション / BGM / SE / 字幕) を自動生成できる ([13章](design/13-ai-track.md))。
5. **JS エフェクトプラグイン**。字幕アニメーションや映像フィルターを ES module プラグインとして拡張できる ([8章](design/08-plugin-host.md))。
6. プロジェクトは**人間可読な JSON** で完全永続化される (schemaVersion 4, [9章](design/09-project-io.md))。
7. UI は**日本語 / 英語をランタイム切替**できる (i18next, [10章](design/10-i18n.md))。
8. UI のパネル (メディアライブラリ / エフェクトライブラリ / ファイルブラウザ / コンソール / インスペクタ / タイムライン / AI タスク一覧 / 絵コンテボード) は **flexlayout-react によるドッキング位置とタブ構成をユーザーが設定** できる。素材と効果はエクスプローラ風のフォルダ UI で整理し、タイムラインへドラッグして使う ([1.7](design/01-architecture.md#17-ui-パネル構成-ドッキング--タブ))。
9. **同一コードベースから GitHub Pages 上のブラウザ版 (静的ホスティング) と Electron デスクトップ版の両方を提供** する。

## 2. 技術スタック

| 領域 | 採用技術 | 備考 |
|---|---|---|
| 言語 | TypeScript 5.x (`strict: true`, `noUncheckedIndexedAccess: true`) | 型安全の徹底、ES2022+ |
| UI | React 18+ + CSS Modules | ドッキング UI に **flexlayout-react** を採用 |
| 状態管理 | ドメイン層: 純 TS クラス / UI 層: **Zustand** | store は「ドメイン変更通知 → React 再描画」の橋渡しに限定 |
| Undo / Redo | 自前 `CommandStack` + `Command` インタフェース | マクロ・マージ・非破壊履歴をサポート |
| ID | `crypto.randomUUID()` | string 型の branded type `Uuid` |
| 時間表現 | `Rational` { num: number, den: number } + フレーム番号 `number` (整数) | 比較は交差乗算。`Number.MAX_SAFE_INTEGER` 超過時のみ BigInt 経路。**double 秒保持禁止** |
| 描画 / 合成 | **WebGL2** (必須) + GLSL ES 3.0 | WebGPU は将来の拡張用として抽象インタフェースを整備 |
| デコード | **WebCodecs `VideoDecoder` / `AudioDecoder`** | demux は **mp4box.js** (MP4/MOV) / **web-demuxer** (WebM/MKV 等)。非対応形式は **ffmpeg.wasm** でプロキシ生成 |
| エンコード / 書き出し | **WebCodecs `VideoEncoder` / `AudioEncoder`** + **mp4-muxer** / **webm-muxer** | Electron では同梱 ffmpeg 実行ファイルによるネイティブ書き出しもオプション |
| 音声処理 | **Web Audio API + AudioWorklet** | マスタクロック = `AudioContext` の再生位置 (サンプルカウンタ) |
| 並列 / スレッド | **Web Worker** + **AudioWorklet** | デコード・波形・サムネイル・AI は Worker 実行。`SharedArrayBuffer` (cross-origin isolation) + Transferable `postMessage` |
| プラグイン | **ES module JS プラグイン** (`import()`) | Worker 内実行。VST3・AviUtl は廃止。音声は将来 **WAM (Web Audio Modules)** |
| AI 推論 | **onnxruntime-web** (WebGPU / WASM) / リモート HTTP (`fetch`) | ComfyUI / OpenAI 互換 / Replicate。Worker 内実行 |
| 永続化 | JSON (同一スキーマ系統、`schemaVersion: 4`) | ファイル I/O は `PlatformHost` 経由 (File System Access API / Electron IPC) |
| 設定 | Web: `localStorage` (UI 設定) + IndexedDB / Electron: `app.getPath('userData')` 配下の JSON | |
| 秘密情報 | Web: IndexedDB (平文相当を UI で明示) / Electron: `safeStorage` 暗号化 | **プロジェクト JSON には絶対に含めない** |
| 国際化 (i18n) | **i18next + react-i18next** | `locales/ja.json`, `locales/en.json` |
| テスト | **Vitest** (単体テスト) + **Playwright** (E2E、Web と Electron の両対応) | CI ヘッドレス実行 |
| パッケージ管理 | **pnpm workspace** | monorepo 構成 |
| Electron 配布 | **electron-builder** (Windows: NSIS, macOS: dmg) | `contextIsolation: true`, `nodeIntegration: false`, `sandbox: true`, preload `contextBridge` |
| Web 配布 | **GitHub Actions** (`pnpm build:web` → `actions/deploy-pages`) | cross-origin isolation は **coi-serviceworker** で付与 |

## 3. 設計上の全体方針 (全章共通の前提)

本書の各章はこれらを前提として記述している。個別章で再説明しない。

### 3.1 時間表現は有理数で統一する
すべてのタイムライン時刻は `Rational` (分子 `num` / 分母 `den` の整数対) と、それを基準とした整数のフレーム番号 (`number`) で保持する。**`double` 秒での保持は禁止する**。

> **理由**: 60000/1001 (59.94fps) や 24000/1001 (23.976fps) といったドロップフレーム系タイムベースを扱う前提で、`double` 秒での往復変換は編集点のズレとして必ず顕在化する。特に「クリップを分割して再結合したら 1 フレームずれる」「SRT を取り込んだら境界が 1 フレーム手前になる」といった不具合はユーザーの信頼を致命的に損なう。

プロジェクトは単一の `timebase` を持つ (既定 `60000/1001`)。素材側のタイムベースは取り込み時にプロジェクトタイムベースへ変換する。
JavaScript の `number` 型は 53 bit 整数 (`Number.MAX_SAFE_INTEGER` = 9,007,199,254,740,991、60fps で約 470 万年分) を安全に表現できる。交差乗算による比較や変換でこの範囲を超える極端な値が現れた場合のみ `BigInt` 演算へフォールバックする。

> **外部境界での例外**: SRT 取り込み ([6章](design/06-subtitle-engine.md)) と LLM が返すカット尺 ([13.7](design/13-ai-track.md)) は、仕様上どうしても秒で入ってくる。これらは**パーサの境界で `secondsToFrames(..., RoundMode)` によりフレームへ変換し、秒のまま保持しない**。「保持を禁じる」のであって「受け取りを禁じる」のではない。

### 3.2 編集操作はすべて Command である
タイムラインを変更するあらゆる操作は `Command` インタフェースを実装したクラスとして作成し、`CommandStack` に積む。これは UI 操作に限らず、**AI 生成結果のコミット** や **SRT 一括取り込み** にも適用する。

> **理由**: 「AI が生成したクリップの挿入だけ Undo できない」という状態は実用上破綻する。生成には時間がかかるため、ユーザーは必ず「試して、気に入らなければ戻す」使い方をする。

### 3.3 ID は UUID 文字列
`Clip`, `Track`, `AiGenerationTask`, プラグインインスタンスはすべて `crypto.randomUUID()` で生成した UUID 文字列 (branded type `Uuid`) で識別する。配列インデックスを永続 ID として使わない (並び替えで壊れるため)。

### 3.4 アセットパスはプロジェクト相対
プロジェクトファイル (`*.yave`) からの相対パスで保存する。絶対パスは持たない。

> **理由**: Windows, macOS, およびブラウザ環境の間でプロジェクトを持ち運ぶ要件があるため。ドライブレターや OS 固有の絶対パスを保存に含めた時点で可搬性が失われる。

相対化できない場所 (別ドライブやブラウザの異なる起点) を参照した場合はプロジェクト保存時に警告し、「アセットをプロジェクトフォルダへ収集する」操作を提案する。Web 環境では File System Access API のディレクトリハンドル配下にアセットを配置するか、単一アーカイブ (`*.yavez`) として管理する。

### 3.5 UI 文字列は必ず翻訳関数を通す
UI 表示文字列は必ず `t()` (react-i18next の `useTranslation` または i18n インスタンス) を通す。生の文字列リテラルを JSX や UI コントローラに直接渡すことを ESLint ルールで禁止する。詳細は [10章](design/10-i18n.md)。

### 3.6 プラットフォーム依存コードの隔離
ブラウザ環境 (Web) とデスクトップ環境 (Electron) の差異は、すべて `PlatformHost` インタフェース ([1.8](design/01-architecture.md#18-platformhost-設計)) に集約する。UI 層・エンジン層・コア層が直接 `window.electron` や Node.js の API を呼ぶことを禁止する。

### 3.7 ドメイン層の純粋性 (新規)
ドメイン層 (`packages/core`) は DOM API、React、Node.js API、ブラウザ固有オブジェクトを一切 import してはならない。純粋な TypeScript オブジェクトとアルゴリズムのみで構成し、ESLint の `no-restricted-imports` で厳格に検証する。これにより、UI なしの Node.js CLI / CI 環境や Worker スレッド内でも core の単体テストおよびロジックが完全に同一動作する。

### 3.8 メインスレッドでの重い処理の禁止 (新規)
ブラウザのメインスレッドは React の再描画と UI イベント処理、WebGL2 の draw call 発行に専念させる。メディアの demux/デコード、波形ピーク計算、サムネイル生成、ONNX AI 推論、SRT の大量パースなどの負荷処理はすべて Web Worker または AudioWorklet へオフロードする。

---

## 4. 用語集

| 用語 | 意味 |
|---|---|
| **トラック / レイヤー** | 本書では同義。`Track` クラスのインスタンス。上に置かれたトラックが手前に合成される |
| **クリップ** | トラック上に配置された、開始・終了を持つ編集単位 (`Clip` 派生) |
| **字幕区間** | 字幕トラック上の 1 クリップ (`SubtitleClip`)。SRT の 1 キューに対応 |
| **タイムベース** | 1 フレームの長さを表す有理数。`60000/1001` なら 1 フレーム = 1001/60000 秒 |
| **PDC** | Plugin Delay Compensation。プラグインやエフェクトが持つ処理遅延を他トラックの遅延で相殺すること |
| **In点 / Out点** | 区間の開始フレーム / 終了フレーム (Out は排他的、すなわち `[in, out)`) |
| **コミット** | AI 生成などの非同期結果を、実際にタイムラインへ反映すること |
| **プロバイダ** | AI 生成の実行主体 (ローカル ONNX Web / リモート HTTP API / Electron sidecar) |
| **グリフラン** | 字幕テキストをレイアウト後、文字単位に分解した描画単位 |
| **AIトラック / 絵コンテトラック** | `TrackType.Storyboard` のトラック。メディアではなく演出指示が載る。合成には参加しない |
| **カット** | AIトラック上の 1 区間 (`CutClip`)。絵コンテの 1 コマにあたる演出指示の単位 |
| **出力バインディング** | 1 カットが生む 1 成果物の仕様 (`OutputBinding`)。役割・配置先・パラメータ・生成状態を持つ |
| **Story Bible** | プロジェクト単位の作品設定 (キャラクター / ロケーション / 画風)。全カットのプロンプトへカスケードする |
| **アニマティック** | 未生成カットをボード画像 + TTS プレビューで仮再生すること |
| **パネル** | flexlayout-react でドッキング配置できる UI の単位。タブ化できるのは [1.7.1](design/01-architecture.md#171-タブとして設定可能なパネル) の 8 種のみ |
| **ライブラリ** | 素材や効果をフォルダで整理して並べるパネル。プロジェクト所有の `mediaLibrary` と、アプリ所有の `effectLibrary` の 2 枚がある |
| **フィルター** | クリップ 1 個に掛ける映像処理 (`VideoFilterInstance`)。配列順に適用され、その後で合成される |
| **トランジション** | クリップ境界に置く映像の切り替え (`Transition`)。クリップ同士は重ねず、境界に付く別オブジェクトとして持つ |
| **タイトル** | 映像トラックに置けるテロップ (`TitleClip`)。描画は字幕と同じ経路だが、書き出し時に字幕トラックとして分離されない |
| **PlatformHost** | Web と Electron のファイルアクセス・暗号化・ネットワーク差分を吸収する抽象アダプタ |
| **Worker** | メインスレッド外で重い処理 (デコード、波形生成、AI 推論) を実行する Web Worker |
| **AudioWorklet** | Web Audio API のリアルタイム音声処理スレッド |
| **OPFS** | Origin Private File System。Web 版で高速にアクセス可能なブラウザ内専用ファイルシステム |
| **cross-origin isolation** | `SharedArrayBuffer` を利用可能にするセキュリティコンテキスト (COOP / COEP) |
| **WebCodecs** | ブラウザ標準の低遅延ビデオ/オーディオデコード・エンコード API |

---

## 5. 非機能要件

| 項目 | 目標値 | 備考 |
|---|---|---|
| **プレビュー性能** | 1920x1080 / 60fps / 4 レイヤー同時合成でフレーム落ちなし | Chromium 系ブラウザ + ハードウェアデコード有効時。4K は 540p プロキシ使用で 30fps 以上 |
| **シーク応答** | 150ms 以内 | 直前キーフレームからの高速デコードおよびキャッシュ hit 時 |
| **音声遅延** | `AudioContext` の `latencyHint: 'interactive'` 既定値で動作 | 音切れ・グリッチを起こさないバッファリング制御 |
| **プロジェクト起動** | 1000 クリップのプロジェクトを 2 秒以内にロード | JSON デシリアライズ + タイムライン構築完了まで |
| **メモリ使用量** | `VideoFrame` 同時保持数に上限 (既定 GPU フレーム 60 枚) を設け、JS ヒープ 2GB 以内 | リーク防止のため使用済み `VideoFrame.close()` を厳格に管理 |
| **対応ブラウザ** | Chrome / Edge 最新 2 バージョンを正式対応 | Firefox / Safari は WebCodecs サポート状況に応じた制限付き対応 |
| **セキュリティ** | 認証情報 (API キー) はプロジェクト JSON に含めず、Electron では `safeStorage`、Web では IndexedDB に保存 | Electron では contextIsolation, sandbox 有効化、CSP 厳格化 ([12章](design/12-build-deploy.md)) |
| **JSON フォーマット** | スキーマ定義・バージョニング・マイグレーションを完全文書化 ([9章](design/09-project-io.md)) | 後方互換性と未知フィールド保持を保証 |
