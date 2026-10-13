import { describe, it, expect } from 'vitest';
import { Project } from '@yave/core';
import { AiOrchestrator, MockGenerationProvider, LocalWhisperProvider } from '../src/ai/AiOrchestrator.js';
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

  it('loads audio, generates SRT via AI, and imports as a subtitle track', async () => {
    const project = new Project();
    const orchestrator = new AiOrchestrator(new MockGenerationProvider());

    const result = await orchestrator.importAudioAndTranscribe({
      project,
      audioName: 'interview_sample.wav',
      durationFrames: 240, // 4 seconds at 60fps
      prompt: 'こんにちは、YAVE動画編集です。\nAI音声認識で字幕トラックを作成します。',
      language: 'ja',
    });

    expect(result).toBeDefined();
    expect(result.audioAssetId).toBeDefined();
    expect(result.audioTrack).toBeDefined();
    expect(result.audioTrack.type).toBe('audio');
    expect(result.audioClip).toBeDefined();
    expect(result.audioClip.name).toBe('interview_sample.wav');
    expect(result.audioClip.range.duration).toBe(240);

    // Subtitle track verification
    expect(result.subtitleTrack).toBeDefined();
    expect(result.subtitleTrack.type).toBe('subtitle');
    expect(result.subtitleClips.length).toBeGreaterThanOrEqual(1);

    // SRT string generated
    expect(result.srtContent).toContain('-->');
    expect(result.srtContent).toContain('こんにちは');

    // Subtitle clips are placed on the subtitle track
    const firstSub = result.subtitleTrack.clipAt(result.subtitleClips[0]!.range.start);
    expect(firstSub).not.toBeNull();
    expect(firstSub!.type).toBe('subtitle');
    expect((firstSub as any).plainText()).toContain('こんにちは');
  });

  it('runs LocalWhisperProvider to convert audio to SRT', async () => {
    const project = new Project();
    const orchestrator = new AiOrchestrator();

    // Create 1 second 440Hz sine wave PCM WAV audio buffer
    const sampleRate = 16000;
    const numSamples = sampleRate * 1;
    const dataSize = numSamples * 2;
    const buffer = new ArrayBuffer(44 + dataSize);
    const view = new DataView(buffer);

    // RIFF header
    view.setUint32(0, 0x52494646, false); // "RIFF"
    view.setUint32(4, 36 + dataSize, true);
    view.setUint32(8, 0x57415645, false); // "WAVE"
    view.setUint32(12, 0x666d7420, false); // "fmt "
    view.setUint32(16, 16, true); // Subchunk1Size
    view.setUint16(20, 1, true); // AudioFormat PCM
    view.setUint16(22, 1, true); // NumChannels 1
    view.setUint32(24, sampleRate, true); // SampleRate
    view.setUint32(28, sampleRate * 2, true); // ByteRate
    view.setUint16(32, 2, true); // BlockAlign
    view.setUint16(34, 16, true); // BitsPerSample
    view.setUint32(36, 0x64617461, false); // "data"
    view.setUint32(40, dataSize, true);

    for (let i = 0; i < numSamples; i++) {
      const sample = Math.sin((2 * Math.PI * 440 * i) / sampleRate);
      const val = Math.floor(sample * 32767);
      view.setInt16(44 + i * 2, val, true);
    }

    // Check if local whisper CLI is available in python
    let whisperInstalled = false;
    try {
      const { execSync } = await import('node:child_process');
      execSync('python3 -c "import subprocess; subprocess.run([\'whisper\', \'--help\'], check=True)"', { stdio: 'ignore' });
      whisperInstalled = true;
    } catch {
      whisperInstalled = false;
    }

    if (whisperInstalled) {
      const result = await orchestrator.importAudioAndTranscribe({
        project,
        audioName: 'sine.wav',
        audioData: buffer,
        durationFrames: 60,
        providerId: 'local-whisper',
        language: 'ja',
      });

      expect(result).toBeDefined();
      expect(result.audioAssetId).toBeDefined();
      expect(result.subtitleTrack).toBeDefined();
      expect(result.srtContent).toBeDefined();
      expect(typeof result.srtContent).toBe('string');
    } else {
      await expect(
        orchestrator.importAudioAndTranscribe({
          project,
          audioName: 'sine.wav',
          audioData: buffer,
          durationFrames: 60,
          providerId: 'local-whisper',
          language: 'ja',
        })
      ).rejects.toThrow();
    }
  }, 30000);
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
