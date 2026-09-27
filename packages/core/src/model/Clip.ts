import { createUuid, type Uuid } from '../id/Uuid.js';
import type { TimeRange } from '../time/TimeRange.js';
import { BlendMode } from './BlendMode.js';
import type { VideoFilterInstance } from './VideoFilter.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export type ClipType =
  | 'video'
  | 'audio'
  | 'subtitle'
  | 'aiPlaceholder'
  | 'image'
  | 'color'
  | 'cut'
  | 'title';

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
  private id_: Uuid;
  private name_: string = '';
  private range_: TimeRange = { start: 0, duration: 60 };
  private enabled_: boolean = true;
  private locked_: boolean = false;
  private opacity_: number = 1.0;
  private blendMode_: BlendMode = BlendMode.Normal;
  private transform_: Transform = {
    position: { x: 0, y: 0 },
    scale: { x: 1, y: 1 },
    rotation: 0,
    anchor: { x: 0.5, y: 0.5 },
  };
  private crop_: CropRect = { x: 0, y: 0, width: 1, height: 1 };
  private fadeIn_: number = 0;
  private fadeOut_: number = 0;
  private filters_: VideoFilterInstance[] = [];
  private generatedByTaskId_: Uuid | null = null;
  private unknownFields_: Record<string, any> = {};

  constructor(id?: Uuid) {
    this.id_ = id ?? createUuid();
  }

  abstract readonly type: ClipType;
  abstract clone(): Clip;

  get id(): Uuid {
    return this.id_;
  }
  setId(id: Uuid): void {
    this.id_ = id;
  }

  get name(): string {
    return this.name_;
  }
  set name(n: string) {
    this.name_ = n;
  }
  setName(name: string): void {
    this.name_ = name;
  }

  get range(): TimeRange {
    return this.range_;
  }
  set range(r: TimeRange) {
    this.setRange(r);
  }
  setRange(r: TimeRange): void {
    this.range_ = { start: Math.trunc(r.start), duration: Math.max(0, Math.trunc(r.duration)) };
  }

  get enabled(): boolean {
    return this.enabled_;
  }
  set enabled(e: boolean) {
    this.enabled_ = e;
  }
  setEnabled(e: boolean): void {
    this.enabled_ = e;
  }

  get locked(): boolean {
    return this.locked_;
  }
  set locked(l: boolean) {
    this.locked_ = l;
  }
  setLocked(l: boolean): void {
    this.locked_ = l;
  }

  get opacity(): number {
    return this.opacity_;
  }
  set opacity(o: number) {
    this.setOpacity(o);
  }
  setOpacity(o: number): void {
    this.opacity_ = Math.max(0, Math.min(1, o));
  }

  get blendMode(): BlendMode {
    return this.blendMode_;
  }
  set blendMode(m: BlendMode) {
    this.blendMode_ = m;
  }
  setBlendMode(m: BlendMode): void {
    this.blendMode_ = m;
  }

  get transform(): Transform {
    return this.transform_;
  }
  setTransform(t: Transform): void {
    this.transform_ = { ...t };
  }

  get crop(): CropRect {
    return this.crop_;
  }
  setCrop(c: CropRect): void {
    this.crop_ = { ...c };
  }

  get fadeIn(): number {
    return this.fadeIn_;
  }
  setFadeIn(f: number): void {
    this.fadeIn_ = Math.max(0, Math.trunc(f));
  }
  setFadeInFrames(f: number): void {
    this.fadeIn_ = Math.max(0, Math.trunc(f));
  }
  fadeInFrames(): number {
    return this.fadeIn_;
  }

  get fadeOut(): number {
    return this.fadeOut_;
  }
  setFadeOut(f: number): void {
    this.fadeOut_ = Math.max(0, Math.trunc(f));
  }
  setFadeOutFrames(f: number): void {
    this.fadeOut_ = Math.max(0, Math.trunc(f));
  }
  fadeOutFrames(): number {
    return this.fadeOut_;
  }

  get filters(): readonly VideoFilterInstance[] {
    return this.filters_;
  }
  setFilters(f: VideoFilterInstance[]): void {
    this.filters_ = [...f];
  }
  addFilter(filter: VideoFilterInstance): void {
    this.filters_.push({ ...filter, params: { ...filter.params } });
  }
  removeFilter(index: number): VideoFilterInstance | null {
    if (index >= 0 && index < this.filters_.length) {
      return this.filters_.splice(index, 1)[0] ?? null;
    }
    return null;
  }
  moveFilter(from: number, to: number): void {
    if (from >= 0 && from < this.filters_.length && to >= 0 && to < this.filters_.length) {
      const [item] = this.filters_.splice(from, 1);
      if (item) {
        this.filters_.splice(to, 0, item);
      }
    }
  }
  setFilterEnabled(index: number, enabled: boolean): void {
    if (index >= 0 && index < this.filters_.length) {
      this.filters_[index]!.enabled = enabled;
    }
  }
  resolvedFilters(): VideoFilterInstance[] {
    return this.filters_.filter((f) => f.enabled !== false);
  }

  get generatedByTaskId(): Uuid | null {
    return this.generatedByTaskId_;
  }
  setGeneratedByTaskId(id: Uuid | null): void {
    this.generatedByTaskId_ = id;
  }
  isAiGenerated(): boolean {
    return this.generatedByTaskId_ !== null;
  }

  get unknownFields(): Record<string, any> {
    return this.unknownFields_;
  }
  setUnknownFields(fields: Record<string, any>): void {
    this.unknownFields_ = { ...fields };
  }

  sourceOffset(): number {
    return 0;
  }
  setSourceOffset(_offset: number): void {}
  shiftSourceOffset(delta: number): void {
    this.setSourceOffset(this.sourceOffset() + delta);
  }
  maxDuration(): number {
    return -1;
  }

  effectiveOpacity(frame: number): number {
    if (!this.enabled_) return 0;
    let op = this.opacity_;
    const local = frame - this.range_.start;
    if (this.fadeIn_ > 0 && local < this.fadeIn_) {
      op *= Math.max(0, local / this.fadeIn_);
    }
    const rem = this.range_.duration - local;
    if (this.fadeOut_ > 0 && rem < this.fadeOut_) {
      op *= Math.max(0, rem / this.fadeOut_);
    }
    return op;
  }

  abstract makeLayerItem(frame: number, zIndex: number, track: any): LayerItem | null;

  protected copyBaseTo(dst: Clip): void {
    dst.name_ = this.name_;
    dst.range_ = { ...this.range_ };
    dst.enabled_ = this.enabled_;
    dst.locked_ = this.locked_;
    dst.opacity_ = this.opacity_;
    dst.blendMode_ = this.blendMode_;
    dst.transform_ = { ...this.transform_ };
    dst.crop_ = { ...this.crop_ };
    dst.fadeIn_ = this.fadeIn_;
    dst.fadeOut_ = this.fadeOut_;
    dst.filters_ = this.filters_.map((f) => ({ ...f, params: { ...f.params } }));
    dst.generatedByTaskId_ = this.generatedByTaskId_;
    dst.unknownFields_ = { ...this.unknownFields_ };
  }
}
