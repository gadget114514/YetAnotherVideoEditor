# 9. プロジェクト保存 (JSON)

[← 目次に戻る](../design.md)

---

## 9.1 方針

- フォーマットは **JSON** (`QJsonDocument`)。人間が読め、diff が取れ、
  壊れたときに手で直せることを重視する。
- 拡張子は `.yave`。中身は UTF-8 の JSON。
- **無限レイヤー構造は `tracks` 配列**として自然に表現する。
- **AI 生成パラメータ・字幕エフェクトスタック・プラグイン設定を完全に永続化**する。
- `schemaVersion` を持ち、マイグレーション関数テーブルで前方互換を保つ。
- **すべてのパスはプロジェクトファイルからの相対パス**。

### 9.1.1 保存形式のオプション

| 形式 | 用途 |
|---|---|
| `QJsonDocument::Indented` (既定) | 通常保存。diff が取れる |
| `QJsonDocument::Compact` | 「軽量保存」オプション。1000 クリップ超のプロジェクト向け |
| CBOR (`QCborValue::fromJsonValue`) | 自動保存 (`.yave.autosave`)。速度優先 |

> 自動保存に CBOR を使う理由: 30 秒ごとに保存する場合、大規模プロジェクトでは
> JSON のテキスト化が無視できないコストになる。CBOR は同じデータモデルなので
> 相互変換が容易。

## 9.2 スキーマ全体

```json
{
  "schemaVersion": 3,
  "application": { "name": "YAVE", "version": "1.0.0" },
  "savedAt": "2026-08-24T15:04:05Z",

  "project": {
    "name": "My Project",
    "timebase": { "num": 1001, "den": 60000 },
    "canvasSize": { "width": 3840, "height": 2160 },
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
      "originalPath": "D:/footage/interview.mp4",
      "hash": "sha256:ab12...",
      "duration": 54000,
      "frameRate": { "num": 1001, "den": 30000 },
      "resolution": { "width": 3840, "height": 2160 },
      "hasAudio": true,
      "generatedByTaskId": null
    }
  ],

  "subtitleStylePresets": [
    {
      "id": "default",
      "name": "Default",
      "style": { "fontFamily": "Noto Sans JP", "fontPointSize": 48.0, "...": "..." }
    }
  ],

  "library": {
    "folders": [
      { "id": "f001-...", "parentId": null,      "name": "撮影素材" },
      { "id": "f002-...", "parentId": "f001-...", "name": "インタビュー" }
    ],
    "assignments": { "1f2e3d4c-...": "f002-..." }
  },

  "storyBible": {
    "artStyle": "水彩調のアニメーション、柔らかい光",
    "negativePrompt": "低品質, 文字, 余分な指",
    "characters": [ /* 9.3.1 参照 */ ],
    "locations":  [ /* 9.3.1 参照 */ ],
    "roleDefaults":    { "mainVideo": { "modelId": "wan2.2-i2v-14b", "steps": 30 } },
    "promptTemplates": { "mainVideo": "{{artStyle}}. {{description}} {{camera}}" }
  },

  "tracks": [ /* 9.3 参照。無限レイヤー */ ],

  "aiTasks":  [ /* 9.5 参照 */ ],
  "markers":  [ { "frame": 1800, "name": "Intro end", "color": "#ff8800" } ],
  "masterAudio": {
    "gain": 1.0,
    "effectChain": [ /* VST3 プラグイン。9.6 参照 */ ]
  },
  "settings": {
    "pdcEnabled": true,
    "proxyEnabled": true,
    "autoCommitAi": false
  }
}
```

### 9.2.1 `library` (メディアのフォルダ構成)

メディアライブラリパネル ([1.7.5](01-architecture.md)) のフォルダ木と、
アセットがどのフォルダに属するかを持つ。

| キー | 意味 |
|---|---|
| `folders[]` | `id` / `parentId` (ルート直下は `null`) / `name` |
| `assignments` | アセット ID -> フォルダ ID。未登録のアセットはルート直下として扱う |

存在しないフォルダ ID を指す `assignments` の要素は、読み込み時に**黙って捨てる**
(壊れたフォルダ参照でアセットが一覧から消えるほうが実害が大きい)。

> **エフェクトライブラリ側のフォルダをここに入れない理由**: あちらはアプリが持つ
> カタログの整理であり、プロジェクトの内容ではない。`QSettings` に置く
> ([1.7.5](01-architecture.md) の永続化表)。

## 9.3 トラック配列 (無限レイヤー)

配列の**順序がそのまま Z オーダー**。index 0 が最背面。

```json
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
    "id": "bbbb-...",
    "name": "Narration",
    "type": "audio",
    "gain": 0.8,
    "pan": 0.0,
    "muted": false,
    "solo": false,
    "clips": [ ... ],
    "effectChain": [ /* VST3 */ ]
  },
  {
    "id": "cccc-...",
    "name": "Subtitles JA",
    "type": "subtitle",
    "visible": true,
    "defaultStylePresetId": "default",
    "clips": [ /* SubtitleClip */ ]
  },
  {
    "id": "dddd-...",
    "name": "AI Generated",
    "type": "aiGenerated",
    "clips": [ ... ]
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
  },
  {
    "id": "gggg-...",
    "name": "AI Video",
    "type": "video",
    "aiRole": "mainVideo",
    "storyboardTrackId": "eeee-...",
    "clips": [ /* 生成された VideoClip */ ]
  }
]
```

> **`zOrder` フィールドを持たない**ことが重要。配列順序が唯一の真実であり、
> 二重管理によるずれを構造的に排除する。

### 9.3.1 AIトラック関連のトラックフィールド

以下は全トラック型で任意。既定は `""` / `null` / `{}`。詳細は [13.3](13-ai-track.md)。

| キー | 意味 |
|---|---|
| `aiRole` | このトラックが担う出力役割。`mainVideo` / `narration` / `bgm` / `se` / `subtitle` / `mask` など。空なら手動作成の通常トラック |
| `storyboardTrackId` | このトラックを生成した絵コンテトラックの ID。役割の解決はこの ID でスコープされる |
| `roleDefaults` | 役割ごとの既定パラメータ (カスケード第 2 段) |

`storyBible` はルート直下に置く。実 JSON は [13.10.1](13-ai-track.md) を参照。

```json
"storyBible": {
  "artStyle": "水彩調のアニメーション、柔らかい光",
  "negativePrompt": "低品質, 文字, 余分な指",
  "promptPrefix": "",
  "promptSuffix": "",
  "characters": [
    { "id": "3b1e-...", "key": "aoi", "name": "葵",
      "appearance": "黒髪ショート、紺のセーラー服",
      "promptFragment": "short black hair, navy sailor uniform",
      "voiceId": "piper-ja-female-1",
      "referenceImage": { "source": "filePath",
                          "filePath": "assets/bible/aoi.png", "strength": 1.0 } }
  ],
  "locations": [
    { "id": "9c22-...", "key": "rooftop", "name": "校舎の屋上",
      "promptFragment": "school rooftop, chain-link fence, morning haze" }
  ],
  "roleDefaults": {
    "mainVideo": { "modelId": "wan2.2-i2v-14b", "steps": 30 },
    "narration": { "modelId": "piper-ja", "targetLufs": -16.0 }
  },
  "promptTemplates": {
    "mainVideo": "{{artStyle}}. {{location}}. {{characters}}. {{description}} {{camera}} {{mood}}"
  }
}
```

> **キャラクターとロケーションが `id` (UUID) と `key` の両方を持つ理由**:
> カットからの参照は §3.3 に従い UUID で行う。一方 `key` は
> プロンプト内での参照と L1 モデル出力の照合に使う人間可読キーである。
> `key` を変えても既存カットの参照は壊れない。

### 9.3.2 `transitions` (クリップ境界のトランジション)

映像トラックのみ。クリップ境界に付く独立オブジェクト ([3.10](03-timeline-render.md))。

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

| キー | 意味 |
|---|---|
| `fromClipId` / `toClipId` | 境界の前後のクリップ。端の境界では `toClipId` が `null` (黒との合成) |
| `centerFrame` | 境界フレーム。`fromClipId` の終端 == `toClipId` の始端 |
| `duration` | 全体長。前後へ半分ずつ伸びる |

読み込み時に**不変条件を検証する** ([3.2.2](03-timeline-render.md) の 4 / 5)。
参照先クリップが無い / 境界が一致しない / ハンドルが足りない要素は捨て、
`LoadResult::warnings` へ 1 件ずつ積む。手で編集された JSON を無条件に信じない。

## 9.4 クリップ

共通フィールド + 型ごとの追加フィールド。
`filters` ([9.4.1](#941-videoclip)) はすべての映像系クリップ (video / image / color /
title / aiPlaceholder) が持てる共通フィールドである。

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
    { "filterId": "yave.filter.colorAdjust",
      "enabled": true,
      "params": { "brightness": 0.05, "contrast": 1.1, "saturation": 1.0, "gamma": 1.0 } },
    { "filterId": "aviutl:縁取り", "enabled": true, "params": { "size": 4 } }
  ],
  "generatedByTaskId": null
}
```

`filters` はビデオフィルタースタック ([3.9](03-timeline-render.md))。**配列順が適用順**。
組み込みフィルタは `yave.filter.*`、プラグイン由来は `<host>:<nativeId>` の形にする。

> **schema v2 の `effectChain` からの改称**: クリップに付くものを `effectChain`、
> トラックに付くものも `effectChain` と呼んでいたため、どちらの話をしているのか
> 区別できなかった。クリップ側を `filters` に改め、`effectChain` は
> **トラック / マスターのオーディオ処理チェーン専用**とする。移行は 9.11 の
> `migrate_2_to_3` が行う。

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

**字幕区間のすべての情報がここに入る。**

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
      "params": {
        "inDuration": 0.3,
        "outDuration": 0.3,
        "curve": "easeInOut"
      }
    },
    {
      "instanceId": "e002-...",
      "effectId": "yave.typewriter",
      "pluginId": "",
      "enabled": true,
      "params": {
        "charsPerSecond": 18.0,
        "startDelay": 0.1,
        "cursorVisible": false
      }
    },
    {
      "instanceId": "e003-...",
      "effectId": "com.example.glitch",
      "pluginId": "com.example.effects",
      "enabled": true,
      "params": {
        "intensity": 0.4,
        "seed": 12345
      }
    }
  ],
  "wordTimings": [
    { "charStart": 0,  "charLength": 4, "startSec": 0.00, "endSec": 0.42 },
    { "charStart": 4,  "charLength": 2, "startSec": 0.42, "endSec": 0.75 }
  ],
  "generatedByTaskId": "task-uuid-..."
}
```

> `wordTimings` は STT 由来の場合のみ存在する。カラオケエフェクトが参照する。
> 手入力の字幕には存在しない (その場合カラオケエフェクトは均等割りにフォールバック)。

### 9.4.4 AiPlaceholderClip

生成中 / 未コミットの区間。

```json
{
  "id": "c004-...",
  "type": "aiPlaceholder",
  "range": { "start": 3600, "duration": 480 },
  "taskId": "task-uuid-...",
  "state": "running"
}
```

### 9.4.5 CutClip

AIトラック (`"type":"storyboard"`) にのみ置ける、カット単位の演出指示。
**メディアではなく仕様であり、再生成の source of truth である。**
各フィールドの意味は [13.3.2](13-ai-track.md) / [13.3.3](13-ai-track.md)。

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
      "promptLock": { "locked": false, "prompt": "", "negativePrompt": "",
                      "lockedAgainstHash": "" },
      "lastTaskId": "task-uuid-...",
      "committedClipIds": ["c001-..."],
      "committedSpecHash": "sha256:ab12...",
      "committedUpstreamHash": "sha256:cd34...",
      "state": "committed"
    },
    {
      "id": "ob-2",
      "role": "narration",
      "resolveMode": "auto",
      "resolvedTrackId": "hhhh-...",
      "paramPatch": { "voiceId": "piper-ja-female-1" },
      "committedClipIds": ["c002-..."],
      "state": "committed"
    },
    {
      "id": "ob-3",
      "role": "subtitle",
      "resolveMode": "auto",
      "resolvedTrackId": null,
      "derivedFromBindingId": "ob-2",
      "state": "notGenerated"
    }
  ],

  "generatedByTaskId": null
}
```

> **カット番号はフィールドとして持たない。** トラック内の配列順序から導出する。
> `zOrder` を持たないのと同じ理由である (9.3 冒頭)。
> 手動採番したい場合のために `label` を別に持ち、空なら UI が導出番号を表示する。
>
> `paramPatch` / `biblePatch` は**疎なパッチ**である。
> キーが存在すること自体が「その段で上書きされている」ことを意味し、
> 「継承に戻す」はキーの削除で表現される ([13.3.5](13-ai-track.md))。

### 9.4.6 TitleClip

`SubtitleClip` ([9.4.3](#943-subtitleclip)) と同じフィールドを持ち、`type` だけが違う。
差分は次の 2 点のみ。

```json
{
  "id": "c006-...",
  "type": "title",
  "range": { "start": 600, "duration": 180 },
  "presetId": "yave.title.center",
  "text": "第 1 章",
  "style": { "...": "SubtitleClip と同じ" },
  "effectStack": [ /* 9.4.3 と同じ */ ],
  "filters": []
}
```

| キー | 意味 |
|---|---|
| `presetId` | 生成元のタイトルプリセット。以後の編集で外れても記録として残す |
| 置けるトラック | 映像トラック。字幕トラックにも置けるが、書き出し時の字幕分離の対象にはならない ([3.11](03-timeline-render.md)) |

## 9.5 AI タスク

**生成パラメータをすべて保存する。** これにより、キャッシュが消えても再生成できる。

```json
"aiTasks": [
  {
    "id": "task-uuid-...",
    "state": "committed",
    "createdAt": "2026-08-24T14:00:00Z",
    "completedAt": "2026-08-24T14:03:22Z",
    "retryCount": 0,
    "errorMessage": "",
    "batchId": "batch-uuid-...",
    "purpose": "commit",
    "cutRef": { "cutClipId": "cut-0001-...", "bindingId": "ob-1" },
    "params": {
      "kind": "video",
      "targetTrackId": "dddd-...",
      "range": { "start": 3600, "duration": 480 },
      "modelId": "wan2.2-i2v-14b",
      "providerId": "comfyui-local",
      "prompt": "夕暮れの海岸を歩く人物、シネマティック、35mm",
      "negativePrompt": "低品質, ぼやけ, 文字",
      "seed": 987654321,
      "steps": 30,
      "guidanceScale": 7.5,

      "videoMode": "imageToVideo",
      "i2vRefMode": "bothEnds",
      "startReference": {
        "source": "timelineFrame",
        "sourceTrackId": "aaaa-...",
        "sourceFrame": 3600,
        "strength": 1.0
      },
      "endReference": {
        "source": "filePath",
        "filePath": "assets/refs/sunset_end.png",
        "strength": 0.85
      },
      "videoReference": null,
      "outputResolution": { "width": 1280, "height": 720 },
      "outputFrameRate": { "num": 1, "den": 30 },
      "loopSeamless": false,

      "replaceExistingClips": false,
      "createNewTrack": false,

      "extraParams": {
        "sampler": "dpmpp_2m",
        "scheduler": "karras",
        "fpsMethod": "rife",
        "fitMethod": "speed"
      }
    },
    "assets": [
      {
        "type": "video",
        "path": ".yave_cache/gen/task-uuid-.../final.mov",
        "collected": false,
        "resolution": { "width": 1280, "height": 720 },
        "durationFrames": 480,
        "frameRate": { "num": 1001, "den": 60000 },
        "metadata": { "seed": 987654321, "modelVersion": "2.2.1" }
      }
    ]
  }
]
```

`"collected": true` の場合、`path` は `assets/generated/...` を指し、
プロジェクトと一緒に持ち運べる ([7.8.3](07-ai-orchestrator.md) 参照)。

### 9.5.1 AIトラック由来のフィールド

| キー | 意味 |
|---|---|
| `batchId` | `StoryboardBatchJob` に属する場合のバッチ ID。タスクパネルのグループ化に使う |
| `purpose` | `"commit"` / `"animaticPreview"`。プレビュー用 TTS はトラックへコミットしない |
| `cutRef` | どのカットのどの出力バインディングから投げられたか |

`cutRef` が設定されているタスクでは、配置は `OutputBinding` が決める。
`params.targetTrackId` / `createNewTrack` / `replaceExistingClips` は無視される
([13.14.6](13-ai-track.md))。

### 9.5.2 永続化ポリシー

**プロジェクト JSON に書き出すのは、`state` が `cached` または `committed` で、
かつ生きた `OutputBinding` から参照されているタスクだけ**とする。
それ以外 (`failed` / `cancelled` / 参照が切れたもの) は
`.yave_cache/tasks.json` にのみ置く。

> 100 カットのプロジェクトを 5 回作り直すと、素直に全部書けば数千件のタスクレコードが
> 積み上がり、[9.14](#914-ファイルサイズの目安) のサイズ予算を破る。
> UI に「生成履歴を整理」を用意し、参照されていないタスクを一括削除できるようにする。

## 9.6 プラグイン設定

### 9.6.1 VST3

```json
"effectChain": [
  {
    "kind": "vst3",
    "nativeId": "5653544643666D70466162466C7420",
    "name": "FabFilter Pro-Q 3",
    "vendor": "FabFilter",
    "enabled": true,
    "bypassed": false,
    "state": "AAAAgD8AAAAAAAAAAA...(Base64)...",
    "latencySamples": 0
  }
]
```

- `state` は `Vst3Host::saveState()` の Base64。
- **プラグイン未インストール時も `state` をそのまま保持して再保存する。**

### 9.6.2 AviUtl フィルタ (Windows)

```json
{
  "kind": "aviutl",
  "dllPath": "../aviutl/plugins/example.auf",
  "filterIndex": 0,
  "filterName": "サンプルフィルタ",
  "enabled": true,
  "track": [100, 50, 0, 0],
  "check": [1, 0],
  "exData": "AAECAwQF...(Base64)...",
  "unavailableOnThisPlatform": false
}
```

**macOS で読み込んだ場合**:
- `"unavailableOnThisPlatform": true` を立ててそのまま保持する。
- レンダリング時はそのエフェクトをスキップする。
- UI に「このエフェクトは Windows でのみ利用できます」と表示する。
- **保存時にはそのまま書き戻す。** Windows に戻せばそのまま動く。

> これも [8.7.1](08-plugin-host.md) と同じ原則。
> クロスプラットフォームで往復するプロジェクトで設定を失わせない。

### 9.6.3 字幕エフェクト

[9.4.3](#943-subtitleclip) の `effectStack` を参照。

## 9.7 ProjectSerializer

```cpp
// src/io/ProjectSerializer.h
namespace yave::io {

struct SaveOptions
{
    bool     indented           = true;
    bool     collectGeneratedAssets = false;
    bool     collectSourceAssets    = false;
    bool     embedThumbnails    = false;
};

struct LoadResult
{
    bool        ok = false;
    QString     errorMessage;
    QStringList warnings;          // 未解決アセット、未インストールプラグイン等
    int         loadedSchemaVersion = 0;
    bool        migrated = false;
};

class ProjectSerializer
{
public:
    static constexpr int kCurrentSchemaVersion = 2;   // 2: AIトラック (13 章)

    /// 保存。アトミックに書く (テンポラリへ書いてから rename)。
    static bool save(const Project& project, const QString& path,
                     const SaveOptions& opts, QString* errorOut = nullptr);

    /// 読み込み。schemaVersion が古ければマイグレーションを適用する。
    static LoadResult load(Project* project, const QString& path);

    /// 自動保存 (CBOR)
    static bool saveAutosave(const Project& project, const QString& path);
    static LoadResult loadAutosave(Project* project, const QString& path);

    // --- 個別シリアライズ (テストしやすいよう public にする) ---
    static QJsonObject serializeProject(const Project& p, const SaveOptions& o);
    static QJsonArray  serializeTracks(const Timeline& tl, const SaveOptions& o);
    static QJsonObject serializeTrack(const Track& t, const SaveOptions& o);
    static QJsonObject serializeClip(const Clip& c, const SaveOptions& o);
    static QJsonObject serializeSubtitleClip(const subtitle::SubtitleClip& c);
    static QJsonArray  serializeEffectStack(
                           const std::vector<subtitle::SubtitleEffectInstance>& stack);
    static QJsonObject serializeAiParams(const ai::AiGenerationParams& p);
    static QJsonObject serializeAiTask(const ai::AiGenerationTask& t);
    static QJsonArray  serializeVst3Chain(const std::vector<plugin::Vst3Host*>& chain);

    static bool deserializeProject(Project* p, const QJsonObject& o, LoadResult* r);
    static std::unique_ptr<Track> deserializeTrack(const QJsonObject& o,
                                                   Project* p, LoadResult* r);
    static std::shared_ptr<Clip>  deserializeClip(const QJsonObject& o,
                                                   Project* p, LoadResult* r);
    static ai::AiGenerationParams deserializeAiParams(const QJsonObject& o);

private:
    static void applyMigrations(QJsonObject& root, int fromVersion, LoadResult* r);
};

} // namespace yave::io
```

具体的な実装は [12章 スニペット 3](12-snippets.md) を参照。

## 9.8 JSON キーの一元管理

キー文字列をリテラルで散らすと、タイプミスが実行時まで発覚しない。
定数として一箇所に集める。

```cpp
// src/io/JsonKeys.h
namespace yave::io::keys {

inline constexpr auto kSchemaVersion = "schemaVersion";
inline constexpr auto kProject       = "project";
inline constexpr auto kTracks        = "tracks";
inline constexpr auto kClips         = "clips";
inline constexpr auto kAiTasks       = "aiTasks";
inline constexpr auto kAssets        = "assets";
inline constexpr auto kLibrary       = "library";
inline constexpr auto kFolders       = "folders";
inline constexpr auto kAssignments   = "assignments";
inline constexpr auto kParentId      = "parentId";

// Track
inline constexpr auto kTrackId       = "id";
inline constexpr auto kTrackName     = "name";
inline constexpr auto kTrackType     = "type";
inline constexpr auto kTrackVisible  = "visible";
inline constexpr auto kTrackLocked   = "locked";
inline constexpr auto kEffectChain   = "effectChain";   // トラック / マスターの音声チェーン専用
inline constexpr auto kTransitions   = "transitions";

// Transition (9.3.2)
inline constexpr auto kTransitionId  = "transitionId";
inline constexpr auto kFromClipId    = "fromClipId";
inline constexpr auto kToClipId      = "toClipId";
inline constexpr auto kCenterFrame   = "centerFrame";

// Clip
inline constexpr auto kClipId        = "id";
inline constexpr auto kClipType      = "type";
inline constexpr auto kRange         = "range";
inline constexpr auto kRangeStart    = "start";
inline constexpr auto kRangeDuration = "duration";
inline constexpr auto kAssetId       = "assetId";
inline constexpr auto kSourceOffset  = "sourceOffset";
inline constexpr auto kFilters       = "filters";       // ビデオフィルタースタック (9.4.1)
inline constexpr auto kFilterId      = "filterId";
inline constexpr auto kPresetId      = "presetId";      // TitleClip (9.4.6)

// SubtitleClip
inline constexpr auto kText          = "text";
inline constexpr auto kTextPlain     = "plain";
inline constexpr auto kTextSpans     = "spans";
inline constexpr auto kStylePresetId = "stylePresetId";
inline constexpr auto kStyleOverride = "styleOverride";
inline constexpr auto kEffectStack   = "effectStack";
inline constexpr auto kWordTimings   = "wordTimings";

// SubtitleEffectInstance
// kEnabled / kParams は VideoFilterInstance (9.4.1) と共用する
inline constexpr auto kInstanceId    = "instanceId";
inline constexpr auto kEffectId      = "effectId";
inline constexpr auto kPluginId      = "pluginId";
inline constexpr auto kEnabled       = "enabled";
inline constexpr auto kParams        = "params";

// AiGenerationParams
inline constexpr auto kKind          = "kind";
inline constexpr auto kTargetTrackId = "targetTrackId";
inline constexpr auto kModelId       = "modelId";
inline constexpr auto kProviderId    = "providerId";
inline constexpr auto kPrompt        = "prompt";
inline constexpr auto kNegativePrompt= "negativePrompt";
inline constexpr auto kSeed          = "seed";
inline constexpr auto kVideoMode     = "videoMode";
inline constexpr auto kI2vRefMode    = "i2vRefMode";
inline constexpr auto kStartReference= "startReference";
inline constexpr auto kEndReference  = "endReference";
inline constexpr auto kVideoReference= "videoReference";
inline constexpr auto kExtraParams   = "extraParams";

// AIトラック / 絵コンテ (13 章)
inline constexpr auto kStoryBible     = "storyBible";
inline constexpr auto kCharacters     = "characters";
inline constexpr auto kLocations      = "locations";
inline constexpr auto kRoleDefaults   = "roleDefaults";
inline constexpr auto kPromptTemplates= "promptTemplates";
inline constexpr auto kArtStyle       = "artStyle";
inline constexpr auto kPromptPrefix   = "promptPrefix";
inline constexpr auto kPromptSuffix   = "promptSuffix";
inline constexpr auto kPromptFragment = "promptFragment";
inline constexpr auto kAppearance     = "appearance";
inline constexpr auto kAiRole         = "aiRole";
inline constexpr auto kStoryboardTrackId = "storyboardTrackId";

// CutClip
inline constexpr auto kSlug           = "slug";
inline constexpr auto kLabel          = "label";
inline constexpr auto kDescription    = "description";
inline constexpr auto kDialogue       = "dialogue";
inline constexpr auto kMood           = "mood";
inline constexpr auto kCamera         = "camera";
inline constexpr auto kCharacterIds   = "characterIds";
inline constexpr auto kLocationId     = "locationId";
inline constexpr auto kTransitionIn   = "transitionIn";
inline constexpr auto kTransitionOut  = "transitionOut";
inline constexpr auto kBoard          = "board";
inline constexpr auto kContinuity     = "continuity";
inline constexpr auto kStatus         = "status";
inline constexpr auto kReviewNote     = "reviewNote";
inline constexpr auto kParamPatch     = "paramPatch";
inline constexpr auto kBiblePatch     = "biblePatch";

// OutputBinding
inline constexpr auto kOutputs        = "outputs";
inline constexpr auto kRole           = "role";
inline constexpr auto kRoleTag        = "roleTag";
inline constexpr auto kResolveMode    = "resolveMode";
inline constexpr auto kResolvedTrackId= "resolvedTrackId";
inline constexpr auto kTrackNameHint  = "trackNameHint";
inline constexpr auto kDerivedFromBindingId = "derivedFromBindingId";
inline constexpr auto kLeadInFrames   = "leadInFrames";
inline constexpr auto kLeadOutFrames  = "leadOutFrames";
inline constexpr auto kPromptLock     = "promptLock";
inline constexpr auto kLockedAgainstHash = "lockedAgainstHash";
inline constexpr auto kCommittedClipIds  = "committedClipIds";
inline constexpr auto kCommittedSpecHash = "committedSpecHash";
inline constexpr auto kCommittedUpstreamHash = "committedUpstreamHash";

// AI タスク (9.5.1)
inline constexpr auto kBatchId        = "batchId";
inline constexpr auto kCutRef         = "cutRef";
inline constexpr auto kPurpose        = "purpose";

} // namespace yave::io::keys
```

## 9.9 enum ⇄ 文字列の変換

**enum を数値で保存しない。** 数値で保存すると、後から enum の途中に値を追加した瞬間に
既存プロジェクトが壊れる。

```cpp
// src/io/EnumMapping.h
namespace yave::io {

template <typename E>
struct EnumMap { static const std::vector<std::pair<E, const char*>>& table(); };

template <typename E>
QString enumToString(E v)
{
    for (const auto& [e, s] : EnumMap<E>::table())
        if (e == v) return QString::fromLatin1(s);
    return {};
}

template <typename E>
E enumFromString(const QString& s, E fallback)
{
    for (const auto& [e, str] : EnumMap<E>::table())
        if (s == QLatin1String(str)) return e;
    return fallback;
}

// 特殊化
template <> inline const std::vector<std::pair<TrackType, const char*>>&
EnumMap<TrackType>::table()
{
    static const std::vector<std::pair<TrackType, const char*>> t = {
        { TrackType::Video,       "video"       },
        { TrackType::Audio,       "audio"       },
        { TrackType::Subtitle,    "subtitle"    },
        { TrackType::AiGenerated, "aiGenerated" },
        { TrackType::Storyboard,  "storyboard"  },   // 13 章
        { TrackType::Unknown,     "unknown"     },
    };
    return t;
}

template <> inline const std::vector<std::pair<ai::VideoGenMode, const char*>>&
EnumMap<ai::VideoGenMode>::table()
{
    static const std::vector<std::pair<ai::VideoGenMode, const char*>> t = {
        { ai::VideoGenMode::TextToVideo,  "textToVideo"  },
        { ai::VideoGenMode::ImageToVideo, "imageToVideo" },
        { ai::VideoGenMode::VideoToVideo, "videoToVideo" },
    };
    return t;
}

template <> inline const std::vector<std::pair<ai::I2VReferenceMode, const char*>>&
EnumMap<ai::I2VReferenceMode>::table()
{
    static const std::vector<std::pair<ai::I2VReferenceMode, const char*>> t = {
        { ai::I2VReferenceMode::StartFrameOnly, "startFrameOnly" },
        { ai::I2VReferenceMode::EndFrameOnly,   "endFrameOnly"   },
        { ai::I2VReferenceMode::BothEnds,       "bothEnds"       },
    };
    return t;
}

} // namespace yave::io
```

### 9.9.1 AIトラック関連の enum 表

[13 章](13-ai-track.md)で追加する enum は、すべてここに特殊化を置く。
**enum を足して表を足し忘れると、保存はできるが読み込めないプロジェクトが生まれる。**

| enum | 文字列 |
|---|---|
| `ClipType` | 既存 + `cut` + `title` (9.4.6) |
| `OutputRole` | `mainVideo`, `mainVideoB`, `overlay`, `narration`, `bgm`, `se`, `subtitle`, `mask` |
| `TrackResolveMode` | `auto`, `existing`, `alwaysNew` |
| `OutputState` | `notGenerated`, `queued`, `running`, `cached`, `committed`, `failed`, `stale`, `blocked` |
| `CutStatus` | `notStarted`, `rough`, `inReview`, `approved` |
| `ContinuityMode` | `none`, `fromBoardImage`, `fromPreviousEnd`, `fromCutId` |
| `ShotSize` | `unspecified`, `extremeWide`, `wide`, `full`, `medium`, `closeUp`, `extremeCloseUp` |
| `CameraAngle` | `unspecified`, `eyeLevel`, `high`, `low`, `birdsEye`, `wormsEye`, `dutch` |
| `CameraMovement` | `unspecified`, `fixed`, `panLeft`, `panRight`, `tiltUp`, `tiltDown`, `dolly`, `slowPushIn`, `pullOut`, `handheld`, `crane`, `follow` |
| `TransitionKind` | `cut`, `dissolve`, `fadeToBlack`, `fadeFromBlack`, `wipe`, `matchCut` |
| `BoardImage::Origin` | `none`, `userFile`, `generated`, `timelineFrame` |
| `ai::GenerationKind` | 既存 + `storyboard` |
| `ai::GenerationPurpose` | `commit`, `animaticPreview` |
| `ai::PlanFitMode` | `scaleToFit`, `keepModelDurations`, `trimToRange` |

### 9.9.2 未知の enum 値の扱い

`enumFromString(s, fallback)` の既定フォールバックに任せてよいのは、
**意味を失っても実害がない enum に限る** (例: `CameraMovement` が
`Unspecified` に落ちてもプロンプトの語彙がひとつ減るだけである)。

`TrackType` は例外とする。未知の型を `Video` などにフォールバックさせると、
新しい種類のトラックが**別の意味を持つトラックとして解釈され、合成に混ざる**。

```cpp
// TrackType だけは専用の変換を使う
TrackType parseTrackType(const QString& s)
{
    for (const auto& [e, str] : EnumMap<TrackType>::table())
        if (s == QLatin1String(str)) return e;
    return TrackType::Unknown;      // フォールバックしない
}
```

`TrackType::Unknown` のトラックは、

- 合成にもオーディオグラフにも参加しない
- クリップを一切受け付けない
- UI では読み取り専用として表示する
- **保存時は元の JSON をそのまま書き戻す** (9.11.2 を Track へ拡張する)

## 9.10 パス解決

```cpp
// src/io/PathResolver.h
namespace yave::io {

class PathResolver
{
public:
    explicit PathResolver(const QString& projectFilePath);

    /// 絶対パス -> プロジェクト相対パス。
    /// 相対化できない場合 (別ドライブ等) は絶対パスを返し、warning を出す。
    QString toRelative(const QString& absolutePath, bool* relativizedOut = nullptr) const;

    /// 相対パス -> 絶対パス。
    /// ファイルが存在しない場合は、以下の順で探す:
    ///   1. プロジェクトフォルダからの相対
    ///   2. assets/ 直下 (ファイル名一致)
    ///   3. 最近使ったフォルダ (QSettings に記録)
    ///   4. ハッシュ一致するファイルを検索対象フォルダから探す
    QString toAbsolute(const QString& relativePath, const QString& expectedHash = {}) const;

    /// パス区切りの正規化。JSON には常に '/' で保存する。
    static QString normalizeSeparators(const QString& p);

private:
    QDir projectDir_;
};

} // namespace yave::io
```

### 9.10.1 パス区切りの正規化

**JSON には必ず `/` で保存する。** Windows のバックスラッシュをそのまま書くと、
JSON のエスケープ (`\\`) が必要になり、macOS で開いたときの扱いも面倒になる。

```cpp
QString PathResolver::normalizeSeparators(const QString& p)
{
    return QDir::fromNativeSeparators(p);   // '\' -> '/'
}
```

読み込み時は `QDir::toNativeSeparators()` は使わない。
Qt のファイル API は `/` をどのプラットフォームでも受け付ける。

### 9.10.2 アセット再リンク

素材が見つからない場合、読み込みは**失敗させない**。
`Asset::isMissing = true` としてプロジェクトを開き、UI で再リンクを促す。

```cpp
struct Asset
{
    QUuid   id;
    QString relativePath;
    QString resolvedAbsolutePath;   // 解決できなかったら空
    QString hash;                   // SHA-256 の先頭 16 バイト
    bool    isMissing = false;
    // ...
};
```

「一括再リンク」機能: 1 つの素材を再リンクしたら、同じフォルダにあった他の
missing 素材も自動で探す。

## 9.11 スキーマバージョニング

```cpp
// src/io/SchemaMigration.h
namespace yave::io {

using MigrationFn = void (*)(QJsonObject& root, LoadResult* result);

/// バージョン N から N+1 へ変換する関数のテーブル
struct Migration { int fromVersion; MigrationFn fn; };

inline const std::vector<Migration>& migrations()
{
    static const std::vector<Migration> table = {
        { 1, &migrate_1_to_2 },     // AIトラック (13 章) の追加
        { 2, &migrate_2_to_3 },     // フィルタ / トランジション / タイトル / ライブラリ
    };
    return table;
}

} // namespace yave::io
```

```cpp
void ProjectSerializer::applyMigrations(QJsonObject& root, int fromVersion, LoadResult* r)
{
    int v = fromVersion;
    while (v < kCurrentSchemaVersion) {
        auto it = std::find_if(migrations().begin(), migrations().end(),
                               [v](const Migration& m) { return m.fromVersion == v; });
        if (it == migrations().end()) {
            r->warnings.append(QObject::tr("No migration path from schema version %1.").arg(v));
            break;
        }
        it->fn(root, r);
        ++v;
        r->migrated = true;
    }
}
```

#### migrate_1_to_2 (AIトラックの追加)

追加のみで破壊的変更は無いが、それでもバージョンを上げる。

> **理由**: v1 のライタが v2 のプロジェクトを保存すると、ルートの `storyBible` を
> 丸ごと落とし、`"type":"storyboard"` のトラックを別の型として書き戻してしまう。
> これは 9.11.2 の未知フィールド保持を `Track` とルートへ拡張することで防ぐが、
> それが入っているかどうかをバージョンで判別できる必要がある。

```cpp
void migrate_1_to_2(QJsonObject& root, LoadResult* r)
{
    // (1) 作品設定を空で用意する
    if (!root.contains(keys::kStoryBible))
        root[keys::kStoryBible] = QJsonObject{};

    // (2) 各トラックへ AIトラック関連の既定値を入れる
    QJsonArray tracks = root[keys::kTracks].toArray();
    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject t = tracks[i].toObject();
        if (!t.contains(keys::kAiRole))            t[keys::kAiRole] = QString();
        if (!t.contains(keys::kStoryboardTrackId)) t[keys::kStoryboardTrackId] = QJsonValue::Null;
        if (!t.contains(keys::kRoleDefaults))      t[keys::kRoleDefaults] = QJsonObject{};
        tracks[i] = t;
    }
    root[keys::kTracks] = tracks;

    // v1 のプロジェクトには CutClip も storyboard トラックも存在しないため、
    // クリップ側の変換は不要である。
    Q_UNUSED(r);
}
```

#### migrate_2_to_3 (フィルタ / トランジション / ライブラリの追加)

追加が主だが、クリップの `effectChain` を `filters` へ改称する破壊的変更を含む
([9.4.1](#941-videoclip))。

```cpp
void migrate_2_to_3(QJsonObject& root, LoadResult* r)
{
    // (1) ライブラリのフォルダ構成を空で用意する
    if (!root.contains(keys::kLibrary)) {
        root[keys::kLibrary] = QJsonObject{
            { keys::kFolders,     QJsonArray{} },
            { keys::kAssignments, QJsonObject{} } };
    }

    QJsonArray tracks = root[keys::kTracks].toArray();
    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject t = tracks[i].toObject();

        // (2) トランジション配列を空で用意する
        if (!t.contains(keys::kTransitions))
            t[keys::kTransitions] = QJsonArray{};

        // (3) クリップの effectChain -> filters へ移す。
        //     トラックの effectChain (オーディオ) はそのまま残す。
        QJsonArray clips = t[keys::kClips].toArray();
        for (int j = 0; j < clips.size(); ++j) {
            QJsonObject c = clips[j].toObject();
            if (c.contains(keys::kEffectChain) && !c.contains(keys::kFilters)) {
                c[keys::kFilters] = c.take(keys::kEffectChain);
            }
            if (!c.contains(keys::kFilters))
                c[keys::kFilters] = QJsonArray{};
            clips[j] = c;
        }
        t[keys::kClips] = clips;
        tracks[i] = t;
    }
    root[keys::kTracks] = tracks;
    Q_UNUSED(r);
}
```

> v2 には `title` 型のクリップが存在しないため、クリップ型の変換は不要である。

### 9.11.1 未来のバージョンを開いた場合

```cpp
if (loadedVersion > kCurrentSchemaVersion) {
    // 開くこと自体は許可するが、明確に警告する
    r->warnings.append(QObject::tr(
        "This project was saved with a newer version of YAVE (schema %1). "
        "Some data may be lost if you save it with this version.")
        .arg(loadedVersion));
    // 未知のフィールドは保持して書き戻す (9.11.2)
}
```

### 9.11.2 未知フィールドの保持

新しいバージョンで追加されたフィールドを、古いバージョンで開いて保存すると
消えてしまう。これを防ぐため、**認識できなかったキーを保持して書き戻す**。

```cpp
class Clip
{
    // ...
protected:
    /// デシリアライズ時に認識できなかった JSON キーを保持する。
    /// シリアライズ時にそのまま書き戻す。
    QJsonObject unknownFields_;
};
```

**この仕組みは `Clip` だけでなく `Track` とプロジェクトルートにも持たせる。**

```cpp
class Track { /* ... */ QJsonObject unknownFields_; };   // 9.3.1 / 11.2

class Project { /* ... */ QJsonObject unknownRootFields_; };
```

> schemaVersion 2 で `storyBible` をルート直下に、`aiRole` / `storyboardTrackId` /
> `roleDefaults` を各トラックに追加した。`Clip` にしか未知フィールド保持が無いままだと、
> **前方互換の約束が新しいデータをまったくカバーしない**。
> `TrackType::Unknown` のトラック (9.9.2) は、この仕組みで丸ごと原文保存される。

```cpp
QJsonObject ProjectSerializer::serializeClip(const Clip& c, const SaveOptions& o)
{
    QJsonObject obj = c.unknownFields();     // 未知フィールドを先に入れる
    obj[keys::kClipId] = c.id().toString(QUuid::WithoutBraces);
    obj[keys::kClipType] = enumToString(c.type());
    // ... 既知フィールドで上書き
    return obj;
}
```

## 9.12 保存の原子性

保存中にクラッシュや電源断が起きても、既存のプロジェクトファイルを壊さない。

```cpp
bool ProjectSerializer::save(const Project& project, const QString& path,
                             const SaveOptions& opts, QString* errorOut)
{
    const QJsonObject root = serializeProject(project, opts);
    const QByteArray data  = QJsonDocument(root).toJson(
        opts.indented ? QJsonDocument::Indented : QJsonDocument::Compact);

    // (1) テンポラリファイルへ書く
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        if (errorOut) *errorOut = file.errorString();
        file.cancelWriting();
        return false;
    }

    // (2) commit() が fsync + rename をアトミックに行う
    if (!file.commit()) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }

    // (3) 世代バックアップ (直前の 5 世代を残す)
    rotateBackups(path, 5);
    return true;
}
```

`QSaveFile` は内部で「一時ファイルへ書く → `fsync` → `rename`」を行う。
`rename` は同一ボリューム内では原子的操作なので、途中で落ちても
既存ファイルは無傷のまま残る。

## 9.13 自動保存とクラッシュリカバリ

```
<project_dir>/
├── MyProject.yave
├── MyProject.yave.bak1        直前の保存
├── MyProject.yave.bak2
├── ...
└── MyProject.yave.autosave    30 秒ごと (CBOR)
```

- 自動保存はバックグラウンドスレッドで行う。UI をブロックしない。
  そのため、保存対象のスナップショットを UI スレッドで作ってからワーカへ渡す。
- 正常終了時に `.autosave` を削除する。
- 起動時に `.autosave` が残っていたら「前回異常終了しました。復元しますか?」と尋ねる。
- 未保存の新規プロジェクトの自動保存先は
  `%APPDATA%/YAVE/autosave/<session-uuid>.autosave`。

## 9.14 パフォーマンス目標

| 項目 | 目標 |
|---|---|
| 1000 クリップのプロジェクトのシリアライズ | 200ms 以内 |
| 同・デシリアライズ (アセット解決を除く) | 300ms 以内 |
| ファイルサイズ (1000 クリップ、Indented) | 3MB 以内 |
| 自動保存 (CBOR、1000 クリップ) | 50ms 以内 |

> `QJsonObject` はコピーオンライトの暗黙共有コンテナだが、
> キー挿入のたびに内部でソートが走る。ホットパス (クリップのシリアライズ) では
> 挿入回数を減らすため、必要なフィールドのみを書く
> (既定値と同じフィールドは省略する) 最適化を入れる。
> 省略されたフィールドはデシリアライズ時に既定値で埋める。
