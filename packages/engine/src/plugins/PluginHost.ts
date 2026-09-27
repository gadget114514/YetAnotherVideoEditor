import {
  type PluginManifest,
  type ISubtitleEffect,
  type IVideoFilter,
  type ParameterSchemaItem,
} from '@yave/plugin-sdk';
import { builtinSubtitleEffects } from '../subtitle/SubtitleEffects.js';

export interface LoadedPlugin {
  manifest: PluginManifest;
  instance: ISubtitleEffect | IVideoFilter | any;
}

/**
 * Sample Glitch Video Filter Plugin implementing IVideoFilter.
 */
export class GlitchFilterPlugin implements IVideoFilter {
  readonly id = 'com.yave.plugin.glitch';
  readonly displayName = 'Glitch Effect';

  getParameterSchema(): readonly ParameterSchemaItem[] {
    return [
      { key: 'intensity', displayNameKey: 'Intensity', type: 'double', defaultValue: 0.5, minValue: 0, maxValue: 1 },
      { key: 'colorShift', displayNameKey: 'Color Shift', type: 'double', defaultValue: 10, minValue: 0, maxValue: 50 },
    ];
  }

  applyGlslFragment(): string {
    return `
      uniform float uIntensity;
      uniform float uColorShift;
      vec4 applyGlitch(vec4 src, vec2 uv) {
        float r = texture(uTex, uv + vec2(uColorShift * 0.002 * uIntensity, 0.0)).r;
        float g = src.g;
        float b = texture(uTex, uv - vec2(uColorShift * 0.002 * uIntensity, 0.0)).b;
        return vec4(r, g, b, src.a);
      }
    `;
  }
}

export class PluginHost {
  private plugins_: Map<string, LoadedPlugin> = new Map();
  private subtitleEffects_: Map<string, ISubtitleEffect> = new Map();
  private videoFilters_: Map<string, IVideoFilter> = new Map();

  constructor() {
    // Register builtin subtitle effects
    for (const [id, fx] of Object.entries(builtinSubtitleEffects)) {
      this.subtitleEffects_.set(id, fx);
    }

    // Register builtin sample plugin
    this.registerVideoFilter(new GlitchFilterPlugin());
  }

  get loadedPlugins(): LoadedPlugin[] {
    return Array.from(this.plugins_.values());
  }

  registerSubtitleEffect(effect: ISubtitleEffect): void {
    this.subtitleEffects_.set(effect.id, effect);
  }

  registerVideoFilter(filter: IVideoFilter): void {
    this.videoFilters_.set(filter.id, filter);
  }

  getSubtitleEffect(id: string): ISubtitleEffect | null {
    return this.subtitleEffects_.get(id) ?? null;
  }

  getVideoFilter(id: string): IVideoFilter | null {
    return this.videoFilters_.get(id) ?? null;
  }

  loadPlugin(manifest: PluginManifest, pluginModule: any): boolean {
    if (!manifest.id) return false;

    const instance = typeof pluginModule.default === 'function'
      ? new pluginModule.default()
      : pluginModule.default || pluginModule;

    if (manifest.kind === 'subtitleEffect') {
      this.registerSubtitleEffect(instance as ISubtitleEffect);
    } else if (manifest.kind === 'videoFilter') {
      this.registerVideoFilter(instance as IVideoFilter);
    }

    this.plugins_.set(manifest.id, {
      manifest,
      instance,
    });

    return true;
  }
}
