import type { Uuid } from '../id/Uuid.js';
import type { Rational } from '../time/Rational.js';
import type { BlendMode } from '../model/BlendMode.js';
import type { VideoFilterInstance } from '../model/VideoFilter.js';

export interface TransitionRef {
  transitionId: string;
  progress: number;
  isIncoming: boolean;
  shaderMode: number;
  params: number[];
  color: number[];
}

export interface LayerItem {
  trackId: Uuid;
  clipId: Uuid;
  zIndex: number;
  opacity: number;
  blendMode: BlendMode;
  transform: {
    position: { x: number; y: number };
    scale: { x: number; y: number };
    rotation: number;
    anchor: { x: number; y: number };
  };
  crop: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
  filters: VideoFilterInstance[];
  transition?: TransitionRef;
}

export interface RenderSnapshot {
  readonly frameIndex: number;
  readonly timebase: Rational;
  readonly layers: readonly LayerItem[];
}
