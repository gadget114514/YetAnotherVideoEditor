import { Project, Timeline, Track, AudioClip, RationalUtil } from '@yave/core';
import { DecodedAudio, type IAudioSourceProvider } from './DecodedAudio.js';
import { DelayLine } from './DelayCompensator.js';

export interface ClipSource {
  preloadedData: Float32Array[] | null;
  preloadedFrames: number;
  channels: number;
  timelineStart: number; // in samples
  timelineEnd: number;   // in samples
  sourceOffset: number;  // in samples
  fadeInSamples: number;
  fadeOutSamples: number;
  gain: number;
  pan: number;
}

export interface TrackNode {
  trackId: string;
  name: string;
  gain: number;
  pan: number;
  muted: boolean;
  solo: boolean;
  clips: ClipSource[];
  compensationDelay: number;
  delayLine?: DelayLine;
}

export interface AudioRenderGraph {
  tracks: TrackNode[];
  masterGain: number;
  anySolo: boolean;
  ownedAudio: DecodedAudio[];
}

export class AudioRenderGraphBuilder {
  static build(
    timeline: Timeline,
    project: Project,
    provider?: IAudioSourceProvider | null
  ): AudioRenderGraph {
    const tracks: TrackNode[] = [];
    const ownedAudio: DecodedAudio[] = [];

    const sampleRate = project.sampleRate ?? 48000;
    const tb = timeline.timebase;
    const f2s = sampleRate / RationalUtil.toDouble(tb);

    let anySolo = false;

    for (let tIdx = 0; tIdx < timeline.trackCount; tIdx++) {
      const track = timeline.trackAt(tIdx);
      if (!track || !track.participatesInAudioGraph()) {
        continue;
      }

      if (track.solo) {
        anySolo = true;
      }

      const clipSources: ClipSource[] = [];

      for (const clip of track.clips) {
        if (!clip || clip.type !== 'audio') {
          continue;
        }

        const audioClip = clip as AudioClip;
        let preloaded: DecodedAudio | null = null;

        if (provider && audioClip.assetId) {
          preloaded = provider.decodeAsset(audioClip.assetId);
          if (preloaded) {
            ownedAudio.push(preloaded);
          }
        }

        const startSample = Math.round(audioClip.range.start * f2s);
        const endSample = Math.round((audioClip.range.start + audioClip.range.duration) * f2s);
        const offsetSample = Math.round(audioClip.sourceOffset() * f2s);
        const fadeInSample = Math.round(audioClip.fadeIn * f2s);
        const fadeOutSample = Math.round(audioClip.fadeOut * f2s);

        clipSources.push({
          preloadedData: preloaded ? preloaded.planar() : null,
          preloadedFrames: preloaded ? preloaded.frames() : 0,
          channels: preloaded ? preloaded.channels : 2,
          timelineStart: startSample,
          timelineEnd: endSample,
          sourceOffset: offsetSample,
          fadeInSamples: fadeInSample,
          fadeOutSamples: fadeOutSample,
          gain: audioClip.gain,
          pan: audioClip.pan,
        });
      }

      tracks.push({
        trackId: track.id,
        name: track.name,
        gain: track.gain,
        pan: track.pan,
        muted: track.muted,
        solo: track.solo,
        clips: clipSources,
        compensationDelay: 0,
      });
    }

    return {
      tracks,
      masterGain: 1.0,
      anySolo,
      ownedAudio,
    };
  }
}
