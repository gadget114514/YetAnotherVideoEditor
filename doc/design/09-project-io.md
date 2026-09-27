# 9. プロジェクト保存 (JSON)

[← 目次に戻る](../design.md)

---

## 9.1 方針

- フォーマットは **JSON**。人間が読め、Git 等で diff が取れ、万一の破損時にも手動で復旧可能なテキスト形式を維持する。
- 拡張子は標準で `.yave`。中身は UTF-8 の JSON。
- **無限レイヤー構造は `tracks` 配列の順序そのもの**として表現する。
- **AI 生成パラメータ・字幕エフェクトスタック・プラグイン設定を完全に永続化**する。
- `schemaVersion` は **4** とする。マイグレーション関数により旧バージョン (v1〜v3) を確実に読み込める前方互換性を保つ。
- **すべてのパスはプロジェクトファイルからの相対パス** (`/` 区切り)。

### 9.1.1 保存形式の種別

| 形式 | 拡張子 | 内容 | 主な用途 |
|---|---|---|---|
| **標準プロジェクト** | `.yave` | インデント付き JSON テキストファイル | 通常の編集保存。同階層または `assets/` フォルダの素材を参照 |
| **単一アーカイブ** | `.yavez` | ZIP アーカイブ (中身: `project.json` + `assets/`) | Web 版の非対応ブラウザ (Firefox/Safari) での保存、プロジェクト共有・配布 |
| **自動保存** | IndexedDB | JSON 文字列 (60 秒周期) | クラッシュからの復旧用 |

---

## 9.2 スキーマ全体 (v4)

```json
{
  "schemaVersion": 4,
  "application": { "name": "YAVE", "version": "2.0.0" },
  "savedAt": "2026-09-27T12:00:00Z",

  "project": {
    "name": "My Project",
    "timebase": { "num": 1001, "den": 60000 },
    "canvasSize": { "width": 1920, "height": 1080 },
    "sampleRate": 48000,
    "channels": 2,
    "duration": 108000,
    "playhead": 3600,
    "workRange": { "start": 0, "duration": 108000 },
    "colorSpace": "bt709"
  },

  "assets": [
    {
      "id": "1f2e3d4c-...",
      "path": "assets/interview.mp4",
      "kind": "video",
      "originalPath": "interview.mp4",
      "hash": "sha256:ab12...",
      "duration": 54000,
      "frameRate": { "num": 1001, "den": 30000 },
      "resolution": { "width": 1920, "height": 1080 },
      "hasAudio": true,
      "generatedByTaskId": null
    }
  ],

  "subtitleStylePresets": [
    {
      "id": "default",
      "name": "Default",
      "style": { "fontFamily": "Noto Sans JP", "fontPointSize": 48.0 }
    }
  ],

  "library": {
    "folders": [
      { "id": "f001-...", "parentId": null, "name": "撮影素材" },
      { "id": "f002-...", "parentId": "f001-...", "name": "インタビュー" }
    ],
    "assignments": { "1f2e3d4c-...": "f002-..." }
  },

  "storyBible": {
    "artStyle": "水彩調のアニメーション、柔らかい光",
    "negativePrompt": "低品質, 文字, 余分な指",
    "promptPrefix": "",
    "promptSuffix": "",
    "characters": [
      {
        "id": "3b1e-...",
        "key": "aoi",
        "name": "葵",
        "appearance": "黒髪ショート、紺のセーラー服",
        "promptFragment": "short black hair, navy sailor uniform",
        "voiceId": "piper-ja-female-1",
        "referenceImage": {
          "source": "filePath",
          "filePath": "assets/bible/aoi.png",
          "strength": 1.0
        }
      }
    ],
    "locations": [
      {
        "id": "9c22-...",
        "key": "rooftop",
        "name": "校舎の屋上",
        "promptFragment": "school rooftop, chain-link fence, morning haze"
      }
    ],
    "roleDefaults": {
      "mainVideo": { "modelId": "wan2.2-i2v-14b", "steps": 30 },
      "narration": { "modelId": "piper-ja", "targetLufs": -16.0 }
    },
    "promptTemplates": {
      "mainVideo": "{{artStyle}}. {{location}}. {{characters}}. {{description}} {{camera}} {{mood}}"
    }
  },

  "tracks": [
    {
      "id": "aaaa-...",
      "name": "Background",
      "type": "video",
      "visible": true,
      "locked": false,
      "muted": false,
      "solo": false,
      "opacity": 1.0,
      "blendMode": "normal",
      "height": 64,
      "color": "#3a5f8a",
      "clips": [ /* 9.4 参照 */ ],
      "transitions": [ /* 9.3.2 参照 */ ],
      "effectChain": []
    },
    {
      "id": "eeee-...",
      "name": "絵コンテ",
      "type": "storyboard",
      "visible": true,
      "height": 96,
      "color": "#7a5f3a",
      "aiRole": "",
      "storyboardTrackId": null,
      "roleDefaults": { "mainVideo": { "steps": 24 } },
      "clips": [ /* 9.4.5 CutClip */ ]
    }
  ],

  "aiTasks": [ /* 9.5 参照 */ ],
  "markers": [ { "frame": 1800, "name": "Intro end", "color": "#ff8800" } ],
  "masterAudio": {
    "gain": 1.0,
    "effectChain": []
  },
  "settings": {
    "pdcEnabled": true,
    "proxyEnabled": true,
    "autoCommitAi": false
  }
}
```

> **`zOrder` フィールドを持たない設計 (継承)**:
> `tracks` 配列の順序そのものが Z オーダーであり、index 0 が最背面、末尾が最前面となる。二重管理によるバグを構造的に排除する。

---

## 9.3 トラック配列と AI トラック

### 9.3.1 AI トラック関連フィールド
- `aiRole`: このトラックが担う出力役割 (`mainVideo`, `narration`, `bgm`, `se`, `subtitle`, `mask` 等)。
- `storyboardTrackId`: このトラックを生成した絵コンテトラックの ID。
- `roleDefaults`: 役割ごとの既定パラメータ (カスケード第 2 段)。

### 9.3.2 `transitions`
クリップ境界に付く独立オブジェクト。クリップ自体を重ねずにトランジションを実現する ([3.10](03-timeline-render.md))。

```json
"transitions": [
  {
    "id": "t001-...",
    "transitionId": "yave.trans.dissolve",
    "fromClipId": "c001-...",
    "toClipId": "c002-...",
    "centerFrame": 1800,
    "duration": 30,
    "params": {}
  }
]
```

---

## 9.4 クリップ定義

### 9.4.1 VideoClip
```json
{
  "id": "c001-...",
  "type": "video",
  "range": { "start": 0, "duration": 1800 },
  "assetId": "1f2e3d4c-...",
  "sourceOffset": 300,
  "speed": 1.0,
  "reversed": false,
  "opacity": 1.0,
  "blendMode": "normal",
  "transform": {
    "position": { "x": 0.0, "y": 0.0 },
    "scale": { "x": 1.0, "y": 1.0 },
    "rotation": 0.0,
    "anchor": { "x": 0.5, "y": 0.5 }
  },
  "crop": { "x": 0.0, "y": 0.0, "width": 1.0, "height": 1.0 },
  "fadeIn": 0,
  "fadeOut": 15,
  "filters": [
    {
      "filterId": "yave.filter.colorAdjust",
      "enabled": true,
      "params": { "brightness": 0.05, "contrast": 1.1, "saturation": 1.0, "gamma": 1.0 }
    }
  ],
  "generatedByTaskId": null
}
```

### 9.4.2 AudioClip
```json
{
  "id": "c002-...",
  "type": "audio",
  "range": { "start": 0, "duration": 2400 },
  "assetId": "5a6b-...",
  "sourceOffset": 0,
  "gain": 1.0,
  "pan": 0.0,
  "fadeIn": 24,
  "fadeOut": 48,
  "fadeInCurve": "equalPower",
  "fadeOutCurve": "linear",
  "generatedByTaskId": "task-uuid-..."
}
```

### 9.4.3 SubtitleClip
```json
{
  "id": "c003-...",
  "type": "subtitle",
  "range": { "start": 120, "duration": 180 },
  "stylePresetId": "default",
  "text": {
    "plain": "これは字幕のテキストです。\n2 行目もあります。",
    "spans": [
      {
        "start": 4,
        "length": 2,
        "bold": true,
        "color": "#ffcc00",
        "ruby": "じまく"
      }
    ]
  },
  "styleOverride": {
    "fontPointSize": 56.0,
    "anchor": { "x": 0.5, "y": 0.85 }
  },
  "effectStack": [
    {
      "instanceId": "e001-...",
      "effectId": "yave.fade",
      "pluginId": "",
      "enabled": true,
      "params": { "inDuration": 0.3, "outDuration": 0.3, "curve": "easeInOut" }
    }
  ],
  "wordTimings": [],
  "generatedByTaskId": null
}
```

### 9.4.4 CutClip (AI トラック演出指示)
```json
{
  "id": "cut-0001-...",
  "type": "cut",
  "range": { "start": 0, "duration": 480 },
  "label": "",
  "slug": "屋上・朝",
  "description": "葵が屋上のフェンス越しに街を見下ろす。風で髪が揺れる。",
  "dialogue": "……今日で、最後か。",
  "mood": "静か / 寂しい",
  "characterIds": ["3b1e-..."],
  "locationId": "9c22-...",
  "camera": { "size": "medium", "angle": "eyeLevel", "movement": "slowPushIn", "note": "" },
  "transitionIn": "cut",
  "transitionOut": "dissolve",
  "board": {
    "origin": "userFile",
    "assetId": "b001-...",
    "sourceTrackId": null,
    "sourceFrame": 0,
    "generatedByTaskId": null
  },
  "continuity": {
    "mode": "fromBoardImage",
    "fromCutId": null,
    "strength": 0.9,
    "sceneBreak": false
  },
  "status": "inReview",
  "reviewNote": "",
  "paramPatch": { "seed": 987654321 },
  "biblePatch": {},
  "outputs": [
    {
      "id": "ob-1",
      "role": "mainVideo",
      "roleTag": "",
      "enabled": true,
      "resolveMode": "auto",
      "resolvedTrackId": "gggg-...",
      "trackNameHint": "AI Video",
      "derivedFromBindingId": null,
      "leadInFrames": 0,
      "leadOutFrames": 15,
      "paramPatch": { "modelId": "wan2.2-i2v-14b" },
      "promptLock": { "locked": false, "prompt": "", "negativePrompt": "", "lockedAgainstHash": "" },
      "lastTaskId": "task-uuid-...",
      "committedClipIds": ["c001-..."],
      "committedSpecHash": "sha256:ab12...",
      "committedUpstreamHash": "sha256:cd34...",
      "state": "committed"
    }
  ],
  "generatedByTaskId": null
}
```

> **カット番号を保持しない設計 (継承)**:
> カット番号はトラック内の配列順序から導出する。`zOrder` を持たないのと同じ理由であり、並び替えによる番号の食い違いを防止する。手動採番したい場合は `label` に保持する。

---

## 9.5 AI タスクの保存

生成パラメータをプロジェクト内に完全永続化し、キャッシュが消去されても再生成できることを保証する。

```json
"aiTasks": [
  {
    "id": "task-uuid-...",
    "state": "committed",
    "createdAt": "2026-09-27T12:00:00Z",
    "completedAt": "2026-09-27T12:02:15Z",
    "retryCount": 0,
    "errorMessage": "",
    "batchId": "batch-uuid-...",
    "purpose": "commit",
    "cutRef": { "cutClipId": "cut-0001-...", "bindingId": "ob-1" },
    "params": {
      "kind": "video",
      "modelId": "wan2.2-i2v-14b",
      "providerId": "comfyui-remote",
      "prompt": "夕暮れの海岸を歩く人物、シネマティック、35mm",
      "negativePrompt": "低品質, ぼやけ, 文字",
      "seed": 987654321,
      "steps": 30,
      "guidanceScale": 7.5
    },
    "assets": []
  }
]
```

---

## 9.6 スキーママイグレーション (v3 → v4)

`schemaVersion` を **4** に更新し、マイグレーション関数 `migrate_3_to_4` を定義する。

### 3 → 4 の変更点
1. **ネイティブプラグインの無効化・保全**:
   - `masterAudio.effectChain` および `tracks[].effectChain` 内の VST3 / AviUtl エントリを検知。
   - 既存データを破棄せず、`{"disabled": true, "legacy": { ...元データ... }}` としてラップ。
2. **その他フィールドの完全互換**:
   - タイムライン、トラック、クリップ、AI トラック、Story Bible 等の構造は v3 と完全互換であり、そのまま読み込める。

```ts
// packages/io/src/SchemaMigration.ts
export function migrate_3_to_4(root: any): void {
  // masterAudio の effectChain 移行
  if (root.masterAudio?.effectChain) {
    root.masterAudio.effectChain = root.masterAudio.effectChain.map((fx: any) => {
      if (fx.kind === 'vst3' || fx.kind === 'aviutl') {
        return { disabled: true, legacy: fx };
      }
      return fx;
    });
  }

  // 各トラックの effectChain 移行
  if (Array.isArray(root.tracks)) {
    for (const t of root.tracks) {
      if (Array.isArray(t.effectChain)) {
        t.effectChain = t.effectChain.map((fx: any) => {
          if (fx.kind === 'vst3' || fx.kind === 'aviutl') {
            return { disabled: true, legacy: fx };
          }
          return fx;
        });
      }
    }
  }

  root.schemaVersion = 4;
}
```

---

## 9.7 未知フィールドの保持 (前方互換性)

古いバージョンのアプリで新しいプロジェクトファイルを開き、再度保存した際に、認識できない新しいフィールドが消滅しないよう、**`Clip`, `Track`, およびプロジェクトルートのすべてで未知フィールドをそのまま保持して書き戻す**。

```ts
export class Clip {
  private unknownFields_: Record<string, any> = {};

  setUnknownFields(fields: Record<string, any>): void {
    this.unknownFields_ = { ...fields };
  }

  get unknownFields(): Record<string, any> {
    return this.unknownFields_;
  }
}
```

---

## 9.8 保存の原子性 (Atomic Save)

保存中のブラウザ終了やクラッシュによってファイルが破損することを防ぐ。

- **Web (File System Access API)**:
  `FileSystemFileHandle.createWritable()` は、一時的なスワップファイルに対して書き込みを行い、`stream.close()` が呼ばれた瞬間にブラウザのネイティブ層で元のファイルとアトミックに置換される。
- **Electron (Node.js)**:
  同階層の一時ファイル (`<path>.tmp`) に JSON を書き込み、`fs.promises.rename()` でアトミックに置換する。

---

## 9.9 自動保存とクラッシュリカバリ

- **周期**: 編集操作が発生している場合、60 秒周期でバックグラウンド実行。
- **保存先**: IndexedDB (オブジェクトストア: `yave_autosave`) にプロジェクト JSON を保存。
- **復旧**: アプリ起動時に IndexedDB 内の前回未保存セッションを検知し、「前回の編集内容が未保存で終了しました。復旧しますか?」ダイアログを提示する。

---

## 9.10 JSON キーと enum の一元管理

文字列リテラルのタイポによるバグを防止するため、`JsonKeys.ts` および `EnumMapping.ts` に定数定義を集約する。

```ts
// packages/io/src/JsonKeys.ts
export const JsonKeys = {
  schemaVersion: 'schemaVersion',
  project: 'project',
  tracks: 'tracks',
  clips: 'clips',
  aiTasks: 'aiTasks',
  assets: 'assets',
  library: 'library',
  storyBible: 'storyBible',
  effectChain: 'effectChain',
  filters: 'filters',
  transitions: 'transitions',
} as const;
```

CI パイプラインにおいて、`packages/io/schema/project.v4.schema.json` を用いてフィクスチャ `tests/fixtures/sample_project.yave` の検証を自動実行する。
