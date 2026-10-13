import { describe, it, expect, vi } from 'vitest';
import { Project } from '@yave/core';
import type { WhisperBridge, WhisperStatus } from '@yave/platform';
import { AiOrchestrator } from '../src/ai/AiOrchestrator.js';
import { LocalWhisperProvider, encodeWav16, mixToMono } from '../src/ai/LocalWhisperProvider.js';

const readyStatus: WhisperStatus = {
  supported: true,
  platform: 'win32',
  installDir: 'x',
  binaryPath: 'whisper-cli.exe',
  binarySource: 'managed',
  installedModels: ['small'],
  activeModel: 'small',
  ready: true,
  models: [],
  canAutoDownloadBinary: true,
};

function makeBridge(overrides: Partial<WhisperBridge> = {}): WhisperBridge {
  return {
    getStatus: async () => readyStatus,
    install: async () => readyStatus,
    cancelInstall: async () => {},
    selfTest: async () => ({ ok: true, message: 'ok' }),
    transcribe: async () => ({ srt: '1\n00:00:00,000 --> 00:00:01,500\nこんにちは\n', tokens: [] }),
    cancelTranscribe: async () => {},
    onInstallProgress: () => () => {},
    onTranscribeProgress: () => () => {},
    ...overrides,
  };
}

describe('encodeWav16 / mixToMono', () => {
  it('writes a 16 kHz mono PCM header and clamps samples', () => {
    const wav = encodeWav16(new Float32Array([0, 1, -1, 2]), 16000);
    const view = new DataView(wav.buffer);
    expect(String.fromCharCode(...wav.slice(0, 4))).toBe('RIFF');
    expect(view.getUint32(24, true)).toBe(16000);
    expect(view.getUint16(22, true)).toBe(1);
    expect(view.getUint32(40, true)).toBe(8);
    expect(view.getInt16(46, true)).toBe(0x7fff);
    expect(view.getInt16(48, true)).toBe(-0x8000);
    expect(view.getInt16(50, true)).toBe(0x7fff); // clamped
  });

  it('averages channels', () => {
    const mono = mixToMono([new Float32Array([1, 0]), new Float32Array([0, 1])]);
    expect(Array.from(mono)).toEqual([0.5, 0.5]);
  });
});

describe('LocalWhisperProvider', () => {
  it('converts audio, calls the bridge, and returns SRT', async () => {
    const transcribe = vi.fn(async () => ({ srt: '1\n00:00:00,000 --> 00:00:01,000\nhi\n', tokens: [] }));
    const convertToWav = vi.fn(async () => new Uint8Array([1, 2, 3]));
    const provider = new LocalWhisperProvider({ bridge: makeBridge({ transcribe }), convertToWav });
    const progress: number[] = [];

    const res = await provider.generate(
      { kind: 'subtitle', audioFile: new Uint8Array([9]), language: 'ja', prompt: 'Transcribe a.wav' },
      (p) => progress.push(p)
    );

    expect(res.srtContent).toContain('hi');
    expect(res.mediaType).toBe('subtitle');
    expect(transcribe).toHaveBeenCalledWith({ wav: new Uint8Array([1, 2, 3]), language: 'ja', prompt: undefined });
    expect(progress.at(-1)).toBe(1);
  });

  it('forwards a user prompt as a vocabulary hint', async () => {
    const transcribe = vi.fn(async () => ({ srt: '', tokens: [] }));
    const provider = new LocalWhisperProvider({
      bridge: makeBridge({ transcribe }),
      convertToWav: async () => new Uint8Array(),
    });
    await provider.generate({ kind: 'subtitle', audioFile: new Uint8Array([1]), prompt: 'YAVE, Whisper' }, () => {});
    expect(transcribe).toHaveBeenCalledWith(expect.objectContaining({ prompt: 'YAVE, Whisper' }));
  });

  it('fails clearly when not set up or no audio is given', async () => {
    const notReady = new LocalWhisperProvider({
      bridge: makeBridge({ getStatus: async () => ({ ...readyStatus, ready: false }) }),
      convertToWav: async () => new Uint8Array(),
    });
    await expect(
      notReady.generate({ kind: 'subtitle', audioFile: new Uint8Array([1]) }, () => {})
    ).rejects.toThrow(/not set up/);

    const provider = new LocalWhisperProvider({ bridge: makeBridge() });
    await expect(provider.generate({ kind: 'subtitle' }, () => {})).rejects.toThrow(/No audio file data/);
  });

  it('cancels the bridge when the signal aborts', async () => {
    const cancelTranscribe = vi.fn(async () => {});
    const ctrl = new AbortController();
    const provider = new LocalWhisperProvider({
      bridge: makeBridge({
        cancelTranscribe,
        transcribe: async () => {
          ctrl.abort();
          return { srt: '', tokens: [] };
        },
      }),
      convertToWav: async () => new Uint8Array(),
    });
    await provider.generate({ kind: 'subtitle', audioFile: new Uint8Array([1]) }, () => {}, ctrl.signal);
    expect(cancelTranscribe).toHaveBeenCalled();
  });

  it('works end-to-end through AiOrchestrator.importAudioAndTranscribe', async () => {
    const orchestrator = new AiOrchestrator();
    orchestrator.registerProvider(
      new LocalWhisperProvider({ bridge: makeBridge(), convertToWav: async () => new Uint8Array([0]) })
    );
    const project = new Project();
    const result = await orchestrator.importAudioAndTranscribe({
      project,
      audioName: 'talk.wav',
      audioData: new Uint8Array([1, 2, 3]),
      durationFrames: 240,
      language: 'ja',
      providerId: LocalWhisperProvider.ID,
    });
    expect(result.srtContent).toContain('こんにちは');
    expect(result.subtitleClips.length).toBe(1);
  });
});
