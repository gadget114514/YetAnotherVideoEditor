import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

export class AiPlaceholderClip extends Clip {
  readonly type: ClipType = 'aiPlaceholder';
  taskId: Uuid = createUuid();
  progress: number = 0;
  prompt: string = '';

  constructor(taskId?: Uuid) {
    super();
    if (taskId) {
      this.taskId = taskId;
    }
  }

  clone(): AiPlaceholderClip {
    const c = new AiPlaceholderClip(this.taskId);
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.progress = this.progress;
    c.prompt = this.prompt;
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
