import type { Uuid } from '../id/Uuid.js';
import type { Project } from '../model/Project.js';
import type { Command } from './Command.js';
import type { VideoFilterInstance } from '../model/VideoFilter.js';

export class AddFilterCommand implements Command {
  readonly name = 'AddFilter';
  private project_: Project;
  private clipId_: Uuid;
  private inst_: VideoFilterInstance;
  private index_: number;

  constructor(
    project: Project,
    clipId: Uuid,
    inst: VideoFilterInstance,
    index: number = -1,
  ) {
    this.project_ = project;
    this.clipId_ = clipId;
    this.inst_ = { ...inst, params: { ...inst.params } };
    this.index_ = index;
  }

  redo(): void {
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    if (this.index_ < 0 || this.index_ >= clip.filters.length) {
      clip.addFilter(this.inst_);
    } else {
      const cur = [...clip.filters];
      cur.splice(this.index_, 0, this.inst_);
      clip.setFilters(cur);
    }
  }

  undo(): void {
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    if (this.index_ < 0 || this.index_ >= clip.filters.length) {
      clip.removeFilter(clip.filters.length - 1);
    } else {
      clip.removeFilter(this.index_);
    }
  }
}

export class RemoveFilterCommand implements Command {
  readonly name = 'RemoveFilter';
  private project_: Project;
  private clipId_: Uuid;
  private index_: number;
  private removed_: VideoFilterInstance | null = null;

  constructor(project: Project, clipId: Uuid, index: number) {
    this.project_ = project;
    this.clipId_ = clipId;
    this.index_ = index;
  }

  redo(): void {
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    this.removed_ = clip.removeFilter(this.index_);
  }

  undo(): void {
    if (!this.removed_) return;
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    const cur = [...clip.filters];
    cur.splice(this.index_, 0, this.removed_);
    clip.setFilters(cur);
  }
}

export class ReorderFilterCommand implements Command {
  readonly name = 'ReorderFilter';
  private project_: Project;
  private clipId_: Uuid;
  private from_: number;
  private to_: number;

  constructor(project: Project, clipId: Uuid, from: number, to: number) {
    this.project_ = project;
    this.clipId_ = clipId;
    this.from_ = from;
    this.to_ = to;
  }

  redo(): void {
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    clip.moveFilter(this.from_, this.to_);
  }

  undo(): void {
    const clip = this.project_.timeline.findClip(this.clipId_);
    if (!clip) return;
    clip.moveFilter(this.to_, this.from_);
  }
}
