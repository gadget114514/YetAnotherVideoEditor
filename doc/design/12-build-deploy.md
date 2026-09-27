# 12. ビルド・デプロイ設計

[← 目次に戻る](../design.md)

---

## 12.1 開発環境

- **ランタイム**: Node.js 22 LTS
- **パッケージマネージャ**: pnpm 9+ (`pnpm workspace`)
- **ビルドツール**: Vite 5+
- **デスクトップランタイム**: Electron 31+

### 初期セットアップ
```bash
# 依存関係のインストール
pnpm install

# ブラウザ版の開発サーバー起動
pnpm dev:web

# Electron 版の開発起動 (Vite 開発サーバー + Electron launch)
pnpm dev:electron
```

---

## 12.2 ルート `package.json` スクリプト一覧

```json
{
  "name": "yave-monorepo",
  "private": true,
  "scripts": {
    "dev:web": "pnpm --filter @yave/web dev",
    "dev:electron": "pnpm --filter @yave/electron dev",
    "build:web": "pnpm --filter @yave/web build",
    "build:electron": "pnpm --filter @yave/electron build",
    "test": "vitest run",
    "test:watch": "vitest",
    "test:e2e": "playwright test",
    "lint": "eslint . --ext .ts,.tsx",
    "typecheck": "tsc --build --noEmit"
  }
}
```

---

## 12.3 Vite 設定の要点

### 12.3.1 `apps/web/vite.config.ts`
```ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  base: '/YetAnotherVideoEditor/', // GitHub Pages 用のサブパス
  plugins: [react()],
  resolve: {
    alias: {
      '@yave/core': path.resolve(__dirname, '../../packages/core/src'),
      '@yave/io': path.resolve(__dirname, '../../packages/io/src'),
      '@yave/engine': path.resolve(__dirname, '../../packages/engine/src'),
      '@yave/platform': path.resolve(__dirname, '../../packages/platform/src'),
      '@yave/ui': path.resolve(__dirname, '../../packages/ui/src'),
    },
  },
  worker: {
    format: 'es', // Worker も ES module としてバンドル
  },
  server: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
});
```

- **AudioWorklet のロード**: `import mixerWorkletUrl from '@yave/engine/audio/worklets/mixer.worklet.ts?url'; await audioContext.audioWorklet.addModule(mixerWorkletUrl);`
- **Web Worker のロード**: `import DecodeWorker from '@yave/engine/media/workers/decode.worker.ts?worker'; const worker = new DecodeWorker();`
- **WASM 配信**: `ffmpeg.wasm` や ONNX Runtime WASM は Vite のアセットパイプラインで自動バンドル。

---

## 12.4 GitHub Pages デプロイ

GitHub Pages は静的ホスティングであり、独自の HTTP レスポンスヘッダを設定できない。しかし、`SharedArrayBuffer` による高速スレッド間共有を利用するには **Cross-Origin Isolation** (COOP: `same-origin` / COEP: `require-corp`) がブラウザから要求される。
これを解決するため、**`coi-serviceworker`** を採用する。

### 12.4.1 `coi-serviceworker` の組み込み
1. `apps/web/index.html` の `<head>` 先頭に `coi-serviceworker.js` を読み込む script タグを配置。
2. 初回到達時、Service Worker が登録され、自身をコントローラとしてページを 1 回自動リロードする。
3. リロード後はすべてのリソースレスポンスに Service Worker が COOP / COEP ヘッダを透過的に付与し、`window.crossOriginIsolated === true` を満たす。

### 12.4.2 ワークフロー設定 (`.github/workflows/pages.yml`)

```yaml
name: Deploy to GitHub Pages

on:
  push:
    branches: [ main ]
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup Node.js 22
        uses: actions/setup-node@v4
        with:
          node-version: 22

      - name: Setup pnpm
        uses: pnpm/action-setup@v3
        with:
          version: 9

      - name: Install dependencies
        run: pnpm install --frozen-lockfile

      - name: Build Web SPA
        run: pnpm build:web

      - name: Setup Pages
        uses: actions/configure-pages@v4

      - name: Upload artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: 'apps/web/dist'

  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    needs: build
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

> **GitHub Pages 有効化手順**:
> リポジトリの `Settings > Pages` において、`Build and deployment` の Source を **"GitHub Actions"** に設定する。

---

## 12.5 Electron 設計

### 12.5.1 BrowserWindow 設定とセキュリティ

```ts
// apps/electron/main/window.ts
import { BrowserWindow, app, session } from 'electron';
import path from 'path';

export function createMainWindow(): BrowserWindow {
  // Cross-Origin Isolation を Electron セッションに適用
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Cross-Origin-Opener-Policy': ['same-origin'],
        'Cross-Origin-Embedder-Policy': ['require-corp'],
      },
    });
  });

  const win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 700,
    webPreferences: {
      contextIsolation: true,       // 必須: メインワールドと分離
      nodeIntegration: false,       // 必須: レンダラーで Node API 禁止
      sandbox: true,               // 必須: Chromium サンドボックス
      preload: path.join(__dirname, '../preload/preload.js'),
    },
  });

  // 外部ブラウザ遷移の制限
  win.webContents.setWindowOpenHandler(({ url }) => {
    // 外部リンクは既定ブラウザで開く
    import('electron').then(({ shell }) => shell.openExternal(url));
    return { action: 'deny' };
  });

  win.webContents.on('will-navigate', (event, url) => {
    if (!url.startsWith('http://localhost') && !url.startsWith('file://')) {
      event.preventDefault();
    }
  });

  return win;
}
```

### 12.5.2 IPC チャネル一覧表 (`PlatformHost` と 1:1 対応)

| IPC チャネル名 | 引数 | 戻り値 | 説明 |
|---|---|---|---|
| `host:openProject` | — | `{ path, content } \| null` | ファイル選択ダイアログ + JSON 読込 |
| `host:saveProject` | `{ path, json }` | `void` | 一時ファイル書き込み + rename |
| `host:saveProjectAs` | `{ suggestedName, json }` | `string \| null` | 名前を付けて保存ダイアログ + 保存 |
| `host:pickMediaFiles` | — | `AssetSource[]` | メディア選択ダイアログ |
| `host:readAsset` | `{ projectPath, relativePath }` | `Buffer` | 素材のバイナリ読み込み |
| `host:getSecret` | `key` | `string \| null` | `safeStorage.decryptString` による復号 |
| `host:setSecret` | `{ key, secret }` | `void` | `safeStorage.encryptString` による暗号化 |
| `host:deleteSecret` | `key` | `void` | 認証情報の削除 |
| `host:fetch` | `{ url, init }` | `{ status, headers, body }` | main プロセス `net.fetch` (CORS 無視) |
| `host:setTitle` | `title` | `void` | ウィンドウタイトルの更新 |

---

## 12.6 Electron セキュリティチェックリスト

- [x] `contextIsolation: true`
- [x] `nodeIntegration: false`
- [x] `sandbox: true`
- [x] `webSecurity: true` (既定)
- [x] `setWindowOpenHandler` によるポップアップ制限
- [x] `will-navigate` による外部 URL 遷移ブロック
- [x] CSP (Content Security Policy) ヘッダの設定 (`default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; ...`)
- [x] `safeStorage` API による API キーの OS キーチェーン保護

---

## 12.7 CI パイプライン (`.github/workflows/ci.yml`)

Pull Request および `main` ブランチへの push 時に全テストを自動実行する。

```yaml
name: CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 22
      - uses: pnpm/action-setup@v3
        with:
          version: 9
      - run: pnpm install --frozen-lockfile
      - run: pnpm lint
      - run: pnpm typecheck
      - run: pnpm test
      - name: Install Playwright Browsers
        run: pnpm exec playwright install --with-deps chromium
      - run: pnpm test:e2e
```
