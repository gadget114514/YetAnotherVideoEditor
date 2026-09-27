# 3. タイムライン & レンダリングエンジン

[← 目次に戻る](../design.md)

---

## 3.1 時間表現

### 3.1.1 Rational

すべてのタイムライン時刻は有理数タイムベース上のフレーム番号で表す。`double` 秒での保持は禁止する。

```ts
// packages/core/src/time/Rational.ts
export interface Rational {
  readonly num: number;
  readonly den: number;
}

export namespace RationalUtil {
  export function create(num: number, den: number): Rational {
    if (den === 0) {
      return { num, den: 1 }; // ゼロ除算保護
    }
    return reduce(num, den);
  }

  export function reduce(num: number, den: number): Rational {
    if (den < 0) {
      num = -num;
      den = -den;
    }
    const g = gcd(Math.abs(num), Math.abs(den));
    return { num: Math.trunc(num / g), den: Math.trunc(den / g) };
  }

  export function toDouble(r: Rational): number {
    return r.den === 0 ? 0 : r.num / r.den;
  }

  export function invert(r: Rational): Rational {
    return create(r.den, r.num);
  }

  export function multiply(a: Rational, b: Rational): Rational {
    return create(a.num * b.num, a.den * b.den);
  }

  export function add(a: Rational, b: Rational): Rational {
    return create(a.num * b.den + b.num * a.den, a.den * b.den);
  }

  export function equals(a: Rational, b: Rational): boolean {
    const ra = reduce(a.num, a.den);
    const rb = reduce(b.num, b.den);
    return ra.num === rb.num && ra.den === rb.den;
  }

  export function compare(a: Rational, b: Rational): number {
    // 交差乗算: a.num * b.den - b.num * a.den
    const diff = a.num * b.den - b.num * a.den;
    if (Math.abs(diff) < Number.MAX_SAFE_INTEGER) {
      return diff < 0 ? -1 : diff > 0 ? 1 : 0;
    }
    // Number.MAX_SAFE_INTEGER を超える場合は BigInt 経路へフォールバック
    const biDiff = BigInt(a.num) * BigInt(b.den) - BigInt(b.num) * BigInt(a.den);
    return biDiff < 0n ? -1 : biDiff > 0n ? 1 : 0;
  }

  function gcd(a: number, b: number): number {
    let x = Math.trunc(a);
    let y = Math.trunc(b);
    while (y !== 0) {
      const t = y;
      y = x % y;
      x = t;
    }
    return x;
  }
}

// 代表的なタイムベース
export const Timebase = {
  Fps23_976: { num: 1001, den: 24000 } as Rational,
  Fps24:     { num: 1,    den: 24 }    as Rational,
  Fps25:     { num: 1,    den: 25 }    as Rational,
  Fps29_97:  { num: 1001, den: 30000 } as Rational,
  Fps30:     { num: 1,    den: 30 }    as Rational,
  Fps59_94:  { num: 1001, den: 60000 } as Rational, // 既定
  Fps60:     { num: 1,    den: 60 }    as Rational,
} as const;

export type RoundMode = 'floor' | 'nearest' | 'ceil';

export function secondsToFrames(seconds: number, tb: Rational, mode: RoundMode = 'nearest'): number {
  // frames = seconds / (num / den) = seconds * den / num
  const val = (seconds * tb.den) / tb.num;
  switch (mode) {
    case 'floor':   return Math.floor(val);
    case 'ceil':    return Math.ceil(val);
    case 'nearest': return Math.round(val);
  }
}

export function framesToSeconds(frames: number, tb: Rational): number {
  return (frames * tb.num) / tb.den;
}

export function rescaleFrames(frames: number, from: Rational, to: Rational, mode: RoundMode = 'nearest'): number {
  // frames * (from.num / from.den) / (to.num / to.den) = frames * from.num * to.den / (from.den * to.num)
  const num = frames * from.num * to.den;
  const den = from.den * to.num;
  const val = num / den;
  switch (mode) {
    case 'floor':   return Math.floor(val);
    case 'ceil':    return Math.ceil(val);
    case 'nearest': return Math.round(val);
  }
}
```

> **オーバーフロー対策**:
> `num * den` の積が `Number.MAX_SAFE_INTEGER` (約 `9.007e15`) を超える長大フレーム数または高精度タイムベースの交差比較では、自動的に `BigInt` に変換して正確に比較する。

### 3.1.2 TimeRange

```ts
// packages/core/src/time/TimeRange.ts
export interface TimeRange {
  readonly start: number;     // フレーム番号 (整数)
  readonly duration: number;  // フレーム数 (整数, > 0)
}

export namespace TimeRangeUtil {
  export function end(r: TimeRange): number {
    return r.start + r.duration;
  }

  export function isEmpty(r: TimeRange): boolean {
    return r.duration <= 0;
  }

  export function contains(r: TimeRange, frame: number): boolean {
    return frame >= r.start && frame < r.start + r.duration;
  }

  export function intersects(a: TimeRange, b: TimeRange): boolean {
    return a.start < b.start + b.duration && b.start < a.start + a.duration;
  }

  export function intersect(a: TimeRange, b: TimeRange): TimeRange | null {
    const s = Math.max(a.start, b.start);
    const e = Math.min(a.start + a.duration, b.start + b.duration);
    return e > s ? { start: s, duration: e - s } : null;
  }

  export function translate(r: TimeRange, deltaFrames: number): TimeRange {
    return { start: r.start + deltaFrames, duration: r.duration };
  }
}
```

> **半開区間で統一する理由**:
> 閉区間だと隣接クリップの境界が 1 フレーム重なり、「連結したはずのクリップが 1 フレーム被って点滅する」という不具合を生む。UI 表示上の Out 点は `end() - 1` として表示する (ユーザーは最終フレーム番号を見たがるため)。

---

## 3.2 無限レイヤー構造

### 3.2.1 データ構造の選択

```ts
export class Timeline {
  private tracks_: Track[] = []; // 無限レイヤー
}

export class Track {
  private clips_: Clip[] = []; // start 昇順にソート済み (不変条件)
  private transitions_: Transition[] = [];
}
```

| 選択 | 理由 |
|---|---|
| `Track[]` 配列 | トラック数に上限を設けない。配列の要素移動による並び替えが容易 |
| Z オーダー = 配列のインデックス | 別途 `zOrder` メンバを持つとソートと同期が必要になり、並び替えのバグ源になる。index 0 が最背面、末尾が最前面 |
| `clips_` は常に start 昇順 | 二分探索を可能にする。挿入時に順序を保つ責務は `Track.insertClip()` が持つ |

### 3.2.2 トラックの不変条件

`Track` は以下を常に満たす。開発環境 (`process.env.NODE_ENV !== 'production'`) では `assertInvariants()` を各操作後に呼ぶ。

1. `clips_` は `start` の昇順にソートされている。
2. 同一トラック内のクリップの `TimeRange` は互いに重ならない。
   - 重ねたい場合はトラックを分ける。これが「無限レイヤー」を前提にできる設計上の強み。
3. すべてのクリップの `duration > 0`。
4. `transitions_` の各要素は、**隣接する 2 クリップの境界にのみ** 置かれる (`from` の終端 == `to` の始端)。1 つの境界に 2 つ以上は置けない。
   - トランジションはクリップを重ねずに実現する ([3.10](#310-トランジション))。2 と両立させるための設計であり、[13.14.7](13-ai-track.md) の A/B ロール規定とも矛盾しない。
5. トランジションの長さは、**両側のクリップが持つハンドル** (`maxDuration()` に対するソースの残り尺) の 2 倍を超えない。

```ts
export function assertTrackInvariants(track: Track): void {
  const clips = track.clips;
  for (let i = 0; i < clips.length; ++i) {
    const c = clips[i]!;
    if (c.range.duration <= 0) {
      throw new Error(`Invariant failed: clip duration <= 0 (id=${c.id})`);
    }
    if (i > 0) {
      const prev = clips[i - 1]!;
      if (prev.range.start > c.range.start) {
        throw new Error(`Invariant failed: clips not sorted by start`);
      }
      if (prev.range.start + prev.range.duration > c.range.start) {
        throw new Error(`Invariant failed: clips overlap in same track (${prev.id} & ${c.id})`);
      }
    }
  }
}
```

### 3.2.3 クリップ検索 (二分探索)

```ts
export function findClipAt(clips: readonly Clip[], frame: number): Clip | null {
  let low = 0;
  let high = clips.length - 1;
  let candidateIndex = -1;

  while (low <= high) {
    const mid = (low + high) >> 1;
    const c = clips[mid]!;
    if (c.range.start <= frame) {
      candidateIndex = mid;
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }

  if (candidateIndex !== -1) {
    const cand = clips[candidateIndex]!;
    if (frame < cand.range.start + cand.range.duration) {
      return cand;
    }
  }
  return null;
}
```

計算量: `clipAt` は O(log n)。

### 3.2.4 トラックの種別と互換性

```ts
export type TrackType = 'video' | 'audio' | 'subtitle' | 'aiGenerated' | 'storyboard' | 'unknown';
```

| TrackType | 受け入れる Clip | 合成への参加 |
|---|---|---|
| `video` | `VideoClip`, `ImageClip`, `ColorClip`, `TitleClip`, `AiPlaceholderClip` (video) | 映像レイヤーとして合成 |
| `audio` | `AudioClip`, `AiPlaceholderClip` (audio) | オーディオグラフへ |
| `subtitle` | `SubtitleClip` | 映像レイヤーとして合成 (最前面寄りに置くのが慣例だが強制しない) |
| `aiGenerated` | 上記すべて | 生成結果の種別に応じて映像または音声 |
| `storyboard` | `CutClip` **のみ** | **参加しない** (演出指示。[13章](13-ai-track.md)) |
| `unknown` | 何も受け入れない | 参加しない (未知スキーマの保全用。[9.9](09-project-io.md)) |

`Track.acceptsClip(clip: Clip): boolean` で受け入れ可否を判定し、UI のドラッグ & ドロップはこれを見て可否フィードバックを出す。`CutClip` は `storyboard` トラック**だけ**が受け入れ、`storyboard` トラックは `CutClip` **だけ**を受け入れる (排他的受け入れ規則)。

---

## 3.3 RenderSnapshot

メインスレッドの WebGL2 レンダラーは `Timeline` を直接参照せず、1 フレームごとに不変のスナップショットを作成して描画する。

```ts
// packages/core/src/snapshot/RenderSnapshot.ts
export interface ResolvedFilter {
  readonly filterId: string;
  readonly params: readonly number[]; // 固定長 float 配列
}

export interface TransitionRef {
  readonly transitionId: string;
  readonly progress: number;          // 0.0 (from 100%) .. 1.0 (to 100%)
  readonly isIncoming: boolean;       // true なら to 側
}

export interface LayerItem {
  readonly clipId: string;
  readonly trackType: TrackType;
  readonly zIndex: number;            // 小さいほど背面
  readonly blendMode: string;
  readonly opacity: number;
  readonly transform: readonly number[]; // 4x4 matrix (16要素配列)
  readonly cropRect: readonly [number, number, number, number]; // x, y, w, h (0..1)
  readonly filters: readonly ResolvedFilter[];
  readonly transition?: TransitionRef;
  readonly source:
    | { kind: 'video'; assetId: string; sourceFrameIndex: number }
    | { kind: 'subtitle'; clipId: string; text: string; style: any }
    | { kind: 'image'; assetId: string }
    | { kind: 'color'; color: string }
    | { kind: 'placeholder'; title: string; progress: number };
}

export interface RenderSnapshot {
  readonly frameIndex: number;
  readonly timebase: Rational;
  readonly canvasSize: { readonly width: number; readonly height: number };
  readonly timelineRevision: number;
  readonly layers: readonly LayerItem[]; // 背面 -> 前面の順
}
```

構築は `Timeline.buildSnapshot(frame: number): RenderSnapshot` が行い、`Object.freeze` で凍結して渡す。

---

## 3.4 WebGL2 合成パイプライン

### 3.4.1 1 フレームの流れ

```
 [Main Thread - rAF ループ]
   PlaybackController が AudioClock から現在の再生フレーム N を取得
        |
   Timeline.buildSnapshot(N)
        |
        v
   WebGlCompositor.renderFrame(snapshot)
        |
   (1) PREPARE
        各 LayerItem のテクスチャを調達:
        - video: FrameCache から VideoFrame (WebGLTexture に texImage2D または外部テクスチャバインド)
        - subtitle: GlyphAtlas (OffscreenCanvas 2D から生成した WebGLTexture)
        - placeholder: プレースホルダテクスチャ
        |
   (2) CLEAR
        中間合成用 FBO (compositeFboA) を canvasSize でクリア (透明黒: 0, 0, 0, 0)
        |
   (3) COMPOSITE (ping-pong FBO)
        for layer in snapshot.layers:
            if layer.filters.length > 0:
                FilterPass.apply(layer.sourceTex, layer.filters) -> filterResultTex
            if layer.transition:
                TransitionPass.blend(fromTex, toTex, layer.transition.progress) -> transResultTex
            LayerPass.draw(layerTex, compositeFboB, blendMode, transform, opacity, crop)
            swap(compositeFboA, compositeFboB)
        |
   (4) PRESENT
        最終 FBO のテクスチャを HTML <canvas> のデフォルトフレームバッファへ描画
        (canvas の CSS アスペクト比を維持してレターボックス配置)
```

### 3.4.2 ブレンドモードの GLSL ES 3.0 実装

```glsl
#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uSrcTex; // 新しいレイヤー
uniform sampler2D uDstTex; // ここまでの合成結果 (背景)
uniform float uOpacity;
uniform int uBlendMode;
uniform vec4 uCropRect; // x, y, w, h (0..1)

vec3 blend(int mode, vec3 base, vec3 src) {
    if (mode == 0) return src;                                     // Normal
    if (mode == 1) return base + src;                              // Add
    if (mode == 2) return base * src;                              // Multiply
    if (mode == 3) return 1.0 - (1.0 - base) * (1.0 - src);        // Screen
    if (mode == 4) return mix(2.0 * base * src,                    // Overlay
                              1.0 - 2.0 * (1.0 - base) * (1.0 - src),
                              step(0.5, base));
    if (mode == 5) return min(base, src);                          // Darken
    if (mode == 6) return max(base, src);                          // Lighten
    if (mode == 7) return abs(base - src);                         // Difference
    if (mode == 8) return base + src - 2.0 * base * src;           // Exclusion
    return src;
}

void main() {
    vec2 srcUv = uCropRect.xy + vUv * uCropRect.zw;
    vec4 s = texture(uSrcTex, srcUv);
    vec4 d = texture(uDstTex, vUv);

    s.a *= uOpacity;

    if (uBlendMode == 9) { // AlphaMask: src の輝度を dst のαに乗算
        float lum = dot(s.rgb, vec3(0.2126, 0.7152, 0.0722));
        fragColor = vec4(d.rgb, d.a * lum);
        return;
    }

    vec3 blended = blend(uBlendMode, d.rgb, s.rgb);
    fragColor = vec4(mix(d.rgb, blended, s.a), max(d.a, s.a));
}
```

---

## 3.5 テクスチャ / FBO プール

4K (3840x2160) RGBA8 は 1 枚あたり約 32MB、1080p (1920x1080) で約 8MB を消費する。フレームごとの新規生成と破棄を排し、`TexturePool` と `FboPool` で再利用する。

- **最大プール予算**: 512MB (1080p 60 枚相当)
- **解放戦略**: LRU。直近 120 フレーム未使用のテクスチャを定期的に `gl.deleteTexture`

---

## 3.6 フレーム予算と Adaptive Quality

1080p / 60fps における 1 フレームの描画予算は **16.67ms**。

| 工程 | 目標予算 | 備考 |
|---|---|---|
| `buildSnapshot` (メイン) | 0.1ms | 配列走査と二分探索 |
| デコード待機 | 0ms | **Worker の先行デコードでキャッシュ済み** |
| テクスチャ転送 (`texImage2D`) | 2.5ms | VideoFrame の GPU アップロード |
| 字幕ラスタライズ / アトラス更新 | 1.0ms | 変更時のみ Canvas2D 描画 |
| WebGL2 合成 (4 レイヤー + フィルタ) | 5.0ms | FBO ping-pong 描画 |
| Canvas への最終描画 | 1.0ms | ビューポート拡縮 |
| 余裕 / JS ガベージコレクション吸収 | 7.0ms | |

### 3.6.1 負荷超過時の品質調整 (Adaptive Quality)

直近 30 フレームの描画時間中央値が 15ms を超えた場合、自動的に劣化戦略を適用する。

```
Level 0: 通常 (キャンバス解像度 1080p)
Level 1: プレビュー解像度 1/2 (540p) で合成、最後にバイリニア拡大
Level 2: プレビュー解像度 1/4 (270p) + ブラー等のフィルタ段数を半減
Level 3: プロキシメディア強制切替 (540p H.264)
Level 4: フレームスキップ (30fps 表示にドロップ、音声は 48kHz 維持)
```

---

## 3.7 プレビュー表示

プレビューは React の `<canvas>` 要素に WebGL2 コンテキストを割り当てて表示する。

- 駆動方式: `requestVideoFrameCallback` ではなく、**`AudioClock` と同期した `requestAnimationFrame` ループ**
- 音声再生中は、AudioWorklet が進めたサンプル位置に最も近いフレーム番号を計算して描画
- 一時停止中・シーク中は、ユーザーのシーク操作イベントによって単発描画

---

## 3.8 編集操作の実装 (Command パターン)

### 3.8.1 クリップ分割 (Split)

```ts
export class SplitClipCommand implements Command {
  readonly name = 'Split Clip';
  private leftClip: Clip | null = null;
  private rightClip: Clip | null = null;
  private originalClip: Clip | null = null;

  constructor(
    private readonly timeline: Timeline,
    private readonly trackId: string,
    private readonly clipId: string,
    private readonly splitFrame: number
  ) {}

  redo(): void {
    const track = this.timeline.getTrack(this.trackId);
    if (!track) return;

    this.originalClip = track.takeClip(this.clipId);
    if (!this.originalClip) return;

    const r = this.originalClip.range;
    const offset = this.splitFrame - r.start;

    this.leftClip = this.originalClip.clone();
    this.leftClip.setRange({ start: r.start, duration: offset });

    this.rightClip = this.originalClip.clone();
    this.rightClip.setRange({ start: this.splitFrame, duration: r.duration - offset });
    this.rightClip.shiftSourceOffset(offset);

    track.insertClip(this.leftClip);
    track.insertClip(this.rightClip);
  }

  undo(): void {
    const track = this.timeline.getTrack(this.trackId);
    if (!track || !this.originalClip) return;

    if (this.leftClip) track.removeClip(this.leftClip.id);
    if (this.rightClip) track.removeClip(this.rightClip.id);
    track.insertClip(this.originalClip);
  }
}
```

### 3.8.2 リップル削除 (Ripple Delete)

削除した区間より後ろにあるクリップを、そのトラック (または全非ロックトラック) 上で前方へ詰める。

```ts
export class RippleDeleteCommand implements Command {
  readonly name = 'Ripple Delete';
  // ... 対象クリップの退避と後ろのクリップの shiftClipsAfter(start, -duration)
}
```

### 3.8.3 スナップ機能

ドラッグ移動中の吸着対象:
1. 再生ヘッド (Playhead)
2. 同一トラック内の隣接クリップの境界 (In点 / Out点)
3. 画面内に見えている他トラックのクリップ境界
4. タイムラインマーカー

吸着判定は画面ピクセル距離 (既定 8px) を現在のズーム率に基づいてフレーム数に換算して判定する。

---

## 3.9 ビデオフィルタースタック

各クリップは自身の属性として `filters: VideoFilterInstance[]` を持つ。

```ts
export interface VideoFilterInstance {
  readonly filterId: string;
  params: Record<string, number | string | boolean>;
  enabled: boolean;
}
```

### 組み込みフィルター一覧

| フィルタ ID | パラメータ | シェーダ |
|---|---|---|
| `yave.filter.colorAdjust` | `brightness` (-1..1), `contrast` (0..2), `saturation` (0..2), `gamma` (0.1..4) | `filter_color.frag.glsl` |
| `yave.filter.blur` | `radius` (0..64 px), `direction` (0=両方向, 1=水平, 2=垂直) | `filter_blur.frag.glsl` (分離ガウシアン 2 パス) |
| `yave.filter.mono` | `intensity` (0..1) | `filter_color.frag.glsl` |
| `yave.filter.sepia` | `intensity` (0..1) | `filter_color.frag.glsl` |

---

## 3.10 トランジション

トランジションはクリップ同士を重ねず、**境界に付く独立オブジェクト**として保持する。

```ts
export interface Transition {
  readonly id: string;
  readonly transitionId: string;     // "yave.trans.dissolve" 等
  readonly fromClipId: string;
  readonly toClipId: string | null;  // null の場合は黒/透明とのフェード
  readonly centerFrame: number;      // 境界フレーム
  readonly durationFrames: number;   // 全体長 (前後へ半分ずつ伸びる)
  params: Record<string, any>;
}
```

### 組み込みトランジション一覧

| ID | 内容 | パラメータ |
|---|---|---|
| `yave.trans.dissolve` | クロスディゾルブ | — |
| `yave.trans.fadeToBlack` | 黒を挟むトランジション | `color`: "#000000" |
| `yave.trans.wipe` | ワイプ切り替え | `angle`: 0..360, `softness`: 0..1 |
| `yave.trans.slide` | 次のクリップが押し込む | `direction`: "left" \| "right" \| "up" \| "down" |
| `yave.trans.push` | 前のクリップを押し出す | `direction`: "left" \| "right" \| "up" \| "down" |

---

## 3.11 タイトルクリップ

タイトルクリップ (`TitleClip`) は `SubtitleClip` を継承する。

| 項目 | 字幕 (`SubtitleClip`) | タイトル (`TitleClip`) |
|---|---|---|
| 配置可能トラック | `subtitle` トラック | **`video` トラック** (映像の上に重ねるテロップ) |
| 主な用途 | セリフ、ナレーション字幕 | タイトルロゴ、見出し、クレジット |
| 書き出し時の分離 | 字幕トラックとして SRT/VTT 分離可能 | 映像レイヤーに不可分に焼き込む |
