import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export class AudioClip extends Clip {
  readonly type: ClipType = 'audio';
  private assetId_: Uuid = createUuid();
  private sourceOffset_: number = 0;
  private maxDurationFrames_: number = -1;
  private gain_: number = 1.0;
  private pan_: number = 0.0;
  private fadeInCurve_: 'linear' | 'equalPower' = 'equalPower';
  private fadeOutCurve_: 'linear' | 'equalPower' = 'linear';

  constructor(assetId?: Uuid) {
    super();
    if (assetId) {
      this.assetId_ = assetId;
    }
  }

  clone(): AudioClip {
    const c = new AudioClip(this.assetId_);
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.sourceOffset_ = this.sourceOffset_;
    c.maxDurationFrames_ = this.maxDurationFrames_;
    c.gain_ = this.gain_;
    c.pan_ = this.pan_;
    c.fadeInCurve_ = this.fadeInCurve_;
    c.fadeOutCurve_ = this.fadeOutCurve_;
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

  get gain(): number {
    return this.gain_;
  }
  set gain(g: number) {
    this.setGain(g);
  }
  setGain(g: number): void {
    this.gain_ = Math.max(0, g);
  }

  get pan(): number {
    return this.pan_;
  }
  set pan(p: number) {
    this.setPan(p);
  }
  setPan(p: number): void {
    this.pan_ = Math.max(-1.0, Math.min(1.0, p));
  }

  get fadeInCurve(): 'linear' | 'equalPower' {
    return this.fadeInCurve_;
  }
  setFadeInCurve(c: 'linear' | 'equalPower'): void {
    this.fadeInCurve_ = c;
  }

  get fadeOutCurve(): 'linear' | 'equalPower' {
    return this.fadeOutCurve_;
  }
  setFadeOutCurve(c: 'linear' | 'equalPower'): void {
    this.fadeOutCurve_ = c;
  }

  makeLayerItem(): LayerItem | null {
    return null;
  }
}
