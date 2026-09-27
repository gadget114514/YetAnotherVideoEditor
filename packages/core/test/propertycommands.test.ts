import { describe, it, expect } from 'vitest';
import {
  Project,
  AudioClip,
  VideoClip,
  BlendMode,
  ClipProperty,
  TrackProperty,
  SetClipPropertyCommand,
  SetTrackPropertyCommand,
} from '../src/index.js';

describe('TestPropertyCommands', () => {
  it('clipGainPanUndoRedo', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('audio');
    const clip = new AudioClip();
    clip.setRange({ start: 0, duration: 100 });
    expect(t.insertClip(clip)).toBe(true);
    const id = clip.id;

    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Gain, 0.6),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Pan, -0.3),
    );
    expect(clip.gain).toBe(0.6);
    expect(clip.pan).toBe(-0.3);

    project.undoStack.undo();
    expect(clip.pan).toBe(0.0);
    project.undoStack.undo();
    expect(clip.gain).toBe(1.0);

    project.undoStack.redo();
    expect(clip.gain).toBe(0.6);
  });

  it('clipOpacityBlendUndoRedo', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');
    const clip = new VideoClip();
    clip.setRange({ start: 0, duration: 100 });
    expect(t.insertClip(clip)).toBe(true);
    const id = clip.id;

    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Opacity, 0.4),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.BlendMode, BlendMode.Add),
    );
    expect(clip.opacity).toBe(0.4);
    expect(clip.blendMode).toBe(BlendMode.Add);

    project.undoStack.undo();
    expect(clip.blendMode).toBe(BlendMode.Normal);
    project.undoStack.undo();
    expect(clip.opacity).toBe(1.0);
  });

  it('clipRangeNameFade', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');
    const clip = new VideoClip();
    clip.setRange({ start: 0, duration: 100 });
    expect(t.insertClip(clip)).toBe(true);
    const id = clip.id;

    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Name, 'hero'),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Start, 200),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.Duration, 50),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.FadeIn, 10),
    );
    project.undoStack.push(
      new SetClipPropertyCommand(project, id, ClipProperty.FadeOut, 5),
    );

    expect(clip.name).toBe('hero');
    expect(clip.range.start).toBe(200);
    expect(clip.range.duration).toBe(50);
    expect(clip.fadeInFrames()).toBe(10);
    expect(clip.fadeOutFrames()).toBe(5);

    // Start 変更後もトラックのソート順が保たれている
    for (let i = 0; i < t.clips.length - 1; i++) {
      expect(t.clips[i]!.range.start).toBeLessThanOrEqual(t.clips[i + 1]!.range.start);
    }

    project.undoStack.undo();
    expect(clip.fadeOutFrames()).toBe(0);
    project.undoStack.undo();
    expect(clip.fadeInFrames()).toBe(0);
    project.undoStack.undo();
    expect(clip.range.duration).toBe(100);
    project.undoStack.undo();
    expect(clip.range.start).toBe(0);
    project.undoStack.undo();
    expect(clip.name).toBe('');
  });

  it('trackGainMutedUndoRedo', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('audio');
    const id = t.id;

    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.Gain, 0.5),
    );
    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.Muted, true),
    );
    expect(t.gain).toBe(0.5);
    expect(t.isMuted()).toBe(true);

    project.undoStack.undo();
    expect(t.isMuted()).toBe(false);
    project.undoStack.undo();
    expect(t.gain).toBe(1.0);
  });

  it('trackOpacityBlendUndoRedo', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');
    const id = t.id;

    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.Opacity, 0.3),
    );
    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.BlendMode, BlendMode.Screen),
    );
    expect(t.opacity).toBe(0.3);
    expect(t.blendMode).toBe(BlendMode.Screen);

    project.undoStack.undo();
    expect(t.blendMode).toBe(BlendMode.Normal);
    project.undoStack.undo();
    expect(t.opacity).toBe(1.0);
  });

  it('trackNameHeightVisible', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');
    const id = t.id;
    const originalName = t.name;

    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.Name, 'bg'),
    );
    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.UiHeight, 120),
    );
    project.undoStack.push(
      new SetTrackPropertyCommand(project, id, TrackProperty.Visible, false),
    );

    expect(t.name).toBe('bg');
    expect(t.uiHeight).toBe(120);
    expect(t.isVisible()).toBe(false);

    project.undoStack.undo();
    expect(t.isVisible()).toBe(true);
    project.undoStack.undo();
    expect(t.uiHeight).toBe(64);
    project.undoStack.undo();
    expect(t.name).toBe(originalName);
  });
});
