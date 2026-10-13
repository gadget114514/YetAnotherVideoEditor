import { describe, it, expect } from 'vitest';
import { PluginHost, GlitchFilterPlugin } from '../src/plugins/PluginHost.js';
import type { ISubtitleEffect, IVideoFilter, PluginManifest } from '@yave/plugin-sdk';

describe('PluginHost', () => {
  it('initializes with builtin subtitle effects and glitch video filter', () => {
    const host = new PluginHost();

    // Check builtin glitch video filter
    const glitchFilter = host.getVideoFilter('com.yave.plugin.glitch');
    expect(glitchFilter).toBeDefined();
    expect(glitchFilter?.displayName).toBe('Glitch Effect');

    // Check builtin subtitle effect (e.g. fade)
    const fadeFx = host.getSubtitleEffect('fade');
    expect(fadeFx).toBeDefined();
  });

  it('registerSubtitleEffect and getSubtitleEffect', () => {
    const host = new PluginHost();
    const customEffect: ISubtitleEffect = {
      id: 'custom.subtitle.effect',
      displayName: 'Custom Effect',
      category: 'Test',
      getParameterSchema: () => [],
      apply: () => {},
    };

    host.registerSubtitleEffect(customEffect);
    expect(host.getSubtitleEffect('custom.subtitle.effect')).toBe(customEffect);
  });

  it('registerVideoFilter and getVideoFilter', () => {
    const host = new PluginHost();
    const customFilter: IVideoFilter = {
      id: 'custom.video.filter',
      displayName: 'Custom Filter',
      getParameterSchema: () => [],
    };

    host.registerVideoFilter(customFilter);
    expect(host.getVideoFilter('custom.video.filter')).toBe(customFilter);
  });

  it('loadPlugin handles manifests and plugin modules', () => {
    const host = new PluginHost();

    const manifestFx: PluginManifest = {
      id: 'my.custom.fx',
      name: { ja: 'カスタムFX', en: 'Custom FX' },
      version: '1.0.0',
      sdkVersion: '1.0.0',
      kind: 'subtitleEffect',
      entry: 'index.js',
    };

    const mockFxInstance: ISubtitleEffect = {
      id: 'my.custom.fx',
      displayName: 'Custom FX',
      category: 'Custom',
      getParameterSchema: () => [],
      apply: () => {},
    };

    const loaded = host.loadPlugin(manifestFx, { default: mockFxInstance });
    expect(loaded).toBe(true);
    expect(host.getSubtitleEffect('my.custom.fx')).toBe(mockFxInstance);
    expect(host.loadedPlugins).toHaveLength(1);
    expect(host.loadedPlugins[0]?.manifest.id).toBe('my.custom.fx');
  });

  it('loadPlugin returns false for invalid manifest', () => {
    const host = new PluginHost();
    const invalidManifest = {} as PluginManifest;
    const loaded = host.loadPlugin(invalidManifest, {});
    expect(loaded).toBe(false);
  });
});
