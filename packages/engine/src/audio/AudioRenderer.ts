import { type ClipSource, type AudioRenderGraph, type TrackNode } from './AudioRenderGraph.js';

export const kPanCenterGain = 0.70710678;

/**
 * Mixes a slice of audio clip samples into destination planar buffers.
 */
export function mixClipBlock(
  buf: (Float32Array | number[])[],
  channels: number,
  numFrames: number,
  blockStart: number,
  src: ClipSource
): void {
  if (!src.preloadedData || src.preloadedFrames <= 0) {
    return;
  }

  const blockEnd = blockStart + numFrames;
  if (src.timelineEnd <= blockStart || src.timelineStart >= blockEnd) {
    return;
  }

  const outStart = Math.max(blockStart, src.timelineStart);
  const outEnd = Math.min(blockEnd, src.timelineEnd);
  if (outStart >= outEnd) {
    return;
  }

  const srcBase = src.sourceOffset + (outStart - src.timelineStart);
  const totalLen = src.timelineEnd - src.timelineStart;

  // Equal-power pan law (constant power)
  const angle = (src.pan + 1.0) * 0.5 * (Math.PI / 2);
  const lGain = src.gain * Math.cos(angle);
  const rGain = src.gain * Math.sin(angle);

  const srcCh = Math.min(src.channels, channels);

  for (let i = 0; i < outEnd - outStart; i++) {
    const s = srcBase + i;
    if (s < 0 || s >= src.preloadedFrames) {
      continue;
    }
    const outIdx = outStart - blockStart + i;

    // Linear fade-in / fade-out
    const inClip = outStart - src.timelineStart + i;
    let fade = 1.0;
    if (src.fadeInSamples > 0 && inClip < src.fadeInSamples) {
      fade = inClip / src.fadeInSamples;
    } else if (src.fadeOutSamples > 0 && inClip >= totalLen - src.fadeOutSamples) {
      fade = (totalLen - inClip) / src.fadeOutSamples;
    }

    const b0 = buf[0];
    const b1 = buf[1];
    if (!b0) continue;

    if (srcCh >= 2 && channels >= 2 && b1) {
      b0[outIdx] = (b0[outIdx] ?? 0) + src.preloadedData[0]![s]! * lGain * fade;
      b1[outIdx] = (b1[outIdx] ?? 0) + src.preloadedData[1]![s]! * rGain * fade;
    } else if (channels >= 2 && b1) {
      const v = src.preloadedData[0]![s]! * fade;
      b0[outIdx] = (b0[outIdx] ?? 0) + v * lGain;
      b1[outIdx] = (b1[outIdx] ?? 0) + v * rGain;
    } else {
      b0[outIdx] = (b0[outIdx] ?? 0) + src.preloadedData[0]![s]! * src.gain * fade;
    }
  }
}

/**
 * Accumulates track buffer into master output buffer with track gain and pan.
 */
export function accumulateWithGainPan(
  src: Float32Array[],
  dst: Float32Array[],
  channels: number,
  frames: number,
  gain: number,
  pan: number
): void {
  const angle = (pan + 1.0) * 0.5 * (Math.PI / 2);
  const lGain = gain * Math.cos(angle);
  const rGain = gain * Math.sin(angle);

  const d0 = dst[0];
  const d1 = dst[1];
  const s0 = src[0];
  const s1 = src[1];
  if (!d0 || !s0) return;

  if (channels >= 2 && d1 && s1) {
    for (let i = 0; i < frames; i++) {
      const cur0 = d0[i] ?? 0;
      const cur1 = d1[i] ?? 0;
      const val0 = s0[i] ?? 0;
      const val1 = s1[i] ?? 0;
      d0[i] = cur0 + val0 * lGain;
      d1[i] = cur1 + val1 * rGain;
    }
  } else if (channels === 1) {
    for (let i = 0; i < frames; i++) {
      const cur0 = d0[i] ?? 0;
      const val0 = s0[i] ?? 0;
      d0[i] = cur0 + val0 * gain;
    }
  }
}

/**
 * Renders an audio quantum block (e.g. 128 frames) from an AudioRenderGraph.
 */
export function renderAudioBlock(
  graph: AudioRenderGraph,
  blockStart: number,
  numFrames: number,
  outChannels: number = 2
): Float32Array[] {
  const masterOut: Float32Array[] = [];
  for (let c = 0; c < outChannels; c++) {
    masterOut.push(new Float32Array(numFrames));
  }

  const trackScratch: Float32Array[] = [];
  for (let c = 0; c < outChannels; c++) {
    trackScratch.push(new Float32Array(numFrames));
  }

  for (const track of graph.tracks) {
    if (track.muted || (graph.anySolo && !track.solo)) {
      continue;
    }

    // Clear scratch buffer
    for (let c = 0; c < outChannels; c++) {
      trackScratch[c]!.fill(0);
    }

    // Mix clips
    for (const clip of track.clips) {
      mixClipBlock(trackScratch, outChannels, numFrames, blockStart, clip);
    }

    // Apply delay line if PDC configured
    if (track.compensationDelay > 0 && track.delayLine) {
      track.delayLine.process(trackScratch, outChannels, numFrames);
    }

    // Accumulate to master
    accumulateWithGainPan(trackScratch, masterOut, outChannels, numFrames, track.gain, track.pan);
  }

  // Master gain
  if (graph.masterGain !== 1.0) {
    for (let c = 0; c < outChannels; c++) {
      const line = masterOut[c];
      if (!line) continue;
      for (let i = 0; i < numFrames; i++) {
        const val = line[i] ?? 0;
        line[i] = val * graph.masterGain;
      }
    }
  }

  return masterOut;
}
