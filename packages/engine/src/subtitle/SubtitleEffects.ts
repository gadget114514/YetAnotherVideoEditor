import {
  type ISubtitleEffect,
  type SubtitleEffectFrame,
  type SubtitleTimeInfo,
  type ParameterSchemaItem,
} from '@yave/plugin-sdk';

/**
 * Builtin Fade In / Fade Out effect.
 */
export class FadeEffect implements ISubtitleEffect {
  readonly id = 'yave.fade';
  readonly displayName = 'Fade';
  readonly category = 'Motion';

  getParameterSchema(): readonly ParameterSchemaItem[] {
    return [
      { key: 'inDuration', displayNameKey: 'Fade In (s)', type: 'double', defaultValue: 0.3, minValue: 0, maxValue: 5 },
      { key: 'outDuration', displayNameKey: 'Fade Out (s)', type: 'double', defaultValue: 0.3, minValue: 0, maxValue: 5 },
    ];
  }

  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void {
    const inDur = params.inDuration ?? 0.3;
    const outDur = params.outDuration ?? 0.3;

    let alpha = 1.0;
    if (inDur > 0 && time.secondsFromIn < inDur) {
      alpha = Math.min(alpha, time.secondsFromIn / inDur);
    }
    if (outDur > 0 && time.secondsToOut < outDur) {
      alpha = Math.min(alpha, time.secondsToOut / outDur);
    }

    frame.blockOpacity *= Math.max(0, Math.min(1, alpha));
  }
}

/**
 * Builtin Typewriter character-by-character reveal effect.
 */
export class TypewriterEffect implements ISubtitleEffect {
  readonly id = 'yave.typewriter';
  readonly displayName = 'Typewriter';
  readonly category = 'Text';

  getParameterSchema(): readonly ParameterSchemaItem[] {
    return [
      { key: 'charsPerSecond', displayNameKey: 'Speed (chars/s)', type: 'double', defaultValue: 15, minValue: 1, maxValue: 100 },
      { key: 'startDelay', displayNameKey: 'Start Delay (s)', type: 'double', defaultValue: 0.1, minValue: 0, maxValue: 5 },
    ];
  }

  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void {
    const cps = params.charsPerSecond ?? 15;
    const delay = params.startDelay ?? 0.1;

    const elapsed = Math.max(0, time.secondsFromIn - delay);
    const visibleCount = Math.floor(elapsed * cps + 1e-6);

    for (let i = 0; i < frame.glyphs.length; i++) {
      if (i >= visibleCount) {
        frame.glyphs[i]!.visible = false;
      }
    }
  }
}

/**
 * Builtin Slide In motion effect.
 */
export class SlideInEffect implements ISubtitleEffect {
  readonly id = 'yave.slidein';
  readonly displayName = 'Slide In';
  readonly category = 'Motion';

  getParameterSchema(): readonly ParameterSchemaItem[] {
    return [
      { key: 'duration', displayNameKey: 'Duration (s)', type: 'double', defaultValue: 0.4, minValue: 0.1, maxValue: 3 },
      { key: 'distance', displayNameKey: 'Distance (px)', type: 'int', defaultValue: 100, minValue: 10, maxValue: 1000 },
      { key: 'direction', displayNameKey: 'Direction', type: 'enum', defaultValue: 'bottom', enumChoices: ['bottom', 'top', 'left', 'right'] },
    ];
  }

  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void {
    const duration = params.duration ?? 0.4;
    const distance = params.distance ?? 100;
    const direction = params.direction ?? 'bottom';

    if (time.secondsFromIn >= duration) {
      return;
    }

    const t = Math.max(0, Math.min(1, time.secondsFromIn / duration));
    // Ease-out cubic
    const progress = 1 - Math.pow(1 - t, 3);
    const offset = distance * (1 - progress);

    let dx = 0;
    let dy = 0;
    if (direction === 'bottom') dy = offset;
    else if (direction === 'top') dy = -offset;
    else if (direction === 'left') dx = -offset;
    else if (direction === 'right') dx = offset;

    // Apply translation to blockTransform [1,0,0,0, 0,1,0,0, 0,0,1,0, dx,dy,0,1]
    frame.blockTransform[12] = (frame.blockTransform[12] ?? 0) + dx;
    frame.blockTransform[13] = (frame.blockTransform[13] ?? 0) + dy;
  }
}

/**
 * Builtin Wave sine-wave displacement effect.
 */
export class WaveEffect implements ISubtitleEffect {
  readonly id = 'yave.wave';
  readonly displayName = 'Wave';
  readonly category = 'Animation';

  getParameterSchema(): readonly ParameterSchemaItem[] {
    return [
      { key: 'amplitude', displayNameKey: 'Amplitude (px)', type: 'double', defaultValue: 12, minValue: 1, maxValue: 100 },
      { key: 'frequency', displayNameKey: 'Frequency', type: 'double', defaultValue: 0.4, minValue: 0.05, maxValue: 2 },
      { key: 'speed', displayNameKey: 'Speed', type: 'double', defaultValue: 4, minValue: 0.5, maxValue: 20 },
    ];
  }

  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void {
    const amp = params.amplitude ?? 12;
    const freq = params.frequency ?? 0.4;
    const speed = params.speed ?? 4;

    const t = time.secondsFromIn * speed;

    for (let i = 0; i < frame.glyphs.length; i++) {
      const g = frame.glyphs[i]!;
      const dy = Math.sin(t + i * freq) * amp;
      g.transform[13] = (g.transform[13] ?? 0) + dy;
    }
  }
}

export const builtinSubtitleEffects: Record<string, ISubtitleEffect> = {
  'yave.fade': new FadeEffect(),
  'yave.typewriter': new TypewriterEffect(),
  'yave.slidein': new SlideInEffect(),
  'yave.wave': new WaveEffect(),
};
