import { app, ipcMain, type WebContents } from 'electron';
import { promises as fs } from 'fs';
import os from 'os';
import path from 'path';
import { randomUUID } from 'crypto';
import {
  WHISPER_MODELS,
  dtwPresetFor,
  getModelSpec,
  getWhisperPaths,
  installWhisper,
  listInstalledModels,
  locateBinary,
  makeSilentWav,
  modelPath,
  readConfig,
  transcribeWav,
  type TranscribeResult,
  type WhisperModelId,
} from './whisperInstaller.js';

interface InstallRequest {
  modelId: WhisperModelId;
  useExistingBinary?: boolean;
  customBinaryPath?: string;
}

interface TranscribeRequest {
  wav: Uint8Array | ArrayBuffer;
  language?: string;
  prompt?: string;
  model?: WhisperModelId;
}

function baseDir(): string {
  return app.getPath('userData');
}

async function buildStatus() {
  const paths = getWhisperPaths(baseDir());
  const cfg = await readConfig(paths);
  const binary = await locateBinary(paths, cfg.customBinaryPath);
  const installed = await listInstalledModels(paths);
  const active = cfg.activeModel && installed.includes(cfg.activeModel) ? cfg.activeModel : installed[0] ?? null;
  return {
    supported: true,
    platform: process.platform,
    installDir: paths.root,
    binaryPath: binary?.path ?? null,
    binarySource: binary?.source ?? null,
    installedModels: installed,
    activeModel: active,
    ready: Boolean(binary && active),
    models: WHISPER_MODELS.map((m) => ({
      id: m.id,
      label: m.label,
      file: m.file,
      sizeBytes: m.sizeBytes,
      note: m.note,
    })),
    canAutoDownloadBinary: process.platform === 'win32',
  };
}

async function transcribe(
  req: TranscribeRequest,
  sender: WebContents,
  signal: AbortSignal
): Promise<TranscribeResult> {
  const paths = getWhisperPaths(baseDir());
  const cfg = await readConfig(paths);
  const binary = await locateBinary(paths, cfg.customBinaryPath);
  if (!binary) throw new Error('Whisper is not installed. Run the Whisper setup wizard first.');
  const modelId = req.model ?? cfg.activeModel;
  if (!modelId) throw new Error('No Whisper model selected.');
  const modelFile = modelPath(paths, getModelSpec(modelId).id);
  await fs.access(modelFile);

  const wavPath = path.join(os.tmpdir(), `yave-whisper-${randomUUID()}.wav`);
  await fs.writeFile(wavPath, Buffer.from(req.wav instanceof ArrayBuffer ? new Uint8Array(req.wav) : req.wav));
  try {
    return await transcribeWav({
      binaryPath: binary.path,
      modelFile,
      wavPath,
      language: req.language,
      prompt: req.prompt,
      dtwPreset: dtwPresetFor(getModelSpec(modelId).id),
      signal,
      onProgress: (p) => {
        if (!sender.isDestroyed()) sender.send('whisper:transcribeProgress', p);
      },
    });
  } finally {
    await fs.rm(wavPath, { force: true });
  }
}

export function registerWhisperIpc(): void {
  let installAbort: AbortController | null = null;
  let transcribeAbort: AbortController | null = null;

  ipcMain.handle('whisper:status', () => buildStatus());

  ipcMain.handle('whisper:install', async (event, req: InstallRequest) => {
    getModelSpec(req?.modelId);
    installAbort?.abort();
    installAbort = new AbortController();
    try {
      await installWhisper({
        baseDir: baseDir(),
        modelId: req.modelId,
        useExistingBinary: Boolean(req.useExistingBinary),
        customBinaryPath: typeof req.customBinaryPath === 'string' && req.customBinaryPath ? req.customBinaryPath : undefined,
        signal: installAbort.signal,
        onProgress: (p) => {
          if (!event.sender.isDestroyed()) event.sender.send('whisper:installProgress', p);
        },
      });
    } finally {
      installAbort = null;
    }
    return buildStatus();
  });

  ipcMain.handle('whisper:cancelInstall', () => {
    installAbort?.abort();
  });

  ipcMain.handle('whisper:selfTest', async (event) => {
    const started = Date.now();
    try {
      transcribeAbort = new AbortController();
      await transcribe({ wav: makeSilentWav(1), language: 'en' }, event.sender, transcribeAbort.signal);
      return { ok: true, message: `Whisper ran successfully (${Date.now() - started} ms).` };
    } catch (err: any) {
      return { ok: false, message: String(err?.message ?? err) };
    } finally {
      transcribeAbort = null;
    }
  });

  ipcMain.handle('whisper:transcribe', async (event, req: TranscribeRequest) => {
    transcribeAbort?.abort();
    transcribeAbort = new AbortController();
    try {
      return await transcribe(req, event.sender, transcribeAbort.signal);
    } finally {
      transcribeAbort = null;
    }
  });

  ipcMain.handle('whisper:cancelTranscribe', () => {
    transcribeAbort?.abort();
  });
}
