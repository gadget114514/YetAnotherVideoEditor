# 13. AIトラック (演出指示 / 絵コンテトラック)

[← 目次に戻る](../design.md)

---

## 13.1 このトラックは何か

### 13.1.1 位置づけ

**AIトラック** (`TrackType: 'storyboard'`) は、タイムライン上に「**これから何を作るか**」を並べるトラックである。映像や音声そのものは直接載らない。載るのは**カット単位の演出指示** (`CutClip`) であり、これは絵コンテ (storyboard) の 1 コマに相当する。

生成を実行すると、各カットは自身が必要とする**出力トラック** (映像 / ナレーション / BGM / SE / 字幕) を解決または新規作成し、そこへ実クリップを配置する。AIトラックは生成後も残り続け、**再生成の source of truth** になる。

```
  +- Storyboard トラック (AIトラック) ------------------------------+
  | [カット1 屋上・朝] [カット2 教室] [カット3 廊下] [カット4 …]      |  <- 仕様。合成に参加しない
  +--------+---------------+--------------+------------------------+
           | 生成           |              |
           v               v              v
  +- Video (aiRole='mainVideo') -----------------------------------+
  | [   生成映像1   ] [   生成映像2   ] [   生成映像3   ]              |
  +----------------------------------------------------------------+
  +- Subtitle (aiRole='subtitle') ----------------------------------+
  | [ セリフ1 ]        [ セリフ2 ]       [ セリフ3 ]                   |
  +----------------------------------------------------------------+
  +- Audio (aiRole='narration') ------------------------------------+
  | [ TTS1     ]       [ TTS2     ]      [ TTS3     ]                |
  +----------------------------------------------------------------+
```

### 13.1.2 L1 と L2

AI へのリクエストは 2 階層ある。

| 階層 | 入力 | 出力 | 実行主体 |
|---|---|---|---|
| **L1 プランニング** | 自然言語の要求 (「3 分の商品紹介動画を作って」) | カット列 + 必要トラック構成 + Story Bible 草案 | LLM (`GenerationKind: 'storyboard'`) |
| **L2 マテリアライズ** | 1 カットの演出指示 | 映像 / 音声 / 字幕 の実メディア | [7 章](07-ai-orchestrator.md) の既存パイプライン |

L1 はタイムラインへ直接メディアを置かず、カット (`CutClip`) を配置するのみである。L2 はカットの演出指示からプロンプトを合成し、依存 DAG 順にメディア生成タスクを発行する。

### 13.1.3 なぜ `aiGenerated` を再利用しないのか

`aiGenerated` トラックは「生成物であることを UI 上で区別する」ためのマーカーであり、振る舞いは中身の実クリップ型 (`VideoClip` 等) で決まる。一方、`storyboard` トラックは演出仕様 (`CutClip`) のみを載せ、合成には一切参加しない。同一の enum 値に 2 つの意味を持たせるとマーカー規則が破綻するため、独立した `TrackType: 'storyboard'` を設ける。

---

## 13.2 用語

| 用語 | 意味 |
|---|---|
| **カット** | AIトラック上の 1 区間 (`CutClip`)。絵コンテの 1 コマにあたる演出指示の単位 |
| **絵コンテトラック / AIトラック** | 本書では同義。`TrackType: 'storyboard'` のトラック |
| **出力バインディング** | 1 カットが生む 1 成果物の仕様 (`OutputBinding`)。役割・配置先・パラメータ・生成状態を持つ |
| **役割 (role)** | 出力の種別 (`OutputRole`)。`mainVideo`, `narration`, `bgm`, `se`, `subtitle`, `mask` など |
| **Story Bible** | プロジェクト単位の設定集。キャラクター・ロケーション・画風・ネガティブプロンプト。全カットのプロンプトへカスケードする |
| **アニマティック** | 未生成カットをボード画像 + TTS プレビューで仮再生すること。絵コンテ動画 |
| **承認状態** | カットの進行状態 (`CutStatus`)。未着手 / ラフ / 確認中 / 承認済み |
| **仕様ハッシュ** | 出力バインディング 1 件の生成仕様を要約した SHA-256。再生成要否の判定に使う |
| **プラン** | L1 が生成する JSON。カット列とトラック構成の設計案 |

---

## 13.3 データモデル

### 13.3.1 CutClip と関連型

```ts
// packages/core/src/model/CutClip.ts
import { Clip } from './Clip';
import type { Uuid } from '../id/Uuid';

export type CutStatus = 'notStarted' | 'rough' | 'inReview' | 'approved';
export type ContinuityMode = 'none' | 'fromBoardImage' | 'fromPreviousEnd' | 'fromCutId';
export type OutputRole = 'mainVideo' | 'mainVideoB' | 'overlay' | 'narration' | 'bgm' | 'se' | 'subtitle' | 'mask';
export type OutputState = 'notGenerated' | 'queued' | 'running' | 'cached' | 'committed' | 'failed' | 'stale' | 'blocked';
export type TrackResolveMode = 'auto' | 'existing' | 'alwaysNew';

export interface CameraWork {
  size: 'unspecified' | 'extremeWide' | 'wide' | 'full' | 'medium' | 'closeUp' | 'extremeCloseUp';
  angle: 'unspecified' | 'eyeLevel' | 'high' | 'low' | 'birdsEye' | 'wormsEye' | 'dutch';
  movement: 'unspecified' | 'fixed' | 'panLeft' | 'panRight' | 'tiltUp' | 'tiltDown' | 'dolly' | 'slowPushIn' | 'pullOut' | 'handheld' | 'crane' | 'follow';
  note: string;
}

export interface BoardImage {
  origin: 'none' | 'userFile' | 'generated' | 'timelineFrame';
  assetId?: Uuid;
  sourceTrackId?: Uuid;
  sourceFrame?: number;
  generatedByTaskId?: Uuid;
}

export interface Continuity {
  mode: ContinuityMode;
  fromCutId?: Uuid;
  strength: number; // 0..1
  sceneBreak: boolean; // true なら前カットからの依存を切断
}

export interface OutputBinding {
  id: string; // "ob-1", "ob-2"
  role: OutputRole;
  roleTag: string;
  enabled: boolean;
  resolveMode: TrackResolveMode;
  resolvedTrackId: Uuid | null;
  trackNameHint: string;
  derivedFromBindingId: string | null;
  leadInFrames: number;
  leadOutFrames: number;
  paramPatch: Record<string, any>;
  promptLock: {
    locked: boolean;
    prompt: string;
    negativePrompt: string;
    lockedAgainstHash: string;
  };
  lastTaskId: Uuid | null;
  committedClipIds: Uuid[];
  committedSpecHash: string;
  committedUpstreamHash: string;
  state: OutputState;
}

export class CutClip extends Clip {
  readonly type = 'cut' as const;
  label: string = ''; // 手動採番 ("12b" 等)。空なら自動採番
  slug: string = '';  // 見出し (「屋上・朝」)
  description: string = ''; // ト書き
  dialogue: string = '';    // セリフ / ナレーション原稿
  mood: string = '';        // 雰囲気
  camera: CameraWork = { size: 'unspecified', angle: 'eyeLevel', movement: 'unspecified', note: '' };
  characterIds: Uuid[] = [];
  locationId?: Uuid;
  transitionIn: 'cut' | 'dissolve' | 'fadeToBlack' | 'fadeFromBlack' | 'wipe' | 'matchCut' = 'cut';
  transitionOut: 'cut' | 'dissolve' | 'fadeToBlack' | 'fadeFromBlack' | 'wipe' | 'matchCut' = 'cut';
  board: BoardImage = { origin: 'none' };
  continuity: Continuity = { mode: 'fromBoardImage', strength: 0.9, sceneBreak: false };
  status: CutStatus = 'notStarted';
  reviewNote: string = '';
  paramPatch: Record<string, any> = {};
  biblePatch: Record<string, any> = {};
  outputs: OutputBinding[] = [];

  clone(): CutClip { /* ... */ return new CutClip(); }
  makeLayerItem(): null { return null; } // 合成には参加しない
  specHash(role: OutputRole, tag?: string): string { /* SHA-256 算出 */ return ''; }
}
```

> **カット番号を保持しない設計 (継承)**:
> カット番号はトラック内の配列順序から導出する。`zOrder` を持たないのと同じ理由であり、並び替えによる番号の食い違いを防止する。

---

## 13.4 パラメータカスケード (4 段マージ)

各カットの出力バインディングに適用される生成パラメータは、以下の 4 階層で上流から下流へ順次マージされる。

```
[1] Story Bible (プロジェクト全体)
      ↓ override
[2] Track.roleDefaults (トラック単位の既定値)
      ↓ override
[3] CutClip.paramPatch (カット単位の上書き)
      ↓ override
[4] OutputBinding.paramPatch (成果物固有の上書き)
```

- パッチはすべて**疎なオブジェクト**として保持される。
- キーが存在することが「上書き」を意味し、キーを削除することで親レベルの値の継承に復帰する。

---

## 13.5 プロンプト合成 (`CutPromptComposer`)

構造化フィールドから自然言語プロンプト文字列を組み立てる。

```ts
export function composePrompt(
  cut: CutClip,
  bible: StoryBible,
  role: OutputRole
): { prompt: string; negativePrompt: string } {
  // 1. promptTemplates からテンプレートを取得 (例: "{{artStyle}}. {{location}}. {{characters}}. {{description}} {{camera}} {{mood}}")
  // 2. プレースホルダを置換:
  //    - {{artStyle}}: bible.artStyle (または cut.biblePatch.artStyle)
  //    - {{location}}: 該当 location の promptFragment
  //    - {{characters}}: 該当キャラの promptFragment をカンマ結合
  //    - {{description}}: cut.description
  //    - {{camera}}: camera_phrases.json から英語句を解決
  //    - {{mood}}: cut.mood
  // 3. bible.promptPrefix / promptSuffix を前後に結合
  return { prompt: '...', negativePrompt: '...' };
}
```

> **カメラワーク辞書**: `resources/ai/camera_phrases.json` は英語固定であり、UI の国際化翻訳経路には載せない。

---

## 13.6 依存バッチ DAG (`StoryboardBatchJob`)

複数カットの一括生成を行う際、各出力バインディング間の依存関係を有向非巡回グラフ (DAG) として解決し、トポロジカルソート順に実行する。

```
  [カット1: mainVideo] (I2V / T2V)
        │
        ├────────────────────────────┐
        ▼ 終了フレーム参照            ▼ 音声タイミング参照
  [カット2: mainVideo] (I2V)    [カット1: narration] (TTS)
                                     │ タイムスタンプ参照
                                     ▼
                                [カット1: subtitle] (字幕)
```

1. **トポロジカルソート**: 依存先が未完了のタスクは `blocked` 状態として待機。
2. **`sceneBreak` フラグ**: `continuity.sceneBreak === true` のカットでは前カットへの依存を切り離し、並列生成を可能にする。
3. **循環参照の検出**: DAG 構築時にサイクルを検知した場合は直ちにエラーとして報告。

---

## 13.7 承認ゲートと再生成検知 (Dirtiness)

### 13.7.1 承認ゲート
- カットの状態が `approved` (承認済み) になるまで、本番メディアの L2 生成には進まない。
- `notStarted`, `rough`, `inReview` の状態では、ボード画像と軽量 TTS による**アニマティック仮再生**のみが許可される。

### 13.7.2 再生成要否の判定 (仕様ハッシュ)
各 `OutputBinding` は前回の生成完了時の仕様ハッシュ (`committedSpecHash`) を保持する。
- 演出指示 (ト書き、カメラ、Bible 画風等) が編集されると、新しい `specHash` が計算される。
- `committedSpecHash !== newSpecHash` となった場合、バインディングの状態は自動的に `stale` (陳腐化) に遷移し、UI に再生成が必要である旨のバッジが表示される。

---

## 13.8 L1 プランニング契約 (`storyboard_plan.schema.json`)

LLM が出力するプラン JSON のスキーマ定義。LLM からの出力は**信頼できない入力**として扱い、必ずバリデータを通す。

- **最大カット数**: 1 回のプランニングで最大 30 カットまでにクランプ。
- **尺の配分**: 各カットの秒数合計がユーザー指定の総尺と合致するよう、最大剰余法を用いてプロジェクトタイムベースのフレーム数に正確に配分する。

---

## 13.9 アニマティック (絵コンテ動画プレビュー)

未生成のカットであっても、以下の手法でタイムラインを仮再生できる。

- **映像**: `CutClip.board` (絵コンテ画像または参照画像) を静止画としてキャンバスに描画。
- **音声**: ローカルの軽量 TTS (`OnnxWebProvider`) を用いてセリフを即座に合成し、一時キャッシュから再生。
- **キャプション**: ト書きやセリフを画面中央下部に一時テロップとしてオーバーレイ表示。

これにより、高コストな動画生成を実行する前に、全体のテンポや演出の整合性をブラウザ上で確認できる。
