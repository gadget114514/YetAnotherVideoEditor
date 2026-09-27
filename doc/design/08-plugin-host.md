# 8. プラグインシステム

[← 目次に戻る](../design.md)

---

## 8.1 ネイティブプラグインの廃止と経緯

### 8.1.1 廃止対象
- **VST3** (Steinberg VST 3.7+ C++ プラグイン)
- **AviUtl プラグイン** (Windows x64 専用フィルタ / 入力プラグイン)
- **C ABI ネイティブ字幕エフェクト DLL / dylib**

### 8.1.2 廃止の理由
1. **ブラウザ環境での実行不能**: Web ブラウザ (GitHub Pages 版) では OS ネイティブコード (.dll / .dylib) を実行できず、WASM への再コンパイルもソース非公開の市販プラグインでは不可能である。
2. **Web / Electron 間の機能格差の排除**: Electron 版のみでネイティブプラグインを維持すると、プロジェクトの可搬性が失われ、コードベースの二重保守が発生する。
3. **セキュリティ**: サードパーティの任意 C/C++ コード実行に伴うクラッシュや脆弱性を完全に排除し、Web 標準のサンドボックス内で安全に実行する。

### 8.1.3 旧プロジェクトの互換性
旧バージョン (v1〜v3) で保存されたプロジェクト内に VST3 や AviUtl の設定が含まれていた場合、読み込み時に破棄せず `{"disabled": true, "legacy": { ... }}` としてそのまま保持する。プロジェクトを再度保存した際もデータが消えないため、将来のネイティブブリッジ等の検討や手動移行を妨げない。

---

## 8.2 JS プラグインのマニフェスト (`yave-plugin.json`)

サードパーティプラグインは単一のディレクトリまたは `.zip` アーカイブとして配布され、ルートに `yave-plugin.json` を配置する。

```json
{
  "id": "com.example.glitch-effect",
  "name": {
    "ja": "グリッチエフェクト",
    "en": "Glitch Effect"
  },
  "version": "1.0.0",
  "sdkVersion": "2.0.0",
  "kind": "videoFilter",
  "entry": "dist/index.js",
  "author": "Example Developer",
  "description": {
    "ja": "RGB シフトとノイズによるグリッチ効果",
    "en": "Glitch effects with RGB shift and noise"
  }
}
```

- **`kind` の種類**:
  - `subtitleEffect`: 字幕アニメーション (`ISubtitleEffect`)
  - `videoFilter`: 映像フィルタ (GLSL フラグメントシェーダ + パラメータ)
  - `transition`: クリップ間トランジション (GLSL シェーダ)
  - `audioEffect`: 音声エフェクト (将来拡張: WAM 2.0)

---

## 8.3 プラグインのロード方式

| 環境 | ロード手順 |
|---|---|
| **Web (ブラウザ)** | ユーザーがファイルピッカーでプラグインフォルダまたは `.zip` を選択 → OPFS (`/yave/plugins/<id>/`) に展開・配置 → エントリ JS を Blob URL に変換して `import(blobUrl)` で動的ロード |
| **Electron** | `app.getPath('userData')/plugins/<id>/` ディレクトリを走査し、ファイルプロトコルまたは ES module としてロード |

---

## 8.4 サンドボックスとセキュリティ

悪意のあるスクリプトやバグによるアプリ全体の停止を防ぐため、以下の防御策を講じる。

1. **Worker 内実行**: プラグインの JavaScript コードは、メインの React / DOM コンテキストとは隔離された専用の `PluginWorker` 内で実行する。メインスレッドの DOM や `localStorage` にはアクセスできない。
2. **GPU シェーダの検証**: 映像フィルタを提供するプラグインは、任意の WebGL コマンドを実行するのではなく、**GLSL ES 3.0 のシェーダ文字列と uniforms 定義のみをホストに提出** する。ホスト側が安全にコンパイルし、FBO パイプラインに組み込む。
3. **ネットワーク遮断**: プラグイン Worker 内での `fetch()` や `WebSocket` 通信を制限または監視する。

---

## 8.5 `packages/plugin-sdk` 公開 API

プラグイン作者向けに公開される TypeScript インタフェース定義:

```ts
// packages/plugin-sdk/src/index.ts
export * from './manifest';
export * from './parameterSchema';
export * from './subtitleEffect';
export * from './videoFilter';

// packages/plugin-sdk/src/videoFilter.ts
export interface VideoFilterDefinition {
  readonly id: string;
  readonly displayName: Record<string, string>;
  readonly fragmentShaderSource: string; // GLSL ES 3.0
  readonly parameterSchema: readonly ParameterSchemaItem[];
  buildUniforms(params: Record<string, any>, time: { progress: number; frame: number }): Record<string, number | number[]>;
}
```

---

## 8.6 将来の音声エフェクト (Web Audio Modules 2.0)

音声エフェクトについては、Web オーディオ標準のプラグイン規格である **WAM (Web Audio Modules) 2.0** を将来の拡張基盤として採用する。AudioWorklet 内で動作する DSP コードと、HTML/Web Components によるカスタム GUI を安全に統合可能とする。

---

## 8.7 クラッシュ耐性とブラックリスト

- プラグインの `prepare()` や `apply()` で例外がスローされた場合、または実行時間が 1 フレーム予算 (16.6ms) を連続して大幅超過した場合、ホストはそのプラグインの実行を即座にバイパスする。
- 致命的なエラーを起こしたプラグインは `PlatformHost.settings` 内の `plugin.blacklist` に追加され、次回起動時に読み込みが無効化される。
