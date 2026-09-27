import { createUuid, type Uuid } from '../id/Uuid.js';

export interface Transition {
  id: Uuid;
  transitionId: string;
  fromClipId?: Uuid;
  toClipId?: Uuid;
  centerFrame: number;
  durationFrames: number;
  params: Record<string, any>;
  startFrame(): number;
  endFrame(): number;
  contains(f: number): boolean;
  progressAt(f: number): number;
}

export class TransitionInstance implements Transition {
  id: Uuid = createUuid();
  transitionId: string = builtinTransition.kDissolve;
  fromClipId?: Uuid;
  toClipId?: Uuid;
  centerFrame: number = 0;
  durationFrames: number = 0;
  params: Record<string, any> = {};

  startFrame(): number {
    return this.centerFrame - Math.floor(this.durationFrames / 2);
  }

  endFrame(): number {
    return this.startFrame() + this.durationFrames;
  }

  contains(f: number): boolean {
    return f >= this.startFrame() && f < this.endFrame();
  }

  progressAt(f: number): number {
    if (this.durationFrames <= 0) return 1.0;
    const rel = (f - this.startFrame()) / this.durationFrames;
    return Math.max(0.0, Math.min(1.0, rel));
  }
}

export const builtinTransition = {
  kDissolve: 'yave.trans.dissolve',
  kFadeToBlack: 'yave.trans.fadeToBlack',
  kWipe: 'yave.trans.wipe',
  kSlide: 'yave.trans.slide',
  kPush: 'yave.trans.push',
} as const;

export interface TransitionDesc {
  transitionId: string;
  displayNameKey: string;
  defaultParams: Record<string, any>;
  allowsMissingPartner?: boolean;
}

export const builtinTransitions: readonly TransitionDesc[] = [
  {
    transitionId: builtinTransition.kDissolve,
    displayNameKey: 'transition.dissolve.name',
    defaultParams: {},
    allowsMissingPartner: false,
  },
  {
    transitionId: builtinTransition.kFadeToBlack,
    displayNameKey: 'transition.fadeToBlack.name',
    defaultParams: { color: '#000000' },
    allowsMissingPartner: true,
  },
  {
    transitionId: builtinTransition.kWipe,
    displayNameKey: 'transition.wipe.name',
    defaultParams: { angle: 0.0, softness: 0.05 },
    allowsMissingPartner: false,
  },
  {
    transitionId: builtinTransition.kSlide,
    displayNameKey: 'transition.slide.name',
    defaultParams: { direction: 0 },
    allowsMissingPartner: false,
  },
  {
    transitionId: builtinTransition.kPush,
    displayNameKey: 'transition.push.name',
    defaultParams: { direction: 0 },
    allowsMissingPartner: false,
  },
];

export function findTransitionDesc(transitionId: string): TransitionDesc | null {
  return builtinTransitions.find((d) => d.transitionId === transitionId) ?? null;
}

export function transitionShaderMode(transitionId: string): number {
  if (transitionId === builtinTransition.kFadeToBlack) return 1;
  if (transitionId === builtinTransition.kWipe) return 2;
  if (transitionId === builtinTransition.kSlide) return 3;
  if (transitionId === builtinTransition.kPush) return 4;
  return 0; // dissolve
}

export function resolveTransitionParams(t: Transition): number[] {
  const out = [0.0, 0.0, 0.0, 0.0];
  const p = t.params || {};

  if (t.transitionId === builtinTransition.kWipe) {
    const angle = typeof p['angle'] === 'number' ? p['angle'] : 0.0;
    out[0] = (angle * Math.PI) / 180.0;
    out[1] = typeof p['softness'] === 'number' ? p['softness'] : 0.05;
  } else if (
    t.transitionId === builtinTransition.kSlide ||
    t.transitionId === builtinTransition.kPush
  ) {
    out[0] = typeof p['direction'] === 'number' ? p['direction'] : 0.0;
  }
  return out;
}

export function resolveTransitionColor(t: Transition): number[] {
  const out = [0.0, 0.0, 0.0, 1.0];
  const p = t.params || {};
  const c = p['color'];
  if (typeof c === 'string' && c.startsWith('#')) {
    const hex = c.replace('#', '');
    if (hex.length === 6) {
      out[0] = parseInt(hex.substring(0, 2), 16) / 255.0;
      out[1] = parseInt(hex.substring(2, 4), 16) / 255.0;
      out[2] = parseInt(hex.substring(4, 6), 16) / 255.0;
      out[3] = 1.0;
    }
  }
  return out;
}
