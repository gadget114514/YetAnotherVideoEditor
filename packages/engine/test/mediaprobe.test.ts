import { describe, it, expect } from 'vitest';
import { MediaProbe } from '../src/media/MediaProbe.js';
import * as fs from 'node:fs';
import * as path from 'node:path';
import * as os from 'node:os';

function createWavBuffer(
  sampleRate: number,
  channels: number,
  numSamples: number,
  fillValue: number = 0.5
): Uint8Array {
  const bitsPerSample = 16;
  const bytesPerSample = 2;
  const blockAlign = channels * bytesPerSample;
  const byteRate = sampleRate * blockAlign;
  const dataSize = numSamples * blockAlign;
  const buffer = new ArrayBuffer(44 + dataSize);
  const view = new DataView(buffer);

  // RIFF
  view.setUint8(0, 'R'.charCodeAt(0));
  view.setUint8(1, 'I'.charCodeAt(0));
  view.setUint8(2, 'F'.charCodeAt(0));
  view.setUint8(3, 'F'.charCodeAt(0));
  view.setUint32(4, 36 + dataSize, true);

  // WAVE
  view.setUint8(8, 'W'.charCodeAt(0));
  view.setUint8(9, 'A'.charCodeAt(0));
  view.setUint8(10, 'V'.charCodeAt(0));
  view.setUint8(11, 'E'.charCodeAt(0));

  // fmt
  view.setUint8(12, 'f'.charCodeAt(0));
  view.setUint8(13, 'm'.charCodeAt(0));
  view.setUint8(14, 't'.charCodeAt(0));
  view.setUint8(15, ' '.charCodeAt(0));
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true); // PCM format
  view.setUint16(22, channels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, byteRate, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitsPerSample, true);

  // data
  view.setUint8(36, 'd'.charCodeAt(0));
  view.setUint8(37, 'a'.charCodeAt(0));
  view.setUint8(38, 't'.charCodeAt(0));
  view.setUint8(39, 'a'.charCodeAt(0));
  view.setUint32(40, dataSize, true);

  let offset = 44;
  for (let i = 0; i < numSamples; i++) {
    const env = 1.0 - i / numSamples;
    const v = Math.round(Math.max(-32768, Math.min(32767, 32767.0 * fillValue * env)));
    for (let c = 0; c < channels; c++) {
      view.setInt16(offset, v, true);
      offset += 2;
    }
  }

  return new Uint8Array(buffer);
}

describe('TestMediaProbe', () => {
  it('audioDurationIsSampleCount', () => {
    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'yave-probe-'));
    const wavPath = path.join(tmpDir, 'mono.wav');

    const sampleRate = 48000;
    const samples = 12000; // 0.25 seconds
    const wavBytes = createWavBuffer(sampleRate, 1, samples);
    fs.writeFileSync(wavPath, wavBytes);

    const info = MediaProbe.probe(wavPath);
    expect(info.ok).toBe(true);
    expect(info.hasAudio).toBe(true);
    expect(info.hasVideo).toBe(false);
    expect(info.audioSampleRate).toBe(sampleRate);
    expect(info.audioChannels).toBe(1);
    expect(info.audioDurationFrames).toBe(samples);

    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  it('stereoChannelsAndRate', () => {
    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'yave-probe-'));
    const wavPath = path.join(tmpDir, 'stereo.wav');

    const sampleRate = 44100;
    const samples = 8000;
    const wavBytes = createWavBuffer(sampleRate, 2, samples);
    fs.writeFileSync(wavPath, wavBytes);

    const info = MediaProbe.probe(wavPath);
    expect(info.ok).toBe(true);
    expect(info.audioSampleRate).toBe(sampleRate);
    expect(info.audioChannels).toBe(2);
    expect(info.audioDurationFrames).toBe(samples);

    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  it('missingFileFails', () => {
    const info = MediaProbe.probe('does_not_exist_12345.wav');
    expect(info.ok).toBe(false);
    expect(info.error.length).toBeGreaterThan(0);
  });
});
