import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { Project } from '@yave/core';
import { PlaybackController } from '../src/playback/PlaybackController.js';

describe('PlaybackController', () => {
  let project: Project;
  let controller: PlaybackController;

  beforeEach(() => {
    if (typeof globalThis.requestAnimationFrame === 'undefined') {
      (globalThis as any).requestAnimationFrame = (cb: FrameRequestCallback) =>
        setTimeout(cb, 16) as unknown as number;
    }
    if (typeof globalThis.cancelAnimationFrame === 'undefined') {
      (globalThis as any).cancelAnimationFrame = (id: number) => clearTimeout(id as any);
    }

    project = new Project('Test Project');
    controller = new PlaybackController(project);
  });

  afterEach(() => {
    controller.dispose();
  });

  it('initial state', () => {
    expect(controller.isPlaying).toBe(false);
    expect(controller.currentFrame).toBe(0);
    expect(controller.frameCache).toBeDefined();
    expect(controller.audioEngine).toBeDefined();
  });

  it('seek and stepFrames updates current frame and notifies listeners', () => {
    const listener = vi.fn();
    const unsubscribe = controller.onFrame(listener);

    controller.seek(30);
    expect(controller.currentFrame).toBe(30);
    expect(listener).toHaveBeenCalledWith(30);

    controller.stepFrames(10);
    expect(controller.currentFrame).toBe(40);
    expect(listener).toHaveBeenCalledWith(40);

    // Negative frame seeking clamps to 0
    controller.seek(-15);
    expect(controller.currentFrame).toBe(0);

    // Unsubscribe listener
    unsubscribe();
    controller.seek(100);
    expect(listener).not.toHaveBeenCalledWith(100);
  });

  it('play, pause, and togglePlay controls playback state', async () => {
    expect(controller.isPlaying).toBe(false);

    await controller.play();
    expect(controller.isPlaying).toBe(true);

    controller.pause();
    expect(controller.isPlaying).toBe(false);

    controller.togglePlay();
    expect(controller.isPlaying).toBe(true);

    controller.togglePlay();
    expect(controller.isPlaying).toBe(false);
  });

  it('setProject updates active project and audio graph', () => {
    const newProject = new Project('New Project');
    controller.setProject(newProject);
    expect(controller.currentFrame).toBe(0);
  });

  it('dispose cleans up controller', () => {
    controller.dispose();
    expect(controller.isPlaying).toBe(false);
  });
});
