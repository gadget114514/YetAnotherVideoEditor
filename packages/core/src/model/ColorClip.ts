import { createUuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export class ColorClip extends Clip {
  readonly type: ClipType = 'color';
  color: string = '#000000';

  clone(): ColorClip {
    const c = new ColorClip();
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.color = this.color;
    return c;
  }

  makeLayerItem(frame: number, zIndex: number, track: any): LayerItem | null {
    if (!this.enabled || !track.visible) return null;
    return {
      trackId: track.id,
      clipId: this.id,
      zIndex,
      opacity: this.effectiveOpacity(frame),
      blendMode: this.blendMode,
      transform: { ...this.transform },
      crop: { ...this.crop },
      filters: [...this.filters],
    };
  }
}
