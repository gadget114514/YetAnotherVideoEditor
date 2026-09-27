# 11. 型定義リファレンス (TypeScript)

[← 目次に戻る](../design.md)

---

本書は、新アーキテクチャの実装出発点としてそのまま使用可能な TypeScript 型定義・インタフェース定義を示す。

---

## 11.1 プリミティブ型・時間型

```ts
// packages/core/src/id/Uuid.ts
export type Uuid = string & { readonly __brand: unique symbol };

export function createUuid(): Uuid {
  return crypto.randomUUID() as Uuid;
}

export function asUuid(id: string): Uuid {
  return id as Uuid;
}

// packages/core/src/time/Rational.ts
export interface Rational {
  readonly num: number;
  readonly den: number;
}

// packages/core/src/time/TimeRange.ts
export interface TimeRange {
  readonly start: number;     // フレーム番号 (整数)
  readonly duration: number;  // フレーム数 (整数, > 0)
}
```

---

## 11.2 クリップ基底 (`Clip`) と具象クラス

```ts
// packages/core/src/model/Clip.ts
import type { Uuid } from '../id/Uuid';
import type { TimeRange } from '../time/TimeRange';
import type { VideoFilterInstance } from './VideoFilter';
import type { LayerItem } from '../snapshot/RenderSnapshot';
import type { Track } from './Track';

export type ClipType = 'video' | 'audio' | 'subtitle' | 'aiPlaceholder' | 'image' | 'color' | 'cut' | 'title';

export interface Transform {
  position: { x: number; y: number };
  scale: { x: number; y: number };
  rotation: number;
  anchor: { x: number; y: number };
}

export interface CropRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export abstract class Clip {
  protected id_: Uuid;
  protected range_: TimeRange;
  protected unknownFields_: Record<string, any> = {};

  constructor(id?: Uuid) {
    this.id_ = id ?? (crypto.randomUUID() as Uuid);
    this.range_ = { start: 0, duration: 60 };
  }

  get id(): Uuid { return this.id_; }
  get range(): TimeRange { return this.range_; }
  setRange(r: TimeRange): void { this.range_ = r; }

  abstract readonly type: ClipType;
  abstract clone(): Clip;

  virtualSourceOffset(): number { return 0; }
  shiftSourceOffset(deltaFrames: number): void {}
  maxDurationFrames(): number { return -1; }

  abstract makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem | null;

  get unknownFields(): Record<string, any> { return this.unknownFields_; }
  setUnknownFields(fields: Record<string, any>): void { this.unknownFields_ = { ...fields }; }
}
```

### 11.2.1 VideoClip / ImageClip / ColorClip
```ts
// packages/core/src/model/VideoClip.ts
export class VideoClip extends Clip {
  readonly type = 'video' as const;
  assetId: Uuid = '' as Uuid;
  sourceOffset: number = 0;
  speed: number = 1.0;
  reversed: boolean = false;
  opacity: number = 1.0;
  blendMode: string = 'normal';
  transform: Transform = { position: { x: 0, y: 0 }, scale: { x: 1, y: 1 }, rotation: 0, anchor: { x: 0.5, y: 0.5 } };
  crop: CropRect = { x: 0, y: 0, width: 1, height: 1 };
  fadeIn: number = 0;
  fadeOut: number = 0;
  filters: VideoFilterInstance[] = [];
  generatedByTaskId: Uuid | null = null;

  clone(): VideoClip { /* 新しい UUID で各プロパティをディープコピー */ return new VideoClip(); }
  makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem { /* ... */ return {} as LayerItem; }
}

export class ImageClip extends Clip {
  readonly type = 'image' as const;
  assetId: Uuid = '' as Uuid;
  opacity: number = 1.0;
  blendMode: string = 'normal';
  transform: Transform = { position: { x: 0, y: 0 }, scale: { x: 1, y: 1 }, rotation: 0, anchor: { x: 0.5, y: 0.5 } };
  filters: VideoFilterInstance[] = [];
  clone(): ImageClip { return new ImageClip(); }
  makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem { return {} as LayerItem; }
}

export class ColorClip extends Clip {
  readonly type = 'color' as const;
  color: string = '#000000';
  opacity: number = 1.0;
  blendMode: string = 'normal';
  clone(): ColorClip { return new ColorClip(); }
  makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem { return {} as LayerItem; }
}
```

### 11.2.2 AudioClip
```ts
// packages/core/src/model/AudioClip.ts
export class AudioClip extends Clip {
  readonly type = 'audio' as const;
  assetId: Uuid = '' as Uuid;
  sourceOffset: number = 0;
  gain: number = 1.0;
  pan: number = 0.0;
  fadeIn: number = 0;
  fadeOut: number = 0;
  fadeInCurve: 'linear' | 'equalPower' = 'equalPower';
  fadeOutCurve: 'linear' | 'equalPower' = 'linear';
  generatedByTaskId: Uuid | null = null;

  clone(): AudioClip { return new AudioClip(); }
  makeLayerItem(): null { return null; }
}
```

### 11.2.3 SubtitleClip / TitleClip
```ts
// packages/core/src/model/SubtitleClip.ts
export interface SubtitleEffectInstance {
  instanceId: Uuid;
  effectId: string;
  pluginId: string;
  enabled: boolean;
  params: Record<string, any>;
}

export class SubtitleClip extends Clip {
  readonly type: ClipType = 'subtitle';
  stylePresetId: string = 'default';
  text: { plain: string; spans: any[] } = { plain: '', spans: [] };
  styleOverride: Partial<any> = {};
  effectStack: SubtitleEffectInstance[] = [];
  wordTimings: any[] = [];
  generatedByTaskId: Uuid | null = null;

  clone(): SubtitleClip { return new SubtitleClip(); }
  makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem { return {} as LayerItem; }
}

export class TitleClip extends SubtitleClip {
  override readonly type = 'title' as const;
  presetId: string = '';
  filters: VideoFilterInstance[] = [];
  override clone(): TitleClip { return new TitleClip(); }
}
```

### 11.2.4 CutClip (AI トラック演出指示)
```ts
// packages/core/src/model/CutClip.ts
export type CutStatus = 'notStarted' | 'rough' | 'inReview' | 'approved';
export type ContinuityMode = 'none' | 'fromBoardImage' | 'fromPreviousEnd' | 'fromCutId';
export type ShotSize = 'unspecified' | 'extremeWide' | 'wide' | 'full' | 'medium' | 'closeUp' | 'extremeCloseUp';
export type CameraAngle = 'unspecified' | 'eyeLevel' | 'high' | 'low' | 'birdsEye' | 'wormsEye' | 'dutch';
export type CameraMovement = 'unspecified' | 'fixed' | 'panLeft' | 'panRight' | 'tiltUp' | 'tiltDown' | 'dolly' | 'slowPushIn' | 'pullOut' | 'handheld' | 'crane' | 'follow';
export type TransitionKind = 'cut' | 'dissolve' | 'fadeToBlack' | 'fadeFromBlack' | 'wipe' | 'matchCut';

export interface CameraWork {
  size: ShotSize;
  angle: CameraAngle;
  movement: CameraMovement;
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
  strength: number;
  sceneBreak: boolean;
}

export class CutClip extends Clip {
  readonly type = 'cut' as const;
  label: string = '';
  slug: string = '';
  description: string = '';
  dialogue: string = '';
  mood: string = '';
  camera: CameraWork = { size: 'unspecified', angle: 'eyeLevel', movement: 'unspecified', note: '' };
  characterIds: Uuid[] = [];
  locationId?: Uuid;
  transitionIn: TransitionKind = 'cut';
  transitionOut: TransitionKind = 'cut';
  board: BoardImage = { origin: 'none' };
  continuity: Continuity = { mode: 'fromBoardImage', strength: 0.9, sceneBreak: false };
  status: CutStatus = 'notStarted';
  reviewNote: string = '';
  paramPatch: Record<string, any> = {};
  biblePatch: Record<string, any> = {};
  outputs: any[] = [];
  generatedByTaskId: Uuid | null = null;

  clone(): CutClip { return new CutClip(); }
  makeLayerItem(frame: number, zIndex: number, track: Track): LayerItem | null { return null; }
  specHash(role: string, tag?: string): string { return ''; }
}
```

---

## 11.3 トラック・タイムライン・プロジェクト

```ts
// packages/core/src/model/Track.ts
export type TrackType = 'video' | 'audio' | 'subtitle' | 'aiGenerated' | 'storyboard' | 'unknown';

export class Track {
  readonly id: Uuid;
  name: string;
  type: TrackType;
  visible: boolean = true;
  locked: boolean = false;
  muted: boolean = false;
  solo: boolean = false;
  opacity: number = 1.0;
  blendMode: string = 'normal';
  height: number = 64;
  color: string = '';
  aiRole: string = '';
  storyboardTrackId: Uuid | null = null;
  roleDefaults: Record<string, any> = {};
  effectChain: any[] = [];

  private clips_: Clip[] = [];
  private transitions_: Transition[] = [];

  constructor(type: TrackType, id?: Uuid) {
    this.type = type;
    this.id = id ?? (crypto.randomUUID() as Uuid);
    this.name = type;
  }

  get clips(): readonly Clip[] { return this.clips_; }
  get transitions(): readonly Transition[] { return this.transitions_; }

  participatesInComposite(): boolean {
    return this.type === 'video' || this.type === 'subtitle' || this.type === 'aiGenerated';
  }

  participatesInAudioGraph(): boolean {
    return this.type === 'audio' || this.type === 'aiGenerated';
  }

  acceptsClip(clip: Clip): boolean {
    if (this.type === 'storyboard') return clip.type === 'cut';
    if (clip.type === 'cut') return false;
    if (this.type === 'video') return clip.type === 'video' || clip.type === 'image' || clip.type === 'color' || clip.type === 'title' || clip.type === 'aiPlaceholder';
    if (this.type === 'audio') return clip.type === 'audio' || clip.type === 'aiPlaceholder';
    if (this.type === 'subtitle') return clip.type === 'subtitle';
    if (this.type === 'aiGenerated') return true;
    return false;
  }

  clipAt(frame: number): Clip | null { /* 二分探索 */ return null; }
  insertClip(clip: Clip): void { /* ソート順序を維持して挿入 */ }
  removeClip(clipId: Uuid): boolean { /* ... */ return true; }
}

// packages/core/src/model/Timeline.ts
export class Timeline {
  private tracks_: Track[] = [];
  private revision_: number = 0;
  timebase: Rational = { num: 1001, den: 60000 };
  canvasSize: { width: number; height: number } = { width: 1920, height: 1080 };

  get tracks(): readonly Track[] { return this.tracks_; }
  get revision(): number { return this.revision_; }

  appendTrack(type: TrackType): Track {
    const t = new Track(type);
    this.tracks_.push(t);
    this.revision_++;
    return t;
  }

  moveTrack(fromIndex: number, toIndex: number): void {
    if (fromIndex === toIndex || fromIndex < 0 || toIndex < 0) return;
    const [t] = this.tracks_.splice(fromIndex, 1);
    if (t) {
      this.tracks_.splice(toIndex, 0, t);
      this.revision_++;
    }
  }

  buildSnapshot(frame: number): RenderSnapshot {
    // 3.3 参照
    return {} as RenderSnapshot;
  }
}

// packages/core/src/model/Project.ts
export class Project {
  readonly id: Uuid = crypto.randomUUID() as Uuid;
  name: string = 'Untitled';
  timeline: Timeline = new Timeline();
  commandStack: CommandStack = new CommandStack();
  assets: Map<Uuid, Asset> = new Map();
  storyBible: StoryBible = { artStyle: '', negativePrompt: '', promptPrefix: '', promptSuffix: '', characters: [], locations: [], roleDefaults: {}, promptTemplates: {} };
  library: LibraryStore = new LibraryStore();
  playhead: number = 0;
  workRange: TimeRange = { start: 0, duration: 108000 };
}
```

---

## 11.4 Undo / Redo (`CommandStack`)

```ts
// packages/core/src/commands/Command.ts
export interface Command {
  readonly name: string;
  redo(): void | Promise<void>;
  undo(): void | Promise<void>;
  mergeWith?(nextCommand: Command): boolean;
}

// packages/core/src/commands/CommandStack.ts
export class CommandStack {
  private undoList_: Command[] = [];
  private redoList_: Command[] = [];
  private cleanIndex_: number = 0;

  async push(cmd: Command): Promise<void> {
    if (this.undoList_.length > 0) {
      const top = this.undoList_[this.undoList_.length - 1]!;
      if (top.mergeWith && top.mergeWith(cmd)) {
        await cmd.redo();
        return;
      }
    }
    await cmd.redo();
    this.undoList_.push(cmd);
    this.redoList_ = [];
  }

  async undo(): Promise<void> {
    const cmd = this.undoList_.pop();
    if (cmd) {
      await cmd.undo();
      this.redoList_.push(cmd);
    }
  }

  async redo(): Promise<void> {
    const cmd = this.redoList_.pop();
    if (cmd) {
      await cmd.redo();
      this.undoList_.push(cmd);
    }
  }

  get canUndo(): boolean { return this.undoList_.length > 0; }
  get canRedo(): boolean { return this.redoList_.length > 0; }
  get isClean(): boolean { return this.undoList_.length === this.cleanIndex_; }
  setClean(): void { this.cleanIndex_ = this.undoList_.length; }
}
```

---

## 11.5 所有関係まとめ

```
Project (ルート集約)
 ├── Timeline (インスタンス)
 │     └── Track[] (配列。インデックス順 = Z オーダー)
 │           ├── Clip[] (Track が所有。Command も Undo 用に参照)
 │           │     ├── VideoFilterInstance[] (Clip 所有の値)
 │           │     └── SubtitleClip / TitleClip
 │           │           └── SubtitleEffectInstance[]
 │           └── Transition[] (境界に付く値オブジェクト)
 ├── CommandStack (所有)
 │     └── Command[] (Undo 用に Clip の参照を保持)
 ├── Asset[] (登録アセットのテーブル)
 ├── StoryBible (作品全体設定)
 └── LibraryStore (素材フォルダ木)

PlatformHost (DI アダプタ)
 ├── KeyValueStore (settings: localStorage / userData JSON)
 └── SecretStore (secrets: safeStorage / IndexedDB)

WebGlCompositor (Render スレッド / ループ)
 └── RenderSnapshot (フレームごとに Timeline から受け取る不変オブジェクト)
```
