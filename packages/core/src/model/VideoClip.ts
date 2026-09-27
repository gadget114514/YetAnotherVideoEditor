import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export class VideoClip extends Clip {
  readonly type: ClipType = 'video';
  private assetId_: Uuid = createUuid();
  private sourceOffset_: number = 0;
  private maxDurationFrames_: number = -1;
  private speed_: number = 1.0;
  private reversed_: boolean = false;

  constructor(assetId?: Uuid) {
    super();
    if (assetId) {
      this.assetId_ = assetId;
    }
  }

  clone(): VideoClip {
    const c = new VideoClip(this.assetId_);
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.sourceOffset_ = this.sourceOffset_;
    c.maxDurationFrames_ = this.maxDurationFrames_;
    c.speed_ = this.speed_;
    c.reversed_ = this.reversed_;
    return c;
  }

  get assetId(): Uuid {
    return this.assetId_;
  }
  setAssetId(id: Uuid): void {
    this.assetId_ = id;
  }

  override sourceOffset(): number {
    return this.sourceOffset_;
  }
  override setSourceOffset(off: number): void {
    this.sourceOffset_ = Math.max(0, Math.trunc(off));
  }

  get maxDurationFrames(): number {
    return this.maxDurationFrames_;
  }
  setMaxDurationFrames(f: number): void {
    this.maxDurationFrames_ = f;
  }
  override maxDuration(): number {
    return this.maxDurationFrames_;
  }

  get speed(): number {
    return this.speed_;
  }
  set speed(s: number) {
    this.setSpeed(s);
  }
  setSpeed(s: number): void {
    this.speed_ = Math.max(0.01, Math.min(100.0, s));
  }

  get reversed(): boolean {
    return this.reversed_;
  }
  set reversed(r: boolean) {
    this.setReversed(r);
  }
  setReversed(r: boolean): void {
    this.reversed_ = r;
  }

  mapToSourceFrame(timelineFrame: number): number {
    const local = timelineFrame - this.range.start;
    if (local < 0) return this.sourceOffset_;
    const scaled = Math.trunc(local * this.speed_);
    if (!this.reversed_) {
      return this.sourceOffset_ + scaled;
    } else {
      const dur = this.range.duration;
      return this.sourceOffset_ + Math.max(0, dur - 1 - scaled);
    }
  }

  makeLayerItem(frame: number, zIndex: number, track: any): LayerItem | null {
    if (!this.enabled || !track.visible) return null;
    const finalOpacity = this.effectiveOpacity(frame) * (track.opacity ?? 1.0);
    if (finalOpacity <= 0) return null;

    return {
      trackId: track.id,
      clipId: this.id,
      zIndex,
      opacity: finalOpacity,
      blendMode: this.blendMode,
      transform: { ...this.transform },
      crop: { ...this.crop },
      filters: [...this.filters],
    };
  }
}
