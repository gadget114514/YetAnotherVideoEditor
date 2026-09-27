import type { Command } from './Command.js';
import type { Project } from '../model/Project.js';
import { Track, type TrackType } from '../model/Track.js';
import { Timeline } from '../model/Timeline.js';

export class AddTrackCommand implements Command {
  readonly name: string = 'Add track';
  private project_: Project;
  private type_: TrackType;
  private index_: number;
  private name_: string;
  private track_: Track | null = null;

  constructor(project: Project, type: TrackType, index: number, name?: string) {
    this.project_ = project;
    this.type_ = type;
    this.index_ = index;
    const ordinal = project.timeline.tracksOfType(type).length + 1;
    this.name_ = name && name.length > 0 ? name : Timeline.defaultTrackName(type, ordinal);
  }

  redo(): void {
    if (!this.track_) {
      this.track_ = new Track(this.type_);
      this.track_.name = this.name_;
    }
    this.project_.timeline.reinsertTrack(this.index_, this.track_);
  }

  undo(): void {
    if (this.track_) {
      this.project_.timeline.takeTrackById(this.track_.id);
    }
  }
}
