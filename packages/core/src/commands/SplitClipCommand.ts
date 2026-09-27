import type { Command } from './Command.js';
import type { Project } from '../model/Project.js';
import type { Uuid } from '../id/Uuid.js';
import type { TimeRange } from '../time/TimeRange.js';
import type { Clip } from '../model/Clip.js';

export class SplitClipCommand implements Command {
  readonly name: string = 'Split clip';
  private project_: Project;
  private trackId_: Uuid;
  private clipId_: Uuid;
  private splitFrame_: number;
  private original_: Clip | null = null;
  private left_: Clip | null = null;
  private right_: Clip | null = null;

  constructor(project: Project, trackIdOrClipId: Uuid | string, clipIdOrFrame: Uuid | string | number, splitFrame?: number) {
    this.project_ = project;
    if (typeof clipIdOrFrame === 'number') {
      this.clipId_ = trackIdOrClipId as Uuid;
      this.splitFrame_ = clipIdOrFrame;
      const found = project.timeline.findClipWithTrack(this.clipId_);
      this.trackId_ = found ? found.track.id : ('' as Uuid);
    } else {
      this.trackId_ = trackIdOrClipId as Uuid;
      this.clipId_ = clipIdOrFrame as Uuid;
      this.splitFrame_ = splitFrame!;
    }
  }

  get wasSplit(): boolean {
    return this.left_ !== null && this.right_ !== null;
  }

  get left(): Clip | null {
    return this.left_;
  }

  get right(): Clip | null {
    return this.right_;
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
    if (!t) return;

    const original = t.takeClip(this.clipId_);
    if (!original) return;

    if (!this.left_ || !this.right_) {
      const r = original.range;
      const offset = this.splitFrame_ - r.start;
      if (offset <= 0 || offset >= r.duration) {
        t.insertClip(original);
        return;
      }

      this.left_ = original.clone();
      this.right_ = original.clone();
      this.left_.setRange({ start: r.start, duration: offset });
      this.right_.setRange({ start: this.splitFrame_, duration: r.duration - offset });
      this.right_.shiftSourceOffset(offset);
    }

    this.original_ = original;
    t.insertClip(this.left_);
    t.insertClip(this.right_);
  }

  undo(): void {
    const t = this.project_.timeline.trackById(this.trackId_);
    if (!t || !this.original_) return;

    if (this.left_) t.removeClip(this.left_.id);
    if (this.right_) t.removeClip(this.right_.id);
    t.insertClip(this.original_);
  }
}

export class RippleDeleteCommand implements Command {
  readonly name: string = 'Ripple delete';
  private project_: Project;
  private trackIndices_: number[];
  private gone_: TimeRange;
  private removedClips_: { trackId: Uuid; clip: Clip }[] = [];

  constructor(project: Project, trackIndices: number[], gone: TimeRange) {
    this.project_ = project;
    this.trackIndices_ = trackIndices;
    this.gone_ = gone;
  }

  redo(): void {
    const tl = this.project_.timeline;
    this.removedClips_ = [];

    for (const idx of this.trackIndices_) {
      const t = tl.trackAt(idx);
      if (!t) continue;

      for (const removed of t.removeClipsIn(this.gone_)) {
        this.removedClips_.push({ trackId: t.id, clip: removed });
      }

      t.shiftClipsAfter(this.gone_.start, -this.gone_.duration);
    }
  }

  undo(): void {
    const tl = this.project_.timeline;

    for (const idx of this.trackIndices_) {
      const t = tl.trackAt(idx);
      if (!t) continue;
      t.shiftClipsAfter(this.gone_.start, this.gone_.duration);
    }

    for (const e of this.removedClips_) {
      const t = tl.trackById(e.trackId);
      if (t) {
        t.insertClip(e.clip);
      }
    }
    this.removedClips_ = [];
  }
}
