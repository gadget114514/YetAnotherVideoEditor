import { describe, it, expect } from 'vitest';
import { Project, AudioClip, VideoClip, createUuid, RationalUtil, type Uuid } from '@yave/core';
import { DecodedAudio, type IAudioSourceProvider } from '../src/audio/DecodedAudio.js';
import {
  AudioRenderGraphBuilder,
  type ClipSource,
} from '../src/audio/AudioRenderGraph.js';
import { mixClipBlock, kPanCenterGain } from '../src/audio/AudioRenderer.js';

function makePcm(frames: number, channels: number): DecodedAudio {
  const d = new DecodedAudio(frames, channels, 48000);
  for (let c = 0; c < channels; ++c) {
    d.channelData[c]!.fill(c + 1);
  }
  return d;
}

class MockProvider implements IAudioSourceProvider {
  id: string = '';
  decoded: DecodedAudio | null = null;

  decodeAsset(assetId: string): DecodedAudio | null {
    return assetId === this.id ? this.decoded : null;
  }
}

function addAudioClip(track: any, assetId: Uuid, start: number, duration: number): AudioClip {
  const c = new AudioClip(assetId);
  c.setRange({ start, duration });
  track.insertClip(c);
  return c;
}

describe('TestAudioGraph', () => {
  it('providerPopulatesPcm', () => {
    const project = new Project();
    const tl = project.timeline;
    const t = tl.appendTrack('audio');
    const assetId = createUuid();
    addAudioClip(t, assetId, 0, 10);

    const provider = new MockProvider();
    provider.id = assetId;
    provider.decoded = makePcm(100, 2);

    const graph = AudioRenderGraphBuilder.build(tl, project, provider);
    expect(graph.tracks.length).toBe(1);
    expect(graph.tracks[0]!.clips.length).toBe(1);

    const src = graph.tracks[0]!.clips[0]!;
    expect(src.preloadedData).not.toBeNull();
    expect(src.preloadedFrames).toBe(100);
    expect(src.channels).toBe(2);
    expect(graph.ownedAudio.length).toBe(1);
    expect(graph.ownedAudio[0]).toBe(provider.decoded);
  });

  it('noProviderLeavesSilence', () => {
    const project = new Project();
    const tl = project.timeline;
    const t = tl.appendTrack('audio');
    addAudioClip(t, createUuid(), 0, 10);

    const graph = AudioRenderGraphBuilder.build(tl, project, null);
    expect(graph.tracks.length).toBe(1);
    expect(graph.tracks[0]!.clips.length).toBe(1);
    expect(graph.tracks[0]!.clips[0]!.preloadedData).toBeNull();
    expect(graph.ownedAudio.length).toBe(0);
  });

  it('providerMismatchLeavesSilence', () => {
    const project = new Project();
    const tl = project.timeline;
    const t = tl.appendTrack('audio');
    addAudioClip(t, createUuid(), 0, 10);

    const provider = new MockProvider();
    provider.id = createUuid();
    provider.decoded = makePcm(10, 2);

    const graph = AudioRenderGraphBuilder.build(tl, project, provider);
    expect(graph.tracks[0]!.clips[0]!.preloadedData).toBeNull();
    expect(graph.ownedAudio.length).toBe(0);
  });

  it('clipSourceSampleConversion', () => {
    const project = new Project();
    project.setSampleRate(48000);
    const tl = project.timeline;
    const t = tl.appendTrack('audio');

    const clip = new AudioClip();
    clip.setRange({ start: 100, duration: 50 });
    clip.setSourceOffset(20);
    clip.setGain(0.8);
    clip.setPan(-0.5);
    clip.setFadeInFrames(10);
    clip.setFadeOutFrames(5);
    expect(t.insertClip(clip)).toBe(true);

    const graph = AudioRenderGraphBuilder.build(tl, project, null);
    const src = graph.tracks[0]!.clips[0]!;

    const tb = tl.timebase;
    const f2s = 48000.0 / RationalUtil.toDouble(tb);
    expect(src.timelineStart).toBe(Math.round(100 * f2s));
    expect(src.timelineEnd).toBe(Math.round(150 * f2s));
    expect(src.sourceOffset).toBe(Math.round(20 * f2s));
    expect(src.fadeInSamples).toBe(Math.round(10 * f2s));
    expect(src.fadeOutSamples).toBe(Math.round(5 * f2s));
    expect(Math.abs(src.gain - 0.8)).toBeLessThan(1e-6);
    expect(Math.abs(src.pan - -0.5)).toBeLessThan(1e-6);
  });

  it('audioOnlyTracksAndClips', () => {
    const project = new Project();
    const tl = project.timeline;
    const v = tl.appendTrack('video');
    tl.appendTrack('audio');
    tl.appendTrack('subtitle');

    const vc = new VideoClip();
    vc.setRange({ start: 0, duration: 10 });
    expect(v.insertClip(vc)).toBe(true);

    const a = tl.trackAt(1)!;
    addAudioClip(a, createUuid(), 0, 10);

    const graph = AudioRenderGraphBuilder.build(tl, project, null);
    expect(graph.tracks.length).toBe(1); // Audio track only
    expect(graph.tracks[0]!.clips.length).toBe(1); // Audio clip only
  });

  it('decodedAudioPlanarStability', () => {
    const d = makePcm(64, 2);
    expect(d.frames()).toBe(64);
    expect(d.channels).toBe(2);
    expect(d.planar().length).toBe(2);
    expect(d.planar()[0]![63]).toBe(1.0);
    expect(d.planar()[1]![63]).toBe(2.0);
  });

  it('mixBasicStereo', () => {
    const d = makePcm(64, 2);
    const src: ClipSource = {
      preloadedData: d.planar(),
      preloadedFrames: d.frames(),
      channels: 2,
      timelineStart: 0,
      timelineEnd: 64,
      sourceOffset: 0,
      fadeInSamples: 0,
      fadeOutSamples: 0,
      gain: 1.0,
      pan: 0.0,
    };

    const l = new Float32Array(64);
    const r = new Float32Array(64);
    const buf = [l, r];
    mixClipBlock(buf, 2, 64, 0, src);

    expect(Math.abs(l[0]! - kPanCenterGain * 1.0)).toBeLessThan(1e-6);
    expect(Math.abs(r[0]! - kPanCenterGain * 2.0)).toBeLessThan(1e-6);
    expect(Math.abs(l[63]! - kPanCenterGain * 1.0)).toBeLessThan(1e-6);
    expect(Math.abs(r[63]! - kPanCenterGain * 2.0)).toBeLessThan(1e-6);
  });

  it('mixMonoToStereo', () => {
    const d = makePcm(64, 1);
    const src: ClipSource = {
      preloadedData: d.planar(),
      preloadedFrames: d.frames(),
      channels: 1,
      timelineStart: 0,
      timelineEnd: 64,
      sourceOffset: 0,
      fadeInSamples: 0,
      fadeOutSamples: 0,
      gain: 1.0,
      pan: 0.0,
    };

    const l = new Float32Array(64);
    const r = new Float32Array(64);
    const buf = [l, r];
    mixClipBlock(buf, 2, 64, 0, src);

    expect(Math.abs(l[0]! - kPanCenterGain)).toBeLessThan(1e-6);
    expect(Math.abs(r[0]! - kPanCenterGain)).toBeLessThan(1e-6);

    const hardLeft: ClipSource = { ...src, pan: -1.0 };
    const l2 = new Float32Array(64);
    const r2 = new Float32Array(64);
    const buf2 = [l2, r2];
    mixClipBlock(buf2, 2, 64, 0, hardLeft);
    expect(Math.abs(l2[0]! - 1.0)).toBeLessThan(1e-6);
    expect(Math.abs(r2[0]!)).toBeLessThan(1e-6);
  });

  it('mixOffsetAndTrim', () => {
    const d = makePcm(64, 2);
    const src: ClipSource = {
      preloadedData: d.planar(),
      preloadedFrames: d.frames(),
      channels: 2,
      timelineStart: 16,
      timelineEnd: 32,
      sourceOffset: 4,
      fadeInSamples: 0,
      fadeOutSamples: 0,
      gain: 1.0,
      pan: 0.0,
    };

    const l = new Float32Array(64);
    const r = new Float32Array(64);
    const buf = [l, r];

    mixClipBlock(buf, 2, 8, 20, src);
    expect(Math.abs(l[0]! - kPanCenterGain * 1.0)).toBeLessThan(1e-6);
    expect(Math.abs(l[7]! - kPanCenterGain * 1.0)).toBeLessThan(1e-6);

    expect(l[8]!).toBe(0.0);
    expect(r[8]!).toBe(0.0);
  });

  it('mixFadeInOut', () => {
    const d = makePcm(32, 2);
    const src: ClipSource = {
      preloadedData: d.planar(),
      preloadedFrames: d.frames(),
      channels: 2,
      timelineStart: 0,
      timelineEnd: 32,
      sourceOffset: 0,
      fadeInSamples: 8,
      fadeOutSamples: 8,
      gain: 1.0,
      pan: 0.0,
    };

    const l = new Float32Array(32);
    const r = new Float32Array(32);
    const buf = [l, r];
    mixClipBlock(buf, 2, 32, 0, src);

    expect(Math.abs(l[0]!)).toBeLessThan(1e-6);
    expect(Math.abs(l[7]! - kPanCenterGain * (7.0 / 8.0))).toBeLessThan(1e-6);
    expect(Math.abs(l[16]! - kPanCenterGain)).toBeLessThan(1e-6);
    expect(Math.abs(l[24]! - kPanCenterGain)).toBeLessThan(1e-6);
    expect(Math.abs(l[31]! - kPanCenterGain * (1.0 / 8.0))).toBeLessThan(1e-6);
  });

  it('mixOutsideRangeIsSilent', () => {
    const d = makePcm(32, 2);
    const src: ClipSource = {
      preloadedData: d.planar(),
      preloadedFrames: d.frames(),
      channels: 2,
      timelineStart: 0,
      timelineEnd: 32,
      sourceOffset: 0,
      fadeInSamples: 0,
      fadeOutSamples: 0,
      gain: 1.0,
      pan: 0.0,
    };

    const l = new Float32Array(64);
    const r = new Float32Array(64);
    const buf = [l, r];

    mixClipBlock(buf, 2, 16, -32, src);
    expect(l[0]!).toBe(0.0);

    mixClipBlock(buf, 2, 16, 32, src);
    expect(l[0]!).toBe(0.0);
  });
});
