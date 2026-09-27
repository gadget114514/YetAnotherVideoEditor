import { describe, it, expect } from 'vitest';
import { SubtitleClip } from '@yave/core';
import { SubtitleRenderer } from '../src/subtitle/SubtitleRenderer.js';
import {
  FadeEffect,
  TypewriterEffect,
  SlideInEffect,
  WaveEffect,
} from '../src/subtitle/SubtitleEffects.js';

describe('SubtitleEngine', () => {
  it('FadeEffect attenuates opacity at in and out points', () => {
    const fade = new FadeEffect();
    const frame: any = { blockOpacity: 1.0, glyphs: [] };

    // At In point (t = 0.0s, inDuration = 0.5s)
    fade.apply(frame, { progress: 0.0, secondsFromIn: 0.0, secondsToOut: 2.0, clipDurationSec: 2.0 }, { inDuration: 0.5, outDuration: 0.5 });
    expect(frame.blockOpacity).toBe(0.0);

    // Halfway through fade-in (t = 0.25s)
    frame.blockOpacity = 1.0;
    fade.apply(frame, { progress: 0.125, secondsFromIn: 0.25, secondsToOut: 1.75, clipDurationSec: 2.0 }, { inDuration: 0.5, outDuration: 0.5 });
    expect(Math.abs(frame.blockOpacity - 0.5)).toBeLessThan(1e-6);

    // In the middle
    frame.blockOpacity = 1.0;
    fade.apply(frame, { progress: 0.5, secondsFromIn: 1.0, secondsToOut: 1.0, clipDurationSec: 2.0 }, { inDuration: 0.5, outDuration: 0.5 });
    expect(frame.blockOpacity).toBe(1.0);

    // Near Out point (0.1s left)
    frame.blockOpacity = 1.0;
    fade.apply(frame, { progress: 0.95, secondsFromIn: 1.9, secondsToOut: 0.1, clipDurationSec: 2.0 }, { inDuration: 0.5, outDuration: 0.5 });
    expect(Math.abs(frame.blockOpacity - 0.2)).toBeLessThan(1e-6);
  });

  it('TypewriterEffect hides glyphs progressively', () => {
    const tw = new TypewriterEffect();
    const glyphs = [
      { visible: true },
      { visible: true },
      { visible: true },
      { visible: true },
      { visible: true },
    ];
    const frame: any = { glyphs };

    // At t = 0 (before delay)
    tw.apply(frame, { progress: 0, secondsFromIn: 0.0, secondsToOut: 1, clipDurationSec: 1 }, { charsPerSecond: 10, startDelay: 0.1 });
    expect(glyphs.every((g) => !g.visible)).toBe(true);

    // At t = 0.3s (delay 0.1s -> elapsed 0.2s * 10 = 2 characters visible)
    for (const g of glyphs) g.visible = true;
    tw.apply(frame, { progress: 0.3, secondsFromIn: 0.3, secondsToOut: 0.7, clipDurationSec: 1 }, { charsPerSecond: 10, startDelay: 0.1 });
    expect(glyphs[0]!.visible).toBe(true);
    expect(glyphs[1]!.visible).toBe(true);
    expect(glyphs[2]!.visible).toBe(false);
    expect(glyphs[3]!.visible).toBe(false);
    expect(glyphs[4]!.visible).toBe(false);
  });

  it('SlideInEffect translates block transform', () => {
    const slide = new SlideInEffect();
    const frame: any = { blockTransform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] };

    // At t = 0 (enters from bottom)
    slide.apply(frame, { progress: 0, secondsFromIn: 0, secondsToOut: 1, clipDurationSec: 1 }, { duration: 0.5, distance: 200, direction: 'bottom' });
    expect(frame.blockTransform[13]).toBe(200);

    // After duration completes
    frame.blockTransform = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1];
    slide.apply(frame, { progress: 0.6, secondsFromIn: 0.6, secondsToOut: 0.4, clipDurationSec: 1 }, { duration: 0.5, distance: 200, direction: 'bottom' });
    expect(frame.blockTransform[13]).toBe(0);
  });

  it('WaveEffect applies sinusoidal displacement across glyphs', () => {
    const wave = new WaveEffect();
    const glyphs = [
      { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] },
      { transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] },
    ];
    const frame: any = { glyphs };

    wave.apply(frame, { progress: 0.2, secondsFromIn: 0.5, secondsToOut: 1.5, clipDurationSec: 2 }, { amplitude: 20, frequency: 1.0, speed: 2.0 });

    // Displacement should be non-zero and vary between glyph 0 and glyph 1
    const dy0 = glyphs[0]!.transform[13]!;
    const dy1 = glyphs[1]!.transform[13]!;
    expect(Math.abs(dy0)).toBeGreaterThan(0);
    expect(Math.abs(dy1)).toBeGreaterThan(0);
    expect(dy0).not.toBe(dy1);
  });
});
