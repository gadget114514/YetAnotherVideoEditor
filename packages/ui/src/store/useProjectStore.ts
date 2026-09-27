import { create } from 'zustand';
import {
  Project,
  VideoClip,
  AudioClip,
  SubtitleClip,
  type TrackType,
  type Track,
  type Clip,
  type Command,
  ClipProperty,
  TrackProperty,
  SetClipPropertyCommand,
  SetTrackPropertyCommand,
  AddTrackCommand,
  AddClipCommand,
  RemoveClipCommand,
  SplitClipCommand,
  Timebase,
} from '@yave/core';
import { ProjectSerializer, type LoadResult, type SaveOptions } from '@yave/io';

export interface ProjectState {
  project: Project;
  revision: number;

  // Command History
  executeCommand: (cmd: Command) => void;
  undo: () => void;
  redo: () => void;
  canUndo: () => boolean;
  canRedo: () => boolean;

  // Serialization
  saveProject: (opts?: SaveOptions) => string;
  loadProject: (jsonString: string) => LoadResult;
  initDefaultProject: () => void;

  // High-level operations
  addTrack: (type: TrackType, name?: string) => Track;
  removeTrack: (trackIndex: number) => boolean;
  addClipToTrack: (trackId: string, clip: Clip) => boolean;
  removeClip: (clipId: string) => boolean;
  splitClipAt: (clipId: string, frame: number) => boolean;
  setClipProperty: (clipId: string, prop: ClipProperty, value: any) => void;
  setTrackProperty: (trackId: string, prop: TrackProperty, value: any) => void;
  touch: () => void;
}

function makeInitialProject(): Project {
  const p = new Project();
  p.setName('YAVE Project');
  p.setTimebase(Timebase.Fps59_94);
  p.setCanvasSize({ width: 1920, height: 1080 });

  const tl = p.timeline;
  const v1 = tl.appendTrack('video', 'V1');
  const a1 = tl.appendTrack('audio', 'A1');
  const s1 = tl.appendTrack('subtitle', 'S1');

  // Add demo video clip
  const vc = new VideoClip();
  vc.setName('Intro Sequence');
  vc.setRange({ start: 0, duration: 300 });
  vc.setOpacity(1.0);
  v1.insertClip(vc);

  // Add demo audio clip
  const ac = new AudioClip();
  ac.setName('BGM');
  ac.setRange({ start: 0, duration: 600 });
  ac.gain = 0.8;
  a1.insertClip(ac);

  // Add demo subtitle clip
  const sc = new SubtitleClip();
  sc.setName('Subtitle 1');
  sc.setRange({ start: 30, duration: 150 });
  sc.setPlainText('YAVE へようこそ！Web 動画編集の世界へ');
  sc.setStyleOverride({ fontPointSize: 48, fillColor: '#ffffff' });
  s1.insertClip(sc);

  return p;
}

export const useProjectStore = create<ProjectState>((set, get) => ({
  project: makeInitialProject(),
  revision: 1,

  touch: () => {
    set((state) => ({ revision: state.revision + 1 }));
  },

  executeCommand: (cmd: Command) => {
    const { project } = get();
    project.undoStack.push(cmd);
    set((state) => ({ revision: state.revision + 1 }));
  },

  undo: () => {
    const { project } = get();
    project.undoStack.undo();
    set((state) => ({ revision: state.revision + 1 }));
  },

  redo: () => {
    const { project } = get();
    project.undoStack.redo();
    set((state) => ({ revision: state.revision + 1 }));
  },

  canUndo: () => {
    return get().project.undoStack.canUndo;
  },

  canRedo: () => {
    return get().project.undoStack.canRedo;
  },

  saveProject: (opts) => {
    const { project } = get();
    return ProjectSerializer.saveToString(project, opts);
  },

  loadProject: (jsonString: string) => {
    const p = new Project();
    const result = ProjectSerializer.loadFromString(p, jsonString);
    if (result.ok) {
      set({ project: p, revision: get().revision + 1 });
    }
    return result;
  },

  initDefaultProject: () => {
    set({ project: makeInitialProject(), revision: get().revision + 1 });
  },

  addTrack: (type: TrackType, name?: string) => {
    const { project, executeCommand } = get();
    const cmd = new AddTrackCommand(project, type, name);
    executeCommand(cmd);
    return cmd.createdTrack!;
  },

  removeTrack: (trackIndex: number) => {
    const { project, touch } = get();
    const ok = project.timeline.removeTrack(trackIndex);
    if (ok) touch();
    return ok;
  },

  addClipToTrack: (trackId: string, clip: Clip) => {
    const { project, executeCommand } = get();
    const cmd = new AddClipCommand(project, trackId, clip);
    executeCommand(cmd);
    return cmd.wasAdded;
  },

  removeClip: (clipId: string) => {
    const { project, executeCommand } = get();
    const cmd = new RemoveClipCommand(project, clipId);
    executeCommand(cmd);
    return cmd.wasRemoved;
  },

  splitClipAt: (clipId: string, frame: number) => {
    const { project, executeCommand } = get();
    const cmd = new SplitClipCommand(project, clipId, frame);
    executeCommand(cmd);
    return cmd.wasSplit;
  },

  setClipProperty: (clipId: string, prop: ClipProperty, value: any) => {
    const { project, executeCommand } = get();
    executeCommand(new SetClipPropertyCommand(project, clipId, prop, value));
  },

  setTrackProperty: (trackId: string, prop: TrackProperty, value: any) => {
    const { project, executeCommand } = get();
    executeCommand(new SetTrackPropertyCommand(project, trackId, prop, value));
  },
}));
