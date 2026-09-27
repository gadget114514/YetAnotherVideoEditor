import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';

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
  readonly type: ClipType = 'cut';
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

  clone(): CutClip {
    const c = new CutClip();
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.label = this.label;
    c.slug = this.slug;
    c.description = this.description;
    c.dialogue = this.dialogue;
    c.mood = this.mood;
    c.camera = { ...this.camera };
    c.characterIds = [...this.characterIds];
    c.locationId = this.locationId;
    c.transitionIn = this.transitionIn;
    c.transitionOut = this.transitionOut;
    c.board = { ...this.board };
    c.continuity = { ...this.continuity };
    c.status = this.status;
    c.reviewNote = this.reviewNote;
    c.paramPatch = { ...this.paramPatch };
    c.biblePatch = { ...this.biblePatch };
    c.outputs = [...this.outputs];
    return c;
  }

  makeLayerItem(): LayerItem | null {
    return null;
  }
}
