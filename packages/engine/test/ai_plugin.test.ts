import { describe, it, expect } from 'vitest';
import { Project } from '@yave/core';
import { AiOrchestrator, MockGenerationProvider } from '../src/ai/AiOrchestrator.js';
import { PluginHost, GlitchFilterPlugin } from '../src/plugins/PluginHost.js';
import type { PluginManifest } from '@yave/plugin-sdk';

describe('Milestone M6: AI Orchestrator', () => {
  it('submits AI task, creates placeholder, and commits to timeline upon completion', async () => {
    const project = new Project();
    const track = project.timeline.appendTrack('video');

    const orchestrator = new AiOrchestrator(new MockGenerationProvider());

    const task = await orchestrator.submitTask(
      {
        kind: 'video',
        prompt: 'Futuristic neon cyberpunk city flying cars',
        durationFrames: 120,
      },
      project,
      track.id,
      0
    );

    expect(task).toBeDefined();
    expect(track.clipCount).toBe(1);

    // Wait for mock generation to finish
    await new Promise((r) => setTimeout(r, 350));

    expect(task.status).toBe('committed');
    expect(task.progress).toBe(1.0);
    expect(task.outputAssetId).toBeDefined();

    // Placeholder has been replaced with VideoClip
    const finalClip = track.clipAt(0);
    expect(finalClip).not.toBeNull();
    expect(finalClip!.type).toBe('video');
    expect(finalClip!.range.duration).toBe(120);
  });
});

describe('Milestone M7: Plugin System', () => {
  it('registers builtin plugins and loads dynamic plugin manifests', () => {
    const host = new PluginHost();

    // Check built-in plugin
    const glitch = host.getVideoFilter('com.yave.plugin.glitch');
    expect(glitch).not.toBeNull();
    expect(glitch!.displayName).toBe('Glitch Effect');
    expect(glitch!.applyGlslFragment!()).toContain('applyGlitch');

    // Load custom dynamic plugin
    const customManifest: PluginManifest = {
      id: 'custom.invert.filter',
      name: { ja: '反転', en: 'Invert' },
      version: '1.0.0',
      sdkVersion: '2.0.0',
      kind: 'videoFilter',
      entry: 'index.js',
    };

    const customModule = {
      default: class CustomInvertFilter {
        readonly id = 'custom.invert.filter';
        readonly displayName = 'Invert Filter';
        getParameterSchema() {
          return [];
        }
      },
    };

    const loaded = host.loadPlugin(customManifest, customModule);
    expect(loaded).toBe(true);

    const filter = host.getVideoFilter('custom.invert.filter');
    expect(filter).not.toBeNull();
    expect(filter!.displayName).toBe('Invert Filter');
  });
});
