# 7. マルチモーダル生成AIエンジン

[← 目次に戻る](../design.md)

---

## 7.1 設計方針

### 7.1.1 プロバイダ抽象

AI 生成タスクの実行基盤は 3 系統あり、`IGenerationProvider` インタフェースで抽象化する。

| プロバイダ | 実装 | 用途 |
|---|---|---|
| `OnnxWebProvider` | `onnxruntime-web` (WebGPU / WASM EP) を Worker で実行 | 軽量タスク: STT (Whisper), TTS, 背景除去/マスク (RMBG), 超解像 |
| `RemoteHttpProvider` | `PlatformHost.fetch` による HTTP/JSON 通信 | 重量タスク: T2V / I2V / V2V。ComfyUI (ローカル LAN / リモート), OpenAI 互換 API, Replicate |
| `SidecarProvider` | **Electron 専用** (Node.js `child_process.spawn`) | ローカル Python スクリプトや Diffusers プロセスとの stdio IPC |

> **Web 版における CORS の制約**:
> ブラウザ版から外部 AI サービス (ComfyUI や各社 API) を直接呼ぶ場合、相手方サーバーが適切な CORS ヘッダ (`Access-Control-Allow-Origin`) を返さないとブラウザのセキュリティ機能により遮断される。この場合、(1) 利用者自身によるリバースプロキシの設置、(2) CORS 制約を受けない Electron 版の利用 を UI で案内する。

### 7.1.2 UI を絶対にブロックしない

生成タスクはすべてバックグラウンドで非同期に実行する。タイムライン上には直ちに `AiPlaceholderClip` が配置され、進捗パーセンテージとステータスが表示される。

---

## 7.2 生成パラメータ

```ts
// packages/core/src/ai/AiGenerationParams.ts
export type GenerationKind = 'video' | 'audio' | 'subtitle' | 'mask' | 'effectMetadata' | 'image' | 'storyboard';
export type GenerationPurpose = 'commit' | 'animaticPreview';
export type VideoGenMode = 'textToVideo' | 'imageToVideo' | 'videoToVideo';
export type I2VReferenceMode = 'startFrameOnly' | 'endFrameOnly' | 'bothEnds';

export interface ImageReference {
  readonly source: 'filePath' | 'timelineFrame';
  readonly filePath?: string;
  readonly sourceTrackId?: string;
  readonly sourceFrame?: number;
  readonly strength: number; // 0..1
}

export interface AiGenerationParams {
  readonly kind: GenerationKind;
  readonly purpose: GenerationPurpose;
  readonly modelId: string;
  readonly providerId: string;
  readonly prompt: string;
  readonly negativePrompt?: string;
  readonly seed?: number;
  readonly steps?: number;
  readonly guidanceScale?: number;
  readonly videoMode?: VideoGenMode;
  readonly i2vRefMode?: I2VReferenceMode;
  readonly startReference?: ImageReference;
  readonly endReference?: ImageReference;
  readonly extraParams?: Record<string, any>;
}
```

---

## 7.3 タスクのライフサイクルと状態遷移

```
              ┌───────────────┐
              │    Queued     │
              └───────┬───────┘
                      │ 開始
                      ▼
              ┌───────────────┐
      ┌───────┤    Running    ├───────┐
      │       └───────┬───────┘       │
      │ エラー        │ 完了          │ キャンセル
      ▼               ▼               ▼
┌───────────┐   ┌───────────┐   ┌───────────┐
│  Failed   │   │  Cached   │   │ Cancelled │
└───────────┘   └─────┬─────┘   └───────────┘
                      │ コミット (Undo 可能)
                      ▼
                ┌───────────┐
                │ Committed │
                └───────────┘
```

1. **Queued**: レーンごとの最大同時実行数 (LocalGpu=1, Remote=6 等) に従い待機。
2. **Running**: プロバイダにリクエストを送信し、定期的に進捗を取得。
3. **Cached**: 生成メディアをキャッシュ領域 (OPFS / userData) に取得完了。
4. **Committed**: タイムラインに実クリップ (`VideoClip` 等) を挿入し、Undo スタックに `CommitGeneratedAssetCommand` を push。

---

## 7.4 キャッシュストレージ (`GenerationCache`)

- **Web (ブラウザ)**: OPFS (Origin Private File System) 内の `/yave/gen/<taskId>/` にバイナリ保存。
- **Electron**: `app.getPath('userData')/gen/<taskId>/` に保存。
- 生成パラメータ (`AiGenerationParams`) と出力ハッシュはタスクレコードとしてプロジェクト JSON に完全永続化されるため、キャッシュを破棄しても同一パラメータで再生成が可能。

---

## 7.5 API キーと機密情報の管理

AI プロバイダとの通信に必要な API キーやトークンは、`PlatformHost.secrets` を介して管理する。

- **プロジェクトファイルには絶対に保存しない**。プロジェクト JSON を共有しても認証情報が漏洩しないことを保証する。
- **Electron**: OS のキーチェーンを利用する `safeStorage` API で暗号化保存。
- **Web**: IndexedDB に保存。ブラウザのストレージ仕様上、同一オリジンからの平文アクセスと同等であるため、「ブラウザ保存は平文相当である」旨を UI で注意喚起する。
