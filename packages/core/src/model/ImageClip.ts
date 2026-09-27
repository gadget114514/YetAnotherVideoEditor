import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export class ImageClip extends Clip {
  readonly type: ClipType = 'image';
  assetId: Uuid = createUuid();

  clone(): ImageClip {
    const c = new ImageClip();
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.assetId = this.assetId;
    return c;
  }

  makeLayerItem(frame: number, zIndex: number, track: any): LayerItem | null {
    if (!this.enabled || !track.visible) return null;
    return {
      trackId: track.id,
      clipId: this.id,
      zIndex,
      opacity: this.effectiveOpacity(frame) * (track.opacity ?? 1.0),
      blendMode: this.blendMode,
      transform: { ...this.transform },
      crop: { ...this.crop },
      filters: [...this.filters],
    };
  }
}
