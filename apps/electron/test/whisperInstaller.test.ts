import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { createHash } from 'crypto';
import { promises as fs } from 'fs';
import os from 'os';
import path from 'path';
import {
  WHISPER_MODELS,
  WHISPER_CPP_BINARY,
  buildWhisperArgs,
  downloadFile,
  getWhisperPaths,
  installWhisper,
  listInstalledModels,
  locateBinary,
  makeSilentWav,
  parseWhisperTokens,
} from '../src/main/whisper/whisperInstaller';

function fetchReturning(body: Buffer, status = 200): typeof fetch {
  return (async () =>
    new Response(new Uint8Array(body), {
      status,
      headers: { 'content-length': String(body.length) },
    })) as unknown as typeof fetch;
}

let tmp: string;
beforeEach(async () => {
  tmp = await fs.mkdtemp(path.join(os.tmpdir(), 'yave-whisper-test-'));
});
afterEach(async () => {
  await fs.rm(tmp, { recursive: true, force: true });
});

describe('downloadFile', () => {
  it('writes the file and reports progress when sha256 matches', async () => {
    const data = Buffer.from('hello whisper');
    const sha = createHash('sha256').update(data).digest('hex');
    const dest = path.join(tmp, 'a.bin');
    const seen: number[] = [];
    await downloadFile('https://example.test/a.bin', dest, {
      fetchFn: fetchReturning(data),
      sha256: sha,
      onProgress: (r) => seen.push(r),
    });
    expect((await fs.readFile(dest)).equals(data)).toBe(true);
    expect(seen.at(-1)).toBe(data.length);
    await expect(fs.access(`${dest}.part`)).rejects.toThrow();
  });

  it('rejects and removes partial file on sha256 mismatch', async () => {
    const dest = path.join(tmp, 'b.bin');
    await expect(
      downloadFile('https://example.test/b.bin', dest, {
        fetchFn: fetchReturning(Buffer.from('tampered')),
        sha256: '0'.repeat(64),
      })
    ).rejects.toThrow(/SHA-256 mismatch/);
    await expect(fs.access(dest)).rejects.toThrow();
    await expect(fs.access(`${dest}.part`)).rejects.toThrow();
  });

  it('refuses non-HTTPS URLs and HTTP errors', async () => {
    await expect(downloadFile('http://example.test/x', path.join(tmp, 'x'))).rejects.toThrow(/HTTPS/);
    await expect(
      downloadFile('https://example.test/x', path.join(tmp, 'x'), { fetchFn: fetchReturning(Buffer.alloc(0), 404) })
    ).rejects.toThrow(/404/);
  });
});

describe('locateBinary', () => {
  it('finds a managed binary nested in the extracted archive', async () => {
    const paths = getWhisperPaths(tmp);
    const exe = process.platform === 'win32' ? 'whisper-cli.exe' : 'whisper-cli';
    await fs.mkdir(path.join(paths.binDir, 'Release'), { recursive: true });
    await fs.writeFile(path.join(paths.binDir, 'Release', exe), 'x');
    const found = await locateBinary(paths, undefined, '');
    expect(found?.source).toBe('managed');
    expect(found?.path.endsWith(exe)).toBe(true);
  });

  it('returns null when nothing is installed', async () => {
    expect(await locateBinary(getWhisperPaths(tmp), undefined, '')).toBeNull();
  });
});

describe('installWhisper', () => {
  it('downloads binary + model via pinned URLs, extracts, and records the active model', async () => {
    const urls: string[] = [];
    const zip = Buffer.from('zip');
    const model = Buffer.from('model');
    const spec = WHISPER_MODELS.find((m) => m.id === 'tiny')!;
    // Patch the pinned digests/sizes through a fetch that returns data matching them is impossible,
    // so assert the sha256 guard rejects unexpected content for the pinned binary.
    const fetchFn = (async (url: string) => {
      urls.push(url);
      return new Response(new Uint8Array(url.endsWith('.zip') ? zip : model), { status: 200 });
    }) as unknown as typeof fetch;

    await expect(
      installWhisper({ baseDir: tmp, modelId: 'tiny', platform: 'win32', fetchFn, envPath: '', extract: async () => {} })
    ).rejects.toThrow(/SHA-256 mismatch/);
    expect(urls[0]).toBe(WHISPER_CPP_BINARY.url);
    expect(spec.file).toBe('ggml-tiny.bin');
  });

  it('with an existing binary only downloads the model and fails on bad checksum', async () => {
    const paths = getWhisperPaths(tmp);
    const exe = process.platform === 'win32' ? 'whisper-cli.exe' : 'whisper-cli';
    await fs.mkdir(paths.binDir, { recursive: true });
    await fs.writeFile(path.join(paths.binDir, exe), 'x');
    const urls: string[] = [];
    const fetchFn = (async (url: string) => {
      urls.push(url);
      return new Response(new Uint8Array(Buffer.from('nope')), { status: 200 });
    }) as unknown as typeof fetch;
    await expect(
      installWhisper({ baseDir: tmp, modelId: 'base', fetchFn, envPath: '' })
    ).rejects.toThrow(/SHA-256 mismatch/);
    expect(urls).toEqual(['https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin']);
    expect(await listInstalledModels(paths)).toEqual([]);
  });

  it('errors helpfully when no binary exists and download is not possible', async () => {
    await expect(
      installWhisper({ baseDir: tmp, modelId: 'tiny', platform: 'linux', envPath: '' })
    ).rejects.toThrow(/whisper-cli was not found/);
  });

  it('rejects unknown model ids', async () => {
    await expect(installWhisper({ baseDir: tmp, modelId: 'huge' as any, envPath: '' })).rejects.toThrow(/Unknown whisper model/);
  });
});

describe('parseWhisperTokens', () => {
  const json = (tokens: unknown[]) => JSON.stringify({ transcription: [{ tokens }] });

  it('drops special tokens and prefers DTW timing', () => {
    const out = parseWhisperTokens(
      json([
        { text: '[_BEG_]', offsets: { from: 0, to: 0 } },
        { text: 'こん', offsets: { from: 100, to: 400 }, t_dtw: 12 },
        { text: 'にちは', offsets: { from: 400, to: 900 }, t_dtw: 40 },
        { text: '[_TT_50]', offsets: { from: 900, to: 900 } },
      ])
    );
    expect(out.map((t) => t.text)).toEqual(['こん', 'にちは']);
    expect(out[0]!.start).toBeCloseTo(0.12, 5);
    expect(out[0]!.end).toBeCloseTo(0.4, 5); // up to the next token's DTW start
    expect(out[1]!.end).toBeCloseTo(0.9, 5);
  });

  it('merges tokens that split a multi-byte character into the next token', () => {
    const out = parseWhisperTokens(
      json([
        { text: '�', offsets: { from: 1000, to: 1100 } },
        { text: '�', offsets: { from: 1100, to: 1200 } },
        { text: '歌', offsets: { from: 1200, to: 1500 } },
      ])
    );
    expect(out).toHaveLength(1);
    expect(out[0]).toMatchObject({ text: '歌', start: 1, end: 1.5 });
  });

  it('returns [] for invalid JSON', () => {
    expect(parseWhisperTokens('not json')).toEqual([]);
  });
});

describe('buildWhisperArgs / makeSilentWav', () => {
  it('builds a safe argument vector', () => {
    const args = buildWhisperArgs(
      { binaryPath: 'w', modelFile: 'm.bin', wavPath: 'in.wav', language: 'ja', prompt: 'YAVE', threads: 4 },
      'out'
    );
    expect(args).toEqual(
      expect.arrayContaining(['-m', 'm.bin', '-f', 'in.wav', '-osrt', '-of', 'out', '-l', 'ja', '-t', '4', '--prompt', 'YAVE'])
    );
  });

  it('requests full JSON and only adds DTW flags when a preset is given', () => {
    const base = { binaryPath: 'w', modelFile: 'm', wavPath: 'a.wav' };
    expect(buildWhisperArgs(base, 'o')).toContain('-ojf');
    expect(buildWhisperArgs(base, 'o')).not.toContain('-dtw');
    const dtw = buildWhisperArgs({ ...base, dtwPreset: 'small' }, 'o');
    expect(dtw[dtw.indexOf('-dtw') + 1]).toBe('small');
    expect(dtw).toContain('-nfa');
  });

  it('falls back to auto for invalid language codes', () => {
    const args = buildWhisperArgs({ binaryPath: 'w', modelFile: 'm', wavPath: 'a.wav', language: '--evil' }, 'o');
    expect(args[args.indexOf('-l') + 1]).toBe('auto');
  });

  it('creates a valid 16 kHz mono WAV header', () => {
    const wav = makeSilentWav(1);
    expect(wav.toString('ascii', 0, 4)).toBe('RIFF');
    expect(wav.readUInt32LE(24)).toBe(16000);
    expect(wav.length).toBe(44 + 32000);
  });
});
