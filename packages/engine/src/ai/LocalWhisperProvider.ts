import { createUuid } from '@yave/core';
import type { WhisperBridge } from '@yave/platform';
import type { AiGenerationParams, AiTaskResult, IGenerationProvider } from './AiOrchestrator.js';

export const WHISPER_SAMPLE_RATE = 16000;

/** Encodes mono float samples (-1..1) as a 16-bit PCM RIFF/WAVE file. */
export function encodeWav16(samples: Float32Array, sampleRate = WHISPER_SAMPLE_RATE): Uint8Array {
  const out = new Uint8Array(44 + samples.length * 2);
  const view = new DataView(out.buffer);
  const writeStr = (off: number, s: string) => {
    for (let i = 0; i < s.length; i++) view.setUint8(off + i, s.charCodeAt(i));
  };
  writeStr(0, 'RIFF');
  view.setUint32(4, 36 + samples.length * 2, true);
  writeStr(8, 'WAVE');
  writeStr(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true); // PCM
  view.setUint16(22, 1, true); // mono
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true);
  view.setUint16(34, 16, true);
  writeStr(36, 'data');
  view.setUint32(40, samples.length * 2, true);
  for (let i = 0; i < samples.length; i++) {
    const s = Math.max(-1, Math.min(1, samples[i]!));
    view.setInt16(44 + i * 2, s < 0 ? s * 0x8000 : s * 0x7fff, true);
  }
  return out;
}

/** Averages all channels of a decoded AudioBuffer-like object into a mono Float32Array. */
export function mixToMono(channels: Float32Array[]): Float32Array {
  if (channels.length === 1) return channels[0]!;
  const len = channels[0]?.length ?? 0;
  const mono = new Float32Array(len);
  for (const ch of channels) for (let i = 0; i < len; i++) mono[i]! += ch[i]! / channels.length;
  return mono;
}

async function toBytes(input: unknown): Promise<Uint8Array> {
  if (input instanceof Uint8Array) return input;
  if (input instanceof ArrayBuffer) return new Uint8Array(input);
  if (typeof Blob !== 'undefined' && input instanceof Blob) return new Uint8Array(await input.arrayBuffer());
  throw new Error('No audio file data provided for local Whisper transcription.');
}

/**
 * Decodes any browser-decodable audio (mp3/wav/ogg/m4a/...) and resamples it to 16 kHz mono WAV,
 * the only input format whisper.cpp accepts without ffmpeg.
 */
export async function convertToWhisperWav(input: unknown): Promise<Uint8Array> {
  const bytes = await toBytes(input);
  const Ctx = (globalThis as any).AudioContext ?? (globalThis as any).webkitAudioContext;
  const Offline = (globalThis as any).OfflineAudioContext;
  if (!Ctx || !Offline) throw new Error('Web Audio API is not available to decode the audio file.');

  const ctx = new Ctx();
  try {
    const copy = bytes.slice().buffer;
    const decoded: AudioBuffer = await ctx.decodeAudioData(copy);
    const frames = Math.max(1, Math.ceil(decoded.duration * WHISPER_SAMPLE_RATE));
    const offline = new Offline(1, frames, WHISPER_SAMPLE_RATE);
    const src = offline.createBufferSource();
    src.buffer = decoded;
    src.connect(offline.destination);
    src.start();
    const rendered: AudioBuffer = await offline.startRendering();
    return encodeWav16(rendered.getChannelData(0), WHISPER_SAMPLE_RATE);
  } finally {
    void ctx.close?.();
  }
}

export interface LocalWhisperProviderOptions {
  bridge: WhisperBridge;
  /** test seam: audio -> 16 kHz mono WAV */
  convertToWav?: (input: unknown) => Promise<Uint8Array>;
}

/** Speech-to-text provider backed by a locally installed whisper.cpp (desktop build). */
export class LocalWhisperProvider implements IGenerationProvider {
  static readonly ID = 'local-whisper';
  readonly id = LocalWhisperProvider.ID;
  readonly name = 'Local Whisper (whisper.cpp)';

  constructor(private readonly options_: LocalWhisperProviderOptions) {}

  async generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<AiTaskResult> {
    const { bridge } = this.options_;
    const convert = this.options_.convertToWav ?? convertToWhisperWav;

    if (params.audioFile == null) {
      throw new Error('No audio file data provided for local Whisper transcription.');
    }
    const status = await bridge.getStatus();
    if (!status.ready) {
      throw new Error('Local Whisper is not set up yet. Open the Whisper setup wizard from the AI panel.');
    }

    onProgress(0.05);
    const wav = await convert(params.audioFile);
    onProgress(0.15);

    const off = bridge.onTranscribeProgress((p) => onProgress(0.15 + p * 0.8));
    const onAbort = () => void bridge.cancelTranscribe();
    signal?.addEventListener('abort', onAbort);
    try {
      const prompt = params.prompt && !params.prompt.startsWith('Transcribe ') ? params.prompt : undefined;
      const { srt, tokens } = await bridge.transcribe({ wav, language: params.language, prompt });
      onProgress(1);
      return {
        assetId: createUuid(),
        mediaType: 'subtitle',
        srtContent: srt,
        // token-level timing lets lyric alignment work at character resolution
        data: { tokens: tokens ?? [] },
      };
    } finally {
      off();
      signal?.removeEventListener('abort', onAbort);
    }
  }
}
