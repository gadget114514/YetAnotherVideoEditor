import type { Uuid } from '../id/Uuid.js';
import type { Project } from '../model/Project.js';
import type { Command } from './Command.js';
import type { Transition } from '../model/Transition.js';

export class AddTransitionCommand implements Command {
  readonly name = 'AddTransition';
  private project_: Project;
  private trackId_: Uuid;
  private transition_: Transition;
  private replaced_: Transition | null = null;
  private error_: string = '';
  private added_: boolean = false;

  constructor(project: Project, trackId: Uuid, transition: Transition) {
    this.project_ = project;
    this.trackId_ = trackId;
    this.transition_ = transition;
  }

  get error(): string {
    return this.error_;
  }

  redo(): void {
    const track = this.project_.timeline.trackById(this.trackId_);
    if (!track) {
      this.error_ = 'Track not found';
      return;
    }

    const existing = track.transitions.find(
      (x) => x.centerFrame === this.transition_.centerFrame,
    );
    if (existing) {
      this.replaced_ = existing;
    }

    const err = { error: '' };
    this.added_ = track.addTransition(this.transition_, err);
    if (!this.added_) {
      this.error_ = err.error;
    }
  }

  undo(): void {
    if (!this.added_) return;
    const track = this.project_.timeline.trackById(this.trackId_);
    if (!track) return;

    track.removeTransition(this.transition_.id);
    if (this.replaced_) {
      track.addTransition(this.replaced_);
    }
  }
}

export class RemoveTransitionCommand implements Command {
  readonly name = 'RemoveTransition';
  private project_: Project;
  private trackId_: Uuid;
  private transitionId_: Uuid;
  private removed_: Transition | null = null;

  constructor(project: Project, trackId: Uuid, transitionId: Uuid) {
    this.project_ = project;
    this.trackId_ = trackId;
    this.transitionId_ = transitionId;
  }

  redo(): void {
    const track = this.project_.timeline.trackById(this.trackId_);
    if (!track) return;
    const t = track.transitions.find((x) => x.id === this.transitionId_);
    if (t) {
      this.removed_ = t;
      track.removeTransition(this.transitionId_);
    }
  }

  undo(): void {
    if (!this.removed_) return;
    const track = this.project_.timeline.trackById(this.trackId_);
    if (!track) return;
    track.addTransition(this.removed_);
  }
}
