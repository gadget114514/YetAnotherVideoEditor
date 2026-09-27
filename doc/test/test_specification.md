# YAVE テスト仕様書 (Test Specification)

本書は、YetAnotherVideoEditor (YAVE) の Web / Electron 新アーキテクチャにおけるテスト戦略、テスト構成、および単体・結合テストの仕様について定義する。

---

## 1. テスト戦略

### 1.1 前提と方針
- **単体テストフレームワーク**: **Vitest** を使用する。高速な TypeScript 実行とモック機能、およびブラウザ環境シミュレーション (happy-dom / jsdom) を提供する。
- **E2E テストフレームワーク**: **Playwright** を使用する。
  - Web 版: Chromium ブラウザインスタンスによる操作テスト
  - Electron 版: `_electron.launch()` によるネイティブウィンドウ・IPC 疎通テスト
- **CI 環境での動作**: 単体テストは GPU もコーデックも無いヘッドレス環境 (GitHub Actions `ubuntu-latest`) で実行可能とする。`packages/core` および `packages/io` は DOM やネイティブ API に依存しないためミリ秒単位で高速実行される。
- **フィクスチャ配置**: テスト用データは `tests/fixtures/` に集約する。

### 1.2 テストの実行方法
```bash
# 全単体テストの実行
pnpm test

# 単体テストの監視モード実行
pnpm test:watch

# E2E テストの実行
pnpm test:e2e
```

---

## 2. 単体テスト仕様一覧 (Vitest)

### 2.1 `packages/core/test/rational.test.ts`
有理数演算と秒・フレーム間の高精度変換を検証する。
- **検証項目**:
  - `reduced`: GCD による約分、負の分母の正規化、ゼロ除算保護。
  - `comparison`: 等価、不等価、大小比較演算。
  - `overflowComparison`: `Number.MAX_SAFE_INTEGER` を超える巨大値に対する `BigInt` 経路での正確な比較。
  - `arithmetic`: タイムベース同士の加減算、乗算。
  - `secondsToFrames` / `framesToSeconds`: Floor, Ceil, Nearest 各丸めモードでの往復変換精度。
  - `rescaleFrames`: 異なるタイムベース間 (例: 30fps ⇄ 59.94fps) のフレーム番号再計算。
  - `dropFrameTimebase`: 23.976, 29.97, 59.94 fps における往復変換の冪等性。

### 2.2 `packages/core/test/timeline.test.ts`
無限レイヤートラック構造とクリップ編集操作を検証する。
- **検証項目**:
  - トラックの追加・削除・並び替え (`moveTrack`) と Z オーダー (配列順) の整合性。
  - 同一トラック内クリップの重複拒否、In点/Out点の整合性。
  - クリップの二分探索 (`findClipAt`, `clipsIn`) の正確性。
  - クリップ分割 (`SplitClipCommand`) と Undo/Redo による完全復元。
  - リップル削除 (`RippleDeleteCommand`) による前方クリップの詰め。
  - `RenderSnapshot` (イミュータブルスナップショット) 生成ロジック。

### 2.3 `packages/io/test/srtparser.test.ts`
SRT 字幕ファイルのパースおよび書き出しを検証する。
- **検証項目**:
  - 標準 SRT 形式のパースと `SubtitleClip` への変換。
  - Shift_JIS エンコーディングの自動判別とフォールバック。
  - インライン装飾タグ (`<b>`, `<i>`, `<font color=...>`) のスパン変換。
  - タイムコード境界における丸め処理 (`secondsToFrames`)。
  - 壊れた SRT 入力に対するロバスト性とクラッシュ防止。
  - パース結果からの `SrtWriter` による往復整合性。

### 2.4 `packages/io/test/projectserializer.test.ts`
プロジェクト JSON のシリアライズ・デシリアライズを検証する。
- **検証項目**:
  - プロジェクト設定、アセット、トラック、クリップ情報の完全なシリアライズ往復。
  - 未知フィールド (`unknownFields`) の保持と書き戻し。
  - 原子的保存 (一時ファイル書き込み → 置換) によるファイル保全。
  - enum 値が数値ではなく文字列として JSON に保存されていること。
  - 旧バージョン (v1〜v3) からのマイグレーション関数の動作検証。
  - フィクスチャ `tests/fixtures/sample_project.yave` の正常読み込み。

### 2.5 `packages/engine/test/delaycompensator.test.ts`
オーディオエフェクト遅延補正 (PDC) を検証する。
- **検証項目**:
  - 各トラックのエフェクトチェーンにおけるレイテンシ合算。
  - 全トラック中の最大レイテンシ算出と各トラックの補正遅延サンプル数決定。
  - マスターチェーンレイテンシのクロックオフセット反映。
  - 遅延リングバッファ (`DelayLine`) の入出力整合性。

### 2.6 `packages/ui/test/i18n.test.ts`
i18next によるランタイム言語切替を検証する。
- **検証項目**:
  - 言語切替 ('ja' ⇄ 'en') による翻訳文字列の即時変更。
  - タイムコード文字列 (`formatTimecode`) がロケールに影響されず一定の書式を出力すること。

---

## 3. E2E テスト仕様 (Playwright)

### 3.1 Web 版 E2E (`tests/e2e/web.spec.ts`)
- **起動確認**: GitHub Pages の静的配信環境でページがエラーなくロードされること。
- **Cross-Origin Isolation**: `window.crossOriginIsolated === true` であること。
- **基本操作**: ライブラリからタイムラインへのアセットドラッグ & ドロップ、クリップ配置、再生ヘッドの移動。

### 3.2 Electron 版 E2E (`tests/e2e/electron.spec.ts`)
- **起動確認**: Electron アプリが正常にウィンドウを生成し、レンダラーが初期化されること。
- **IPC 疎通**: `PlatformHost` 経由のファイルオープン・保存ダイアログ IPC が正常に応答すること。
- **ウィンドウ制御**: タイトル変更、最小化、閉じる前の未保存確認ダイアログの動作。
