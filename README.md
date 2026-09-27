# YAVE (Yet Another Video Editor)

TypeScript + React 製のハイエンド動画編集アプリケーション。同一コードベースからブラウザ版 (GitHub Pages 上での静的配信) とデスクトップ版 (Electron) の両方を提供します。

> **開発状況**:
> 現在は新アーキテクチャの設計段階です。実装は [移行マイルストーン (doc/design/14-migration.md)](doc/design/14-migration.md#144-移行マイルストーン) に従って順次進められます。
> 以前の C++17 / Qt 6 実装は `legacy-qt` タグ (`git checkout legacy-qt`) を参照してください。

---

## 🌐 公開 URL (予定)

- **Web 版 (GitHub Pages)**: [https://gadget114514.github.io/YetAnotherVideoEditor/](https://gadget114514.github.io/YetAnotherVideoEditor/)

---

## 📖 詳細設計書

全体の詳細設計は `doc/design.md` および `doc/design/` 配下に完全文書化されています。

| 章 | 設計ドキュメント |
|---|---|
| 目次 | [全体方針 & 目次](doc/design.md) |
| 1 | [全体アーキテクチャ設計](doc/design/01-architecture.md) |
| 2 | [ディレクトリ構成](doc/design/02-directory-layout.md) |
| 3 | [タイムライン & レンダリングエンジン](doc/design/03-timeline-render.md) |
| 4 | [ビデオエンジン (WebCodecs + GPU)](doc/design/04-video-engine.md) |
| 5 | [オーディオエンジン](doc/design/05-audio-engine.md) |
| 6 | [字幕エンジン](doc/design/06-subtitle-engine.md) |
| 7 | [マルチモーダル生成AIエンジン](doc/design/07-ai-orchestrator.md) |
| 8 | [プラグインシステム](doc/design/08-plugin-host.md) |
| 9 | [プロジェクト保存 (JSON)](doc/design/09-project-io.md) |
| 10 | [国際化 (日英切替)](doc/design/10-i18n.md) |
| 11 | [型定義リファレンス](doc/design/11-type-reference.md) |
| 12 | [ビルド・デプロイ設計](doc/design/12-build-deploy.md) |
| 13 | [AIトラック (演出指示 / 絵コンテ)](doc/design/13-ai-track.md) |
| 14 | [移行ガイド & テスト対応表](doc/design/14-migration.md) |

---

## 🏗 ディレクトリ構成 (予定)

```
/
├── apps/
│   ├── web/          Vite SPA エントリ (GitHub Pages 向け)
│   └── electron/     Electron main / preload (デスクトップ向け)
├── packages/
│   ├── core/         純粋なドメインロジック (Rational, Timeline, Commands)
│   ├── io/           プロジェクト JSON / SRT / VTT 入出力
│   ├── engine/       WebCodecs, WebGL2, AudioWorklet, AI 実行
│   ├── platform/     PlatformHost アダプタ (Web / Electron)
│   ├── plugin-sdk/   サードパーティ向け JS プラグイン公開 API
│   └── ui/           React コンポーネント, flexlayout-react 8 パネル
├── locales/          多言語辞書 (ja.json, en.json)
└── tests/            Playwright E2E テスト & フィクスチャ
```

---

## 🛠 予定コマンド

```bash
# 依存関係のインストール
pnpm install

# ブラウザ版の開発サーバー起動
pnpm dev:web

# Electron 版の開発起動
pnpm dev:electron

# 単体テスト実行 (Vitest)
pnpm test

# E2E テスト実行 (Playwright)
pnpm test:e2e
```

---

## 📄 ライセンス

TBD
