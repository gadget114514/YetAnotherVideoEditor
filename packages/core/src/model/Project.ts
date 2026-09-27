import { createUuid, type Uuid } from '../id/Uuid.js';
import { Timeline } from './Timeline.js';
import { CommandStack } from '../commands/CommandStack.js';
import type { TimeRange } from '../time/TimeRange.js';
import { AssetLibrary } from './AssetLibrary.js';
import type { Rational } from '../time/Rational.js';
import { Timebase } from '../time/Rational.js';

export interface MediaFolder {
  id: Uuid;
  name: string;
  parentId?: Uuid;
}

export interface MediaFolderTree {
  folders: MediaFolder[];
}

export interface SubtitleStylePreset {
  id: string;
  name: string;
  style: Record<string, any>;
}

export class Project {
  readonly id: Uuid = createUuid();
  name: string = 'Untitled';
  private timeline_: Timeline = new Timeline();
  private commandStack_: CommandStack = new CommandStack();
  private assets_: AssetLibrary = new AssetLibrary();
  private mediaFolders_: MediaFolderTree = { folders: [] };
  private subtitleStylePresets_: SubtitleStylePreset[] = [
    {
      id: 'default',
      name: 'Default',
      style: { fontFamily: 'Noto Sans JP', fontPointSize: 48.0 },
    },
  ];

  timebase: Rational = Timebase.Fps59_94;
  canvasSize: { width: number; height: number } = { width: 1920, height: 1080 };
  sampleRate: number = 48000;
  channels: number = 2;
  duration: number = 108000;
  playhead: number = 0;
  workRange: TimeRange = { start: 0, duration: 108000 };
  colorSpace: string = 'bt709';
  storyBible: Record<string, any> = {};
  unknownFields: Record<string, any> = {};

  get timeline(): Timeline {
    return this.timeline_;
  }
  timelineRef(): Timeline {
    return this.timeline_;
  }

  get commandStack(): CommandStack {
    return this.commandStack_;
  }

  get undoStack(): CommandStack {
    return this.commandStack_;
  }
  undoStackRef(): CommandStack {
    return this.commandStack_;
  }

  get assets(): AssetLibrary {
    return this.assets_;
  }
  assetsRef(): AssetLibrary {
    return this.assets_;
  }

  setName(n: string): void {
    this.name = n;
  }

  setTimebase(tb: Rational): void {
    this.timebase = tb;
    this.timeline_.timebase = tb;
  }

  setCanvasSize(size: { width: number; height: number }): void {
    this.canvasSize = { ...size };
    this.timeline_.canvasSize = { ...size };
  }

  get mediaFolders(): MediaFolderTree {
    return this.mediaFolders_;
  }
  setMediaFolders(tree: MediaFolderTree): void {
    this.mediaFolders_ = {
      folders: tree.folders.map((f) => ({ ...f })),
    };
  }

  get subtitleStylePresets(): readonly SubtitleStylePreset[] {
    return this.subtitleStylePresets_;
  }
  setSubtitleStylePresets(presets: SubtitleStylePreset[]): void {
    this.subtitleStylePresets_ = presets.map((p) => ({ ...p, style: { ...p.style } }));
  }
}
