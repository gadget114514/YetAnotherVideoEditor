import { createHash } from 'crypto';
import { spawn, execFile } from 'child_process';
import { createWriteStream, promises as fs } from 'fs';
import os from 'os';
import path from 'path';
import { Readable } from 'stream';

/**
 * Node-only helpers that install and drive a local whisper.cpp (`whisper-cli`).
 * Nothing here imports `electron`, so it can be unit-tested with injected fetch / paths.
 */

export type WhisperModelId = 'tiny' | 'base' | 'small' | 'medium' | 'large-v3-turbo';

export interface WhisperModelSpec {
  readonly id: WhisperModelId;
  readonly label: string;
  readonly file: string;
  readonly sizeBytes: number;
  readonly sha256: string;
  readonly note: string;
}

const HF_BASE = 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main';

export const WHISPER_MODELS: readonly WhisperModelSpec[] = [
  {
    id: 'tiny',
    label: 'tiny (75 MB)',
    file: 'ggml-tiny.bin',
    sizeBytes: 77691713,
    sha256: 'be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21',
    note: 'fastest / lowest accuracy',
  },
  {
    id: 'base',
    label: 'base (142 MB)',
    file: 'ggml-base.bin',
    sizeBytes: 147951465,
    sha256: '60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe',
    note: 'fast',
  },
  {
    id: 'small',
    label: 'small (466 MB)',
    file: 'ggml-small.bin',
    sizeBytes: 487601967,
    sha256: '1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b',
    note: 'recommended balance for Japanese',
  },
  {
    id: 'medium',
    label: 'medium (1.5 GB)',
    file: 'ggml-medium.bin',
    sizeBytes: 1533763059,
    sha256: '6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208',
    note: 'accurate / slow on CPU',
  },
  {
    id: 'large-v3-turbo',
    label: 'large-v3-turbo q5_0 (547 MB)',
    file: 'ggml-large-v3-turbo-q5_0.bin',
    sizeBytes: 574041195,
    sha256: '394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2',
    note: 'best accuracy per size (quantized)',
  },
];

/** Pinned whisper.cpp prebuilt Windows x64 (CPU) release. Verified by SHA-256 after download. */
export const WHISPER_CPP_BINARY = {
  tag: 'b5454',
  assetName: 'whisper-bin-x64.zip',
  sha256: '6ba69e3482d7826214f90a6a9c84ca07782aec1e1d0c6a7c30c994fd5d816ccb',
  url: 'https://github.com/ggml-org/whisper.cpp/releases/download/b5454/whisper-bin-x64.zip',
} as const;

export interface WhisperInstallProgress {
  phase: 'binary' | 'extract' | 'model' | 'verify' | 'done';
  /** 0..1 within the current phase */
  progress: number;
  message: string;
}

export type BinarySource = 'managed' | 'path' | 'custom';

export interface WhisperPaths {
  readonly root: string;
  readonly binDir: string;
  readonly modelsDir: string;
  readonly configFile: string;
}

export function getWhisperPaths(baseDir: string): WhisperPaths {
  const root = path.join(baseDir, 'whisper');
  return {
    root,
    binDir: path.join(root, 'bin'),
    modelsDir: path.join(root, 'models'),
    configFile: path.join(root, 'config.json'),
  };
}

export interface WhisperConfig {
  activeModel?: WhisperModelId;
  customBinaryPath?: string;
}

export async function readConfig(paths: WhisperPaths): Promise<WhisperConfig> {
  try {
    return JSON.parse(await fs.readFile(paths.configFile, 'utf8')) as WhisperConfig;
  } catch {
    return {};
  }
}

export async function writeConfig(paths: WhisperPaths, cfg: WhisperConfig): Promise<void> {
  await fs.mkdir(paths.root, { recursive: true });
  await fs.writeFile(paths.configFile, JSON.stringify(cfg, null, 2), 'utf8');
}

export function getModelSpec(id: string): WhisperModelSpec {
  const spec = WHISPER_MODELS.find((m) => m.id === id);
  if (!spec) throw new Error(`Unknown whisper model: ${id}`);
  return spec;
}

export function modelPath(paths: WhisperPaths, id: WhisperModelId): string {
  return path.join(paths.modelsDir, getModelSpec(id).file);
}

const BINARY_NAMES = process.platform === 'win32' ? ['whisper-cli.exe'] : ['whisper-cli', 'whisper-cpp'];

async function isFile(p: string): Promise<boolean> {
  try {
    return (await fs.stat(p)).isFile();
  } catch {
    return false;
  }
}

async function findFileRecursive(dir: string, names: readonly string[], depth = 4): Promise<string | null> {
  let entries;
  try {
    entries = await fs.readdir(dir, { withFileTypes: true });
  } catch {
    return null;
  }
  for (const e of entries) {
    if (e.isFile() && names.includes(e.name.toLowerCase())) return path.join(dir, e.name);
  }
  if (depth <= 0) return null;
  for (const e of entries) {
    if (e.isDirectory()) {
      const found = await findFileRecursive(path.join(dir, e.name), names, depth - 1);
      if (found) return found;
    }
  }
  return null;
}

async function findOnPath(names: readonly string[], envPath = process.env.PATH ?? ''): Promise<string | null> {
  for (const dir of envPath.split(path.delimiter).filter(Boolean)) {
    for (const n of names) {
      const p = path.join(dir, n);
      if (await isFile(p)) return p;
    }
  }
  return null;
}

export async function locateBinary(
  paths: WhisperPaths,
  customPath?: string,
  envPath?: string
): Promise<{ path: string; source: BinarySource } | null> {
  if (customPath && (await isFile(customPath))) return { path: customPath, source: 'custom' };
  const managed = await findFileRecursive(paths.binDir, BINARY_NAMES);
  if (managed) return { path: managed, source: 'managed' };
  const onPath = await findOnPath(BINARY_NAMES, envPath);
  if (onPath) return { path: onPath, source: 'path' };
  return null;
}

export interface DownloadOptions {
  fetchFn?: typeof fetch;
  sha256?: string;
  expectedSize?: number;
  signal?: AbortSignal;
  onProgress?: (received: number, total: number) => void;
}

/** Streams `url` to `dest` (via `dest.part`), verifying SHA-256 when provided. */
export async function downloadFile(url: string, dest: string, opts: DownloadOptions = {}): Promise<void> {
  const fetchFn = opts.fetchFn ?? fetch;
  if (!url.startsWith('https://')) throw new Error(`Refusing non-HTTPS download: ${url}`);

  await fs.mkdir(path.dirname(dest), { recursive: true });
  const part = `${dest}.part`;
  const res = await fetchFn(url, { signal: opts.signal, redirect: 'follow' });
  if (!res.ok || !res.body) throw new Error(`Download failed (${res.status}) for ${url}`);

  const total = Number(res.headers.get('content-length')) || opts.expectedSize || 0;
  const hash = createHash('sha256');
  let received = 0;

  const out = createWriteStream(part);
  try {
    const source = Readable.fromWeb(res.body as any);
    for await (const chunk of source as AsyncIterable<Buffer>) {
      hash.update(chunk);
      received += chunk.length;
      if (!out.write(chunk)) await new Promise<void>((r) => out.once('drain', () => r()));
      opts.onProgress?.(received, total);
    }
    await new Promise<void>((resolve, reject) => {
      out.once('error', reject);
      out.end(() => resolve());
    });
  } catch (err) {
    out.destroy();
    await fs.rm(part, { force: true });
    throw err;
  }

  const digest = hash.digest('hex');
  if (opts.sha256 && digest !== opts.sha256.toLowerCase()) {
    await fs.rm(part, { force: true });
    throw new Error(`SHA-256 mismatch for ${path.basename(dest)} (expected ${opts.sha256}, got ${digest})`);
  }
  await fs.rename(part, dest);
}

function run(cmd: string, args: string[]): Promise<void> {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { windowsHide: true }, (err, _stdout, stderr) => {
      if (err) reject(new Error(`${cmd} failed: ${stderr || err.message}`));
      else resolve();
    });
  });
}

/** Extracts a .zip using the OS built-in bsdtar (Windows 10+, macOS) or PowerShell as fallback. */
export async function extractZip(zipPath: string, destDir: string): Promise<void> {
  await fs.mkdir(destDir, { recursive: true });
  try {
    await run('tar', ['-xf', zipPath, '-C', destDir]);
  } catch (tarErr) {
    if (process.platform !== 'win32') throw tarErr;
    await run('powershell.exe', [
      '-NoProfile',
      '-NonInteractive',
      '-Command',
      `Expand-Archive -LiteralPath '${zipPath.replace(/'/g, "''")}' -DestinationPath '${destDir.replace(/'/g, "''")}' -Force`,
    ]);
  }
}

export interface InstallOptions {
  baseDir: string;
  modelId: WhisperModelId;
  /** Use an already-installed whisper-cli (PATH or this explicit path) instead of downloading one. */
  useExistingBinary?: boolean;
  customBinaryPath?: string;
  fetchFn?: typeof fetch;
  signal?: AbortSignal;
  onProgress?: (p: WhisperInstallProgress) => void;
  /** test seam */
  extract?: (zip: string, dest: string) => Promise<void>;
  platform?: NodeJS.Platform;
  envPath?: string;
}

export async function installWhisper(opts: InstallOptions): Promise<void> {
  const paths = getWhisperPaths(opts.baseDir);
  const spec = getModelSpec(opts.modelId);
  const platform = opts.platform ?? process.platform;
  const emit = (p: WhisperInstallProgress) => opts.onProgress?.(p);
  const extract = opts.extract ?? extractZip;

  await fs.mkdir(paths.root, { recursive: true });
  const cfg = await readConfig(paths);

  // 1. binary
  let binary = await locateBinary(paths, opts.customBinaryPath ?? cfg.customBinaryPath, opts.envPath);
  if (opts.customBinaryPath && binary?.source !== 'custom') {
    throw new Error(`whisper-cli not found at: ${opts.customBinaryPath}`);
  }
  if (!binary) {
    if (opts.useExistingBinary || platform !== 'win32') {
      throw new Error(
        platform === 'win32'
          ? 'whisper-cli was not found. Install whisper.cpp or let the wizard download it.'
          : 'whisper-cli was not found on PATH. Install whisper.cpp (macOS: `brew install whisper-cpp`; Linux: build from source or use the release tarball) and retry, or specify its path.'
      );
    }
    const zipPath = path.join(paths.root, WHISPER_CPP_BINARY.assetName);
    emit({ phase: 'binary', progress: 0, message: `Downloading whisper.cpp ${WHISPER_CPP_BINARY.tag}...` });
    await downloadFile(WHISPER_CPP_BINARY.url, zipPath, {
      fetchFn: opts.fetchFn,
      sha256: WHISPER_CPP_BINARY.sha256,
      signal: opts.signal,
      onProgress: (r, t) =>
        emit({ phase: 'binary', progress: t ? r / t : 0, message: 'Downloading whisper.cpp...' }),
    });
    emit({ phase: 'extract', progress: 0, message: 'Extracting...' });
    await fs.rm(paths.binDir, { recursive: true, force: true });
    await extract(zipPath, paths.binDir);
    await fs.rm(zipPath, { force: true });
    binary = await locateBinary(paths, undefined, opts.envPath);
    if (!binary) throw new Error('whisper-cli.exe was not found in the downloaded archive.');
    emit({ phase: 'extract', progress: 1, message: 'Extracted.' });
  }

  // 2. model
  const dest = modelPath(paths, spec.id);
  let haveModel = false;
  try {
    haveModel = (await fs.stat(dest)).size === spec.sizeBytes;
  } catch {
    /* not present */
  }
  if (!haveModel) {
    emit({ phase: 'model', progress: 0, message: `Downloading model ${spec.file}...` });
    await downloadFile(`${HF_BASE}/${spec.file}`, dest, {
      fetchFn: opts.fetchFn,
      sha256: spec.sha256,
      expectedSize: spec.sizeBytes,
      signal: opts.signal,
      onProgress: (r, t) =>
        emit({ phase: 'model', progress: t ? r / t : 0, message: `Downloading model ${spec.file}...` }),
    });
  }

  await writeConfig(paths, {
    ...cfg,
    activeModel: spec.id,
    customBinaryPath: binary.source === 'custom' ? binary.path : cfg.customBinaryPath,
  });
  emit({ phase: 'done', progress: 1, message: 'Whisper is ready.' });
}

export async function listInstalledModels(paths: WhisperPaths): Promise<WhisperModelId[]> {
  const out: WhisperModelId[] = [];
  for (const m of WHISPER_MODELS) {
    try {
      if ((await fs.stat(path.join(paths.modelsDir, m.file))).size === m.sizeBytes) out.push(m.id);
    } catch {
      /* missing */
    }
  }
  return out;
}

/** whisper.cpp `-dtw` presets (token-level timestamps from cross-attention DTW). */
const DTW_PRESETS: Record<WhisperModelId, string> = {
  tiny: 'tiny',
  base: 'base',
  small: 'small',
  medium: 'medium',
  'large-v3-turbo': 'large.v3.turbo',
};

export function dtwPresetFor(id: WhisperModelId): string {
  return DTW_PRESETS[id];
}

export interface WhisperToken {
  text: string;
  start: number;
  end: number;
}

export interface TranscribeResult {
  srt: string;
  /** token-level timing (seconds); empty when unavailable */
  tokens: WhisperToken[];
}

export interface TranscribeOptions {
  binaryPath: string;
  modelFile: string;
  wavPath: string;
  language?: string;
  prompt?: string;
  threads?: number;
  /** enables DTW token timestamps (also needs flash attention off) */
  dtwPreset?: string;
  signal?: AbortSignal;
  onProgress?: (progress: number) => void;
}

/** Builds the whisper-cli argument vector (exported for tests). */
export function buildWhisperArgs(o: TranscribeOptions, outBase: string): string[] {
  const args = [
    '-m', o.modelFile,
    '-f', o.wavPath,
    '-osrt',
    '-ojf',
    '-of', outBase,
    '-l', o.language && /^[a-z]{2,3}$|^auto$/.test(o.language) ? o.language : 'auto',
    '-t', String(o.threads ?? Math.max(1, Math.min(8, os.cpus().length))),
    '-pp',
  ];
  if (o.dtwPreset) args.push('-dtw', o.dtwPreset, '-nfa');
  if (o.prompt) args.push('--prompt', o.prompt);
  return args;
}

/**
 * Extracts token timings from whisper.cpp's `--output-json-full`. Special tokens are dropped and
 * tokens that split a multi-byte character (U+FFFD) are merged into the following token.
 */
export function parseWhisperTokens(jsonText: string): WhisperToken[] {
  let data: any;
  try {
    data = JSON.parse(jsonText);
  } catch {
    return [];
  }
  const out: WhisperToken[] = [];
  for (const seg of Array.isArray(data?.transcription) ? data.transcription : []) {
    const toks = Array.isArray(seg?.tokens) ? seg.tokens : [];
    let pendingStart: number | null = null;
    let pendingText = '';
    for (let i = 0; i < toks.length; i++) {
      const t = toks[i];
      const text = typeof t?.text === 'string' ? t.text : '';
      if (!text || /^\[_[A-Z]+_?\d*\]$/.test(text) || text.startsWith('<|')) continue;
      const fromMs = typeof t?.t_dtw === 'number' && t.t_dtw >= 0 ? t.t_dtw * 10 : Number(t?.offsets?.from);
      let toMs = Number(t?.offsets?.to);
      const next = toks[i + 1];
      if (typeof t?.t_dtw === 'number' && t.t_dtw >= 0 && typeof next?.t_dtw === 'number' && next.t_dtw >= 0) {
        toMs = Math.max(fromMs, next.t_dtw * 10);
      }
      if (!Number.isFinite(fromMs) || !Number.isFinite(toMs)) continue;
      if (text.includes('�')) {
        pendingStart ??= fromMs;
        pendingText += text.replace(/�/g, '');
        continue;
      }
      out.push({
        text: pendingText + text,
        start: (pendingStart ?? fromMs) / 1000,
        end: Math.max(toMs, fromMs) / 1000,
      });
      pendingStart = null;
      pendingText = '';
    }
  }
  return out;
}

/**
 * Runs whisper-cli on a 16 kHz mono WAV. Tries token-level DTW timestamps first and falls back
 * to a plain run when the binary does not support those flags.
 */
export async function transcribeWav(o: TranscribeOptions): Promise<TranscribeResult> {
  const attempts: TranscribeOptions[] = o.dtwPreset ? [o, { ...o, dtwPreset: undefined }] : [o];
  let lastError: unknown;
  for (const attempt of attempts) {
    try {
      return await runWhisperOnce(attempt);
    } catch (err) {
      if (o.signal?.aborted) throw err;
      lastError = err;
    }
  }
  throw lastError;
}

async function runWhisperOnce(o: TranscribeOptions): Promise<TranscribeResult> {
  const outBase = o.wavPath.replace(/\.wav$/i, '');
  const args = buildWhisperArgs(o, outBase);

  await new Promise<void>((resolve, reject) => {
    const child = spawn(o.binaryPath, args, {
      windowsHide: true,
      cwd: path.dirname(o.binaryPath),
      signal: o.signal,
    });
    let stderr = '';
    const onData = (buf: Buffer) => {
      const text = buf.toString();
      stderr = (stderr + text).slice(-4000);
      for (const m of text.matchAll(/progress\s*=\s*(\d+)%/g)) {
        o.onProgress?.(Math.min(1, Number(m[1]) / 100));
      }
    };
    child.stderr.on('data', onData);
    child.stdout.on('data', onData);
    child.once('error', (e) => reject(e));
    child.once('close', (code) => {
      if (code === 0) resolve();
      else reject(new Error(`whisper-cli exited with code ${code}: ${stderr.trim().split('\n').slice(-5).join('\n')}`));
    });
  });

  const srtFile = `${outBase}.srt`;
  const jsonFile = `${outBase}.json`;
  try {
    const srt = await fs.readFile(srtFile, 'utf8');
    let tokens: WhisperToken[] = [];
    try {
      tokens = parseWhisperTokens(await fs.readFile(jsonFile, 'utf8'));
    } catch {
      /* token timing is optional */
    }
    return { srt, tokens };
  } finally {
    await fs.rm(srtFile, { force: true });
    await fs.rm(jsonFile, { force: true });
  }
}

/** 1 second of 16 kHz mono 16-bit silence, used for the installation self-test. */
export function makeSilentWav(seconds = 1, sampleRate = 16000): Buffer {
  const samples = Math.floor(seconds * sampleRate);
  const buf = Buffer.alloc(44 + samples * 2);
  buf.write('RIFF', 0);
  buf.writeUInt32LE(36 + samples * 2, 4);
  buf.write('WAVE', 8);
  buf.write('fmt ', 12);
  buf.writeUInt32LE(16, 16);
  buf.writeUInt16LE(1, 20);
  buf.writeUInt16LE(1, 22);
  buf.writeUInt32LE(sampleRate, 24);
  buf.writeUInt32LE(sampleRate * 2, 28);
  buf.writeUInt16LE(2, 32);
  buf.writeUInt16LE(16, 34);
  buf.write('data', 36);
  buf.writeUInt32LE(samples * 2, 40);
  return buf;
}
