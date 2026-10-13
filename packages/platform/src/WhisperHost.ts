/**
 * Renderer-facing contract for the local Whisper (whisper.cpp) runtime.
 * Implemented by the Electron preload as `window.yaveWhisper`; absent in the web build.
 */

export type WhisperModelId = 'tiny' | 'base' | 'small' | 'medium' | 'large-v3-turbo';

export interface WhisperModelInfo {
  readonly id: WhisperModelId;
  readonly label: string;
  readonly file: string;
  readonly sizeBytes: number;
  readonly note: string;
}

export interface WhisperStatus {
  readonly supported: boolean;
  readonly platform: string;
  readonly installDir: string;
  readonly binaryPath: string | null;
  readonly binarySource: 'managed' | 'path' | 'custom' | null;
  readonly installedModels: WhisperModelId[];
  readonly activeModel: WhisperModelId | null;
  readonly ready: boolean;
  readonly models: WhisperModelInfo[];
  readonly canAutoDownloadBinary: boolean;
}

export interface WhisperInstallRequest {
  modelId: WhisperModelId;
  /** Do not download whisper-cli; use one already on PATH / at `customBinaryPath`. */
  useExistingBinary?: boolean;
  customBinaryPath?: string;
}

export interface WhisperInstallProgress {
  phase: 'binary' | 'extract' | 'model' | 'verify' | 'done';
  progress: number;
  message: string;
}

export interface WhisperTranscribeRequest {
  /** 16 kHz mono 16-bit PCM WAV */
  wav: Uint8Array;
  language?: string;
  prompt?: string;
  model?: WhisperModelId;
}

export interface WhisperToken {
  text: string;
  /** seconds */
  start: number;
  end: number;
}

export interface WhisperTranscribeResult {
  srt: string;
  /** token-level timing (DTW); empty when the binary/model could not provide it */
  tokens: WhisperToken[];
}

export interface WhisperBridge {
  getStatus(): Promise<WhisperStatus>;
  install(req: WhisperInstallRequest): Promise<WhisperStatus>;
  cancelInstall(): Promise<void>;
  selfTest(): Promise<{ ok: boolean; message: string }>;
  transcribe(req: WhisperTranscribeRequest): Promise<WhisperTranscribeResult>;
  cancelTranscribe(): Promise<void>;
  onInstallProgress(cb: (p: WhisperInstallProgress) => void): () => void;
  onTranscribeProgress(cb: (progress: number) => void): () => void;
}

/** Returns the host-provided bridge, or null in the browser build. */
export function getWhisperBridge(): WhisperBridge | null {
  const g = globalThis as { yaveWhisper?: WhisperBridge };
  return g.yaveWhisper ?? null;
}
