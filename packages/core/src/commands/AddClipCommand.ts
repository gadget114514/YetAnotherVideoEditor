import type { Command } from './Command.js';
import type { Project } from '../model/Project.js';
import type { Uuid } from '../id/Uuid.js';
import type { Clip } from '../model/Clip.js';

export class AddClipCommand implements Command {
  readonly name: string = 'Add clip';
  private project_: Project;
  private trackId_: Uuid;
  private trackIndex_: number;
  private clip_: Clip;
  private inserted_: boolean = false;
  private rejectReason_: string = '';

  constructor(project: Project, trackId: Uuid | string, trackIndexOrClip: number | Clip, clip?: Clip) {
    this.project_ = project;
    this.trackId_ = trackId as Uuid;
    if (typeof trackIndexOrClip === 'number') {
      this.trackIndex_ = trackIndexOrClip;
      this.clip_ = clip!;
    } else {
      this.trackIndex_ = -1;
      this.clip_ = trackIndexOrClip;
    }
  }

  get wasInserted(): boolean {
    return this.inserted_;
  }

  get wasAdded(): boolean {
    return this.inserted_;
  }

  get rejectReason(): string {
    return this.rejectReason_;
  }

  redo(): void {
    let t = this.project_.timeline.trackById(this.trackId_);
    if (!t && this.trackIndex_ >= 0) {
      t = this.project_.timeline.trackAt(this.trackIndex_);
    }
    if (!t) {
      this.inserted_ = false;
      this.rejectReason_ = 'Track not found';
      return;
    }

    this.inserted_ = t.insertClip(this.clip_);
    if (!this.inserted_) {
      this.rejectReason_ = 'Clip overlaps with an existing clip';
    } else {
      this.rejectReason_ = '';
    }
  }

  undo(): void {
    if (this.inserted_) {
      let t = this.project_.timeline.trackById(this.trackId_);
      if (!t && this.trackIndex_ >= 0) {
        t = this.project_.timeline.trackAt(this.trackIndex_);
      }
      if (t) {
        t.removeClip(this.clip_.id);
      }
    }
  }
}

export class RemoveClipCommand implements Command {
  readonly name: string = 'Remove clip';
  private project_: Project;
  private trackId_: Uuid;
  private clipId_: Uuid;
  private removedClip_: Clip | null = null;

  constructor(project: Project, trackIdOrClipId: Uuid | string, clipId?: Uuid | string) {
    this.project_ = project;
    if (clipId !== undefined) {
      this.trackId_ = trackIdOrClipId as Uuid;
      this.clipId_ = clipId as Uuid;
    } else {
      this.clipId_ = trackIdOrClipId as Uuid;
      const found = project.timeline.findClipWithTrack(this.clipId_);
      this.trackId_ = found ? found.track.id : ('' as Uuid);
    }
  }

  get wasRemoved(): boolean {
    return this.removedClip_ !== null;
  }

  get removedClip(): Clip | null {
    return this.removedClip_;
  }

  redo(): void {
    let t = this.project_.timeline.trackById(this.trackId_);
    if (!t) {
      const found = this.project_.timeline.findClipWithTrack(this.clipId_);
      if (found) {
        t = found.track;
        this.trackId_ = t.id;
      }
    }
    if (t) {
      this.removedClip_ = t.takeClip(this.clipId_);
    }
  }

  undo(): void {
    if (this.removedClip_) {
      const t = this.project_.timeline.trackById(this.trackId_);
      if (t) {
        t.insertClip(this.removedClip_);
      }
    }
  }
}
