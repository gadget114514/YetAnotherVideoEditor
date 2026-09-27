# 6. 字幕エンジン

[← 目次に戻る](../design.md)

---

## 6.1 設計の要点

1. **SRT の 1 キュー = タイムライン上の 1 区間 (`SubtitleClip`)** に 1:1 変換する。取り込み後は、他のクリップと完全に同じ操作 (移動 / トリム / 分割 / 複製 / 削除 / 別トラックへ移動 / リップル編集) ができる。字幕を特別扱いしない。
2. 字幕区間は「**テキスト + スタイル + エフェクトスタック**」で構成される。
3. **アニメーションは JS プラグインで拡張可能**。組み込みエフェクトも外部プラグインも同一の `ISubtitleEffect` インタフェースで実装する。
4. エフェクトは**ラスタライズ済みグリフには触れず、グリフごとの変換パラメータ (行列・透明度・色) のみを書き換える**。これによりテキストの再ラスタライズなしで毎フレーム変化でき、60fps を維持できる。
5. **AviUtl ブリッジ機能は廃止**とする (Web 移行に伴うプラットフォーム共通化のため)。

---

## 6.2 データ構造

```ts
export interface TextSpan {
  readonly start: number;  // UTF-16 インデックス
  readonly length: number;
  readonly bold?: boolean;
  readonly italic?: boolean;
  readonly underline?: boolean;
  readonly color?: string;
  readonly fontFamily?: string;
  readonly sizeScale?: number;
  readonly ruby?: string; // 振り仮名
}

export interface SubtitleText {
  plain: string;
  spans: TextSpan[];
}

export interface SubtitleStyle {
  fontFamily: string;
  fontPointSize: number;
  fontWeight: number;
  italic: boolean;
  fillColor: string;
  outlineColor: string;
  outlineWidth: number;
  shadowColor: string;
  shadowOffset: { x: number; y: number };
  shadowBlur: number;
  boxEnabled: boolean;
  boxColor: string;
  hAlign: 'left' | 'center' | 'right';
  vAlign: 'top' | 'middle' | 'bottom';
  lineSpacing: number;
  letterSpacing: number;
}
```

---

## 6.3 SRT パーサと文字コード自動判別

外部 SRT ファイルを安全に取り込むため、UTF-8 と Shift_JIS のフォールバック処理を実装する。

```ts
// packages/io/src/srt/SrtParser.ts
export function decodeSrtBuffer(buffer: ArrayBuffer): string {
  try {
    const utf8Decoder = new TextDecoder('utf-8', { fatal: true });
    return utf8Decoder.decode(buffer);
  } catch {
    // UTF-8 デコード失敗時は Shift_JIS としてデコード
    const sjisDecoder = new TextDecoder('shift_jis');
    return sjisDecoder.decode(buffer);
  }
}
```

### タイムコードの有理数変換
SRT 内のミリ秒表記 (`00:01:23,456`) は、パーサの境界で直ちに `secondsToFrames(seconds, projectTimebase, 'nearest')` によりフレーム番号へ変換する。内部データ構造に秒を保持しない。

---

## 6.4 テキストレイアウト (OffscreenCanvas 2D)

ブラウザの `OffscreenCanvas` と `CanvasRenderingContext2D` の `measureText()` を活用し、テキストを行単位およびグリフ (文字) 単位に分解して配置情報を算出する。

```ts
export interface GlyphInfo {
  readonly charIndex: number;
  readonly char: string;
  readonly lineIndex: number;
  readonly isWhitespace: boolean;
  readonly layoutRect: { x: number; y: number; width: number; height: number };
  readonly baseColor: string;
}

export interface SubtitleGlyphRun {
  readonly lineCount: number;
  readonly blockSize: { width: number; height: number };
  readonly glyphs: readonly GlyphInfo[];
}
```

---

## 6.5 グリフアトラス

毎フレームのテキスト再ラスタライズを避けるため、テキストレイアウト結果を `OffscreenCanvas` 上に一度だけレンダリングし、WebGL2 のテクスチャ (`GlyphAtlas`) としてキャッシュする。

- **キャッシュキー**: `sha256(text + JSON.stringify(style) + canvasWidth + canvasHeight)`
- テキストやスタイルが変更されない限り、再生中は同一のテクスチャをサンプリングし、グリフごとのアフィン変換のみを頂点シェーダで適用する。

---

## 6.6 エフェクトプラグイン機構 (`ISubtitleEffect`)

組み込みエフェクトおよびサードパーティ JS プラグインはすべて `ISubtitleEffect` を実装する。

```ts
// packages/plugin-sdk/src/subtitleEffect.ts
export interface GlyphTransform {
  transform: number[]; // 4x4 matrix
  color: string;
  opacity: number;
  visible: boolean;
  blurRadius: number;
}

export interface SubtitleEffectFrame {
  readonly run: SubtitleGlyphRun;
  readonly canvasSize: { width: number; height: number };
  readonly clipStartFrame: number;
  readonly clipDuration: number;
  readonly currentFrame: number;
  readonly fps: number;
  // 書き換え可能ターゲット
  glyphs: GlyphTransform[];
  blockTransform: number[];
  blockOpacity: number;
}

export interface SubtitleTimeInfo {
  readonly progress: number; // 0.0 (In) .. 1.0 (Out)
  readonly secondsFromIn: number;
  readonly secondsToOut: number;
  readonly clipDurationSec: number;
}

export interface ParameterSchemaItem {
  readonly key: string;
  readonly displayNameKey: string;
  readonly type: 'bool' | 'int' | 'double' | 'color' | 'string' | 'enum';
  readonly defaultValue: any;
  readonly minValue?: number;
  readonly maxValue?: number;
  readonly step?: number;
  readonly enumChoices?: readonly string[];
  readonly unitSuffix?: string;
}

export interface ISubtitleEffect {
  readonly id: string;
  readonly displayName: string;
  readonly category: string;
  getParameterSchema(): readonly ParameterSchemaItem[];
  prepare?(run: SubtitleGlyphRun, params: Record<string, any>): void;
  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void;
}
```

### 組み込みエフェクト一覧

| ID | 名前 | パラメータ | 動作 |
|---|---|---|---|
| `yave.fade` | フェード | `inDuration`, `outDuration`, `curve` | イン点とアウト点で透明度をフェード |
| `yave.typewriter` | タイプライター | `charsPerSecond`, `startDelay`, `cursorVisible` | 1 文字ずつ順に出現 |
| `yave.karaoke` | カラオケ | `highlightColor`, `mode`, `preRoll` | 再生進捗または wordTimings に連動して文字色を変化 |
| `yave.slidein` | スライドイン | `direction`, `distance`, `duration`, `easing` | 画面外からスライドして進入 |
| `yave.popperchar` | 文字ごとポップ | `stagger`, `overshoot`, `duration` | 文字ごとにスケールが跳ねて出現 |
| `yave.wave` | ウェーブ | `amplitude`, `frequency`, `speed`, `axis` | 文字が波打つように Y 座標を振動 |
| `yave.blurin` | ブラーイン | `startBlur`, `duration` | ぼやけた状態からシャープに出現 |
| `yave.shake` | シェイク | `amplitude`, `frequency`, `seed` | 揺れ・振動エフェクト |

---

## 6.7 字幕の書き出し

- **SRT 書き出し**: `SrtWriter` がタイムライン上の `SubtitleClip` を In 点順にソートし、プロジェクトタイムベースから秒とミリ秒に変換して標準 SRT 文字列を生成。
- **VTT 書き出し**: Web 用の WebVTT (`WEBVTT` ヘッダ、ドット区切りのタイムコード) フォーマットを出力可能。
- **焼き込み書き出し**: 動画書き出し (`ExportJob`) 時に、WebGL2 合成パイプラインの最前面レイヤーとして字幕テクスチャをレンダリング。
