import { describe, it, expect, beforeEach } from 'vitest';
import { useProjectStore } from '../src/store/useProjectStore.js';
import { usePlaybackStore } from '../src/store/usePlaybackStore.js';
import { useSelectionStore } from '../src/store/useSelectionStore.js';
import { VideoClip, TrackType } from '@yave/core';

describe('UI Zustand Stores', () => {
  beforeEach(() => {
    useProjectStore.getState().initDefaultProject();
    usePlaybackStore.getState().pause();
    usePlaybackStore.getState().setCurrentFrame(0);
    useSelectionStore.getState().selectClip(null);
    useSelectionStore.getState().selectTrack(null);
  });

  describe('useProjectStore', () => {
    it('initializes default project with 3 tracks', () => {
      const state = useProjectStore.getState();
      expect(state.project).toBeDefined();
      expect(state.project.timeline.trackCount).toBe(3);
      expect(state.canUndo()).toBe(false);
      expect(state.canRedo()).toBe(false);
    });

    it('adds a new track and supports undo/redo', () => {
      const store = useProjectStore.getState();
      const track = store.addTrack('audio', 'BGM2');
      expect(track).toBeDefined();
      expect(useProjectStore.getState().project.timeline.trackCount).toBe(4);
      expect(useProjectStore.getState().canUndo()).toBe(true);

      useProjectStore.getState().undo();
      expect(useProjectStore.getState().project.timeline.trackCount).toBe(3);
      expect(useProjectStore.getState().canRedo()).toBe(true);

      useProjectStore.getState().redo();
      expect(useProjectStore.getState().project.timeline.trackCount).toBe(4);
    });

    it('adds and removes clip from track with command stack', () => {
      const store = useProjectStore.getState();
      const videoTrack = store.project.timeline.tracksOfType('video')[0]!;
      const initialCount = videoTrack.clipCount;
      const clip = new VideoClip();
      clip.setRange({ start: 1000, duration: 300 });

      const added = store.addClipToTrack(videoTrack.id, clip);
      expect(added).toBe(true);
      expect(videoTrack.clipCount).toBe(initialCount + 1);
      expect(useProjectStore.getState().canUndo()).toBe(true);

      const removed = useProjectStore.getState().removeClip(clip.id);
      expect(removed).toBe(true);
      expect(videoTrack.clipCount).toBe(initialCount);

      useProjectStore.getState().undo();
      expect(videoTrack.clipCount).toBe(initialCount + 1);
    });

    it('splits a clip at playhead frame', () => {
      const store = useProjectStore.getState();
      const videoTrack = store.project.timeline.tracksOfType('video')[0]!;
      const clip = new VideoClip();
      clip.setRange({ start: 2000, duration: 600 });
      store.addClipToTrack(videoTrack.id, clip);
      const countBeforeSplit = videoTrack.clipCount;

      const splitOk = useProjectStore.getState().splitClipAt(clip.id, 2200);
      expect(splitOk).toBe(true);
      expect(videoTrack.clipCount).toBe(countBeforeSplit + 1);

      useProjectStore.getState().undo();
      expect(videoTrack.clipCount).toBe(countBeforeSplit);
    });
  });

  describe('usePlaybackStore', () => {
    it('controls playhead and playback state', () => {
      const store = usePlaybackStore.getState();
      expect(store.isPlaying).toBe(false);
      expect(store.currentFrame).toBe(0);

      store.setCurrentFrame(120);
      expect(usePlaybackStore.getState().currentFrame).toBe(120);

      store.stepFrames(10);
      expect(usePlaybackStore.getState().currentFrame).toBe(130);

      store.stepFrames(-5);
      expect(usePlaybackStore.getState().currentFrame).toBe(125);

      store.togglePlay();
      expect(usePlaybackStore.getState().isPlaying).toBe(true);

      store.pause();
      expect(usePlaybackStore.getState().isPlaying).toBe(false);
    });
  });

  describe('useSelectionStore', () => {
    it('manages clip and track selection', () => {
      const store = useSelectionStore.getState();
      expect(store.selectedClipId).toBeNull();
      expect(store.selectedTrackId).toBeNull();

      store.selectClip('clip-123');
      expect(useSelectionStore.getState().selectedClipId).toBe('clip-123');

      store.selectTrack('track-456');
      expect(useSelectionStore.getState().selectedTrackId).toBe('track-456');

      store.clearSelection();
      expect(useSelectionStore.getState().selectedClipId).toBeNull();
      expect(useSelectionStore.getState().selectedTrackId).toBeNull();
    });
  });
});
