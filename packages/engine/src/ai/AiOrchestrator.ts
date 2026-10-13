import {
  Project,
  VideoClip,
  AudioClip,
  SubtitleClip,
  AiPlaceholderClip,
  Track,
  createUuid,
  type Uuid,
  secondsToFrames,
  RoundMode,
} from '@yave/core';
import { SrtParser, convertCuesToClips } from '@yave/io';
import { alignLyrics, type LyricsAlignmentResult } from './LyricsAligner.js';
import { MediaProbe } from '../media/MediaProbe.js';

export type GenerationKind = 'video' | 'audio' | 'subtitle' | 'image' | 'stt';
export type AiTaskStatus = 'queued' | 'running' | 'cached' | 'committed' | 'failed' | 'cancelled';

export interface AiGenerationParams {
  readonly kind: GenerationKind;
  readonly modelId?: string;
  readonly providerId?: string;
  readonly prompt?: string;
  readonly durationFrames?: number;
  readonly audioAssetId?: Uuid | string;
  readonly audioFile?: File | Blob | ArrayBuffer | Uint8Array;
  readonly audioFileName?: string;
  readonly language?: string;
  /** Correct lyrics (one subtitle per line). Recognised text is replaced by these lines, re-timed to the audio. */
  readonly lyrics?: string;
  readonly extraParams?: Record<string, any>;
}

export interface AiTaskResult {
  readonly assetId: Uuid;
  readonly mediaType: GenerationKind;
  readonly srtContent?: string;
  readonly data?: any;
}

export interface AiTaskRecord {
  readonly id: Uuid;
  readonly params: AiGenerationParams;
  status: AiTaskStatus;
  progress: number;
  outputAssetId?: Uuid;
  srtContent?: string;
  generatedClips?: SubtitleClip[];
  lyricsAlignment?: LyricsAlignmentResult;
  error?: string;
}

export interface IGenerationProvider {
  readonly id: string;
  readonly name: string;
  generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<AiTaskResult>;
}

export class MockGenerationProvider implements IGenerationProvider {
  readonly id = 'mock-provider';
  readonly name = 'Mock AI Generator';

  async generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<AiTaskResult> {
    const steps = 5;
    for (let i = 1; i <= steps; i++) {
      if (signal?.aborted) {
        throw new Error('Generation cancelled');
      }
      onProgress(i / steps);
      await new Promise((r) => setTimeout(r, 40));
    }

    let srtContent: string | undefined;
    if (params.kind === 'subtitle' || params.kind === 'stt') {
      srtContent = this.generateMockSrt(params);
    }

    return {
      assetId: createUuid(),
      mediaType: params.kind,
      srtContent,
    };
  }

  private generateMockSrt(params: AiGenerationParams): string {
    const durationFrames = params.durationFrames ?? 360;
    const totalSec = Math.max(1, durationFrames / 60);

    const formatSrtTime = (seconds: number): string => {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = Math.floor(seconds % 60);
      const ms = Math.floor((seconds % 1) * 1000);
      const pad = (n: number, w: number = 2) => String(n).padStart(w, '0');
      return `${pad(h)}:${pad(m)}:${pad(s)},${pad(ms, 3)}`;
    };

    let texts: string[] = [];
    if (params.prompt && !params.prompt.startsWith('Transcribe ')) {
      texts = params.prompt.split('\n').map((s) => s.trim()).filter((s) => s.length > 0);
    }
    if (texts.length === 0) {
      if (params.language === 'en') {
        texts = [
          'Audio data loaded successfully into audio track.',
          'AI speech recognition generated SRT subtitles.',
          'Subtitles imported into subtitle track seamlessly.',
        ];
      } else {
        texts = [
          '音声データを読み込み、音声トラックを配置しました。',
          'AIが音声を解析し、SRT字幕を自動生成しました。',
          '字幕トラックとしてタイムラインに取り込みました。',
        ];
      }
    }

    const cueCount = Math.min(texts.length, Math.max(1, Math.floor(totalSec / 2.5)));
    const actualTexts = texts.slice(0, cueCount);
    const cueDuration = Math.min(3.0, Math.max(1.0, (totalSec - 0.2) / cueCount));

    const cues: string[] = [];
    for (let i = 0; i < actualTexts.length; i++) {
      const start = 0.3 + i * (cueDuration + 0.3);
      const end = Math.min(totalSec, start + cueDuration);
      cues.push(`${i + 1}\n${formatSrtTime(start)} --> ${formatSrtTime(end)}\n${actualTexts[i]}`);
    }

    return cues.join('\n\n') + '\n';
  }
}

export interface WhisperProviderOptions {
  apiKey?: string;
  baseUrl?: string;
  model?: string;
  fetchFn?: typeof fetch;
}

export class OpenAiWhisperProvider implements IGenerationProvider {
  readonly id = 'openai-whisper';
  readonly name = 'OpenAI Whisper (STT)';
  private options_: WhisperProviderOptions;

  constructor(options: WhisperProviderOptions = {}) {
    this.options_ = options;
  }

  async generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<AiTaskResult> {
    const apiKey = this.options_.apiKey;
    const baseUrl = this.options_.baseUrl ?? 'https://api.openai.com/v1';
    const fetchFn = this.options_.fetchFn ?? (typeof fetch !== 'undefined' ? fetch : undefined);

    if (!apiKey && !this.options_.baseUrl) {
      throw new Error('OpenAI API key or custom endpoint is required for Whisper STT.');
    }

    onProgress(0.1);

    const formData = new FormData();
    if (params.audioFile instanceof Blob || (typeof File !== 'undefined' && params.audioFile instanceof File)) {
      formData.append('file', params.audioFile, params.audioFileName || 'audio.wav');
    } else if (params.audioFile instanceof Uint8Array || params.audioFile instanceof ArrayBuffer) {
      const blob = new Blob([params.audioFile as any], { type: 'audio/wav' });
      formData.append('file', blob, params.audioFileName || 'audio.wav');
    } else {
      throw new Error('No audio file data provided for Whisper transcription.');
    }

    formData.append('model', this.options_.model ?? 'whisper-1');
    formData.append('response_format', 'srt');
    if (params.language) {
      formData.append('language', params.language);
    }
    if (params.prompt && !params.prompt.startsWith('Transcribe ')) {
      formData.append('prompt', params.prompt);
    }

    onProgress(0.3);

    const headers: Record<string, string> = {};
    if (apiKey) {
      headers['Authorization'] = `Bearer ${apiKey}`;
    }

    if (!fetchFn) {
      throw new Error('Fetch API is not available');
    }

    const res = await fetchFn(`${baseUrl}/audio/transcriptions`, {
      method: 'POST',
      headers,
      body: formData,
      signal,
    });

    if (!res.ok) {
      const errText = await res.text();
      throw new Error(`Whisper API request failed (${res.status}): ${errText}`);
    }

    onProgress(0.9);
    const srtContent = await res.text();
    onProgress(1.0);

    return {
      assetId: createUuid(),
      mediaType: 'subtitle',
      srtContent,
    };
  }
}

export interface ImportAudioAndTranscribeOptions {
  project: Project;
  audioName: string;
  audioData?: ArrayBuffer | Uint8Array | Blob | File;
  durationFrames?: number;
  audioTrackId?: string;
  subtitleTrackId?: string;
  startFrame?: number;
  prompt?: string;
  /** Correct lyrics; recognised subtitles are replaced by these lines with matched timing. */
  lyrics?: string;
  language?: string;
  providerId?: string;
  extraParams?: Record<string, any>;
  onProgress?: (progress: number) => void;
}

export interface ImportAudioAndTranscribeResult {
  audioAssetId: Uuid;
  audioTrack: Track;
  audioClip: AudioClip;
  subtitleTrack: Track;
  subtitleClips: SubtitleClip[];
  srtContent: string;
  lyricsAlignment?: LyricsAlignmentResult;
  task: AiTaskRecord;
}

export class AiOrchestrator {
  private providers_: Map<string, IGenerationProvider> = new Map();
  private tasks_: Map<string, AiTaskRecord> = new Map();
  private defaultProvider_: IGenerationProvider;

  constructor(defaultProvider?: IGenerationProvider) {
    this.defaultProvider_ = defaultProvider ?? new MockGenerationProvider();
    this.registerProvider(this.defaultProvider_);
  }

  registerProvider(provider: IGenerationProvider): void {
    this.providers_.set(provider.id, provider);
  }

  getProvider(providerId?: string): IGenerationProvider {
    return (providerId ? this.providers_.get(providerId) : undefined) ?? this.defaultProvider_;
  }

  getTask(taskId: string): AiTaskRecord | null {
    return this.tasks_.get(taskId) ?? null;
  }

  async waitForTask(taskId: string): Promise<AiTaskRecord> {
    const task = this.getTask(taskId);
    if (!task) return Promise.reject(new Error(`Task not found: ${taskId}`));
    if (task.status === 'committed' || task.status === 'cached' || task.status === 'failed' || task.status === 'cancelled') {
      return Promise.resolve(task);
    }
    return new Promise((resolve, reject) => {
      const check = () => {
        const t = this.getTask(taskId);
        if (!t) return reject(new Error(`Task not found: ${taskId}`));
        if (t.status === 'committed' || t.status === 'cached') {
          resolve(t);
        } else if (t.status === 'failed') {
          reject(new Error(t.error ?? 'Generation failed'));
        } else if (t.status === 'cancelled') {
          reject(new Error('Generation cancelled'));
        } else {
          setTimeout(check, 20);
        }
      };
      setTimeout(check, 20);
    });
  }

  async submitTask(
    params: AiGenerationParams,
    project: Project,
    trackId: string,
    startFrame: number
  ): Promise<AiTaskRecord> {
    const taskId = createUuid();
    const task: AiTaskRecord = {
      id: taskId,
      params,
      status: 'queued',
      progress: 0,
    };
    this.tasks_.set(taskId, task);

    // Place placeholder on the track
    const track = project.timeline.trackById(trackId);
    let placeholder: AiPlaceholderClip | null = null;
    if (track) {
      placeholder = new AiPlaceholderClip(taskId);
      placeholder.setRange({
        start: startFrame,
        duration: params.durationFrames ?? 180, // default 3 seconds
      });
      const promptName = params.prompt ?? (params.kind === 'subtitle' ? 'AI Subtitles' : 'AI Asset');
      placeholder.setName(promptName.slice(0, 30));
      placeholder.prompt = promptName;
      track.insertClip(placeholder);
    }

    // Run task in background
    this.runTask(task, project, trackId, placeholder);

    return task;
  }

  private async runTask(
    task: AiTaskRecord,
    project: Project,
    trackId: string,
    placeholder: AiPlaceholderClip | null
  ): Promise<void> {
    task.status = 'running';

    const provider = this.providers_.get(task.params.providerId ?? '') ?? this.defaultProvider_;

    try {
      const result = await provider.generate(
        task.params,
        (progress) => {
          task.progress = progress;
          if (placeholder) {
            placeholder.progress = progress;
          }
        }
      );

      task.status = 'cached';
      task.outputAssetId = result.assetId;
      task.srtContent = result.srtContent;
      if (task.params.lyrics && task.params.lyrics.trim() && (task.params.kind === 'subtitle' || task.params.kind === 'stt')) {
        // Prefer token-level timing (forced alignment resolution); fall back to subtitle cues.
        const tokens: Array<{ text: string; start: number; end: number }> = Array.isArray(result.data?.tokens)
          ? result.data.tokens
          : [];
        const parsed = result.srtContent ? SrtParser.parseText(result.srtContent) : null;
        const cues =
          tokens.length > 0
            ? tokens.map((t) => ({ startSeconds: t.start, endSeconds: t.end, text: t.text }))
            : (parsed?.cues ?? []).map((c) => ({
                startSeconds: c.startSeconds,
                endSeconds: c.endSeconds,
                text: c.rawText,
              }));
        const alignment = alignLyrics(cues, task.params.lyrics);
        task.lyricsAlignment = alignment;
        task.srtContent = alignment.srt;
      }
      task.progress = 1.0;

      // Commit to real clip
      this.commitTask(task, project, trackId, placeholder);
    } catch (err: any) {
      task.status = 'failed';
      task.error = err?.message ?? 'Generation failed';
    }
  }

  commitTask(
    task: AiTaskRecord,
    project: Project,
    trackId: string,
    placeholder: AiPlaceholderClip | null
  ): boolean {
    if (!task.outputAssetId) return false;
    const track = project.timeline.trackById(trackId);
    if (!track) return false;

    let range = { start: 0, duration: 180 };
    if (placeholder) {
      range = { ...placeholder.range };
      track.removeClip(placeholder.id);
    }

    if (task.params.kind === 'audio') {
      const audioClip = new AudioClip(task.outputAssetId);
      audioClip.setRange(range);
      audioClip.setName(task.params.prompt?.slice(0, 30) || 'Audio');
      track.insertClip(audioClip);
    } else if (task.params.kind === 'subtitle' || task.params.kind === 'stt') {
      if (task.srtContent) {
        const parsed = SrtParser.parseText(task.srtContent);
        const clips = convertCuesToClips(parsed, project.timeline.timebase);
        const inserted: SubtitleClip[] = [];
        for (const clip of clips) {
          clip.setRange({
            start: clip.range.start + range.start,
            duration: clip.range.duration,
          });
          track.insertClip(clip);
          inserted.push(clip);
        }
        task.generatedClips = inserted;
      } else {
        const subClip = new SubtitleClip();
        subClip.setRange(range);
        subClip.setPlainText(task.params.prompt || 'Generated Subtitle');
        track.insertClip(subClip);
        task.generatedClips = [subClip];
      }
    } else {
      const videoClip = new VideoClip(task.outputAssetId);
      videoClip.setRange(range);
      videoClip.setName(task.params.prompt?.slice(0, 30) || 'Video');
      track.insertClip(videoClip);
    }

    task.status = 'committed';
    return true;
  }

  /**
   * Imports an audio file, places it on an audio track, requests SRT generation
   * from AI, and imports the generated cues into a subtitle track.
   */
  async importAudioAndTranscribe(
    options: ImportAudioAndTranscribeOptions
  ): Promise<ImportAudioAndTranscribeResult> {
    const { project, audioName } = options;

    // 1. Register audio asset in project
    const asset = project.assets.registerAsset(audioName, 'audio');

    // 2. Determine duration in frames
    let duration = options.durationFrames;
    if (!duration && options.audioData) {
      if (options.audioData instanceof Uint8Array || options.audioData instanceof ArrayBuffer) {
        const info = MediaProbe.probe(options.audioData);
        if (info.ok && info.hasAudio && info.audioSampleRate > 0) {
          const sec = info.audioDurationFrames / info.audioSampleRate;
          duration = secondsToFrames(sec, project.timeline.timebase, RoundMode.Ceil);
        }
      }
    }
    if (!duration || duration <= 0) {
      duration = 360; // default 6s
    }
    asset.durationFrames = duration;

    // 3. Audio track & clip
    let audioTrack: Track;
    if (options.audioTrackId) {
      audioTrack = project.timeline.trackById(options.audioTrackId) ?? project.timeline.appendTrack('audio');
    } else {
      const existing = project.timeline.tracksOfType('audio');
      audioTrack = existing[0] ?? project.timeline.appendTrack('audio', `Audio (${audioName})`);
    }

    const startFrame = options.startFrame ?? audioTrack.contentDuration();
    const audioClip = new AudioClip(asset.id);
    audioClip.setName(audioName);
    audioClip.setRange({ start: startFrame, duration });
    audioTrack.insertClip(audioClip);

    // 4. Subtitle track
    let subtitleTrack: Track;
    if (options.subtitleTrackId) {
      subtitleTrack = project.timeline.trackById(options.subtitleTrackId) ?? project.timeline.appendTrack('subtitle');
    } else {
      const existing = project.timeline.tracksOfType('subtitle');
      subtitleTrack = existing[0] ?? project.timeline.appendTrack('subtitle', `Subtitles (${audioName})`);
    }

    // 5. Submit AI STT task on the subtitle track
    const task = await this.submitTask(
      {
        kind: 'subtitle',
        // Lyrics double as a vocabulary hint for the recogniser (Whisper prompts are short, so truncate).
        prompt: options.prompt ?? (options.lyrics?.trim() ? options.lyrics.trim().slice(0, 180) : `Transcribe ${audioName}`),
        durationFrames: duration,
        audioAssetId: asset.id,
        audioFile: options.audioData,
        audioFileName: audioName,
        language: options.language,
        lyrics: options.lyrics,
        providerId: options.providerId,
        extraParams: options.extraParams,
      },
      project,
      subtitleTrack.id,
      startFrame
    );

    // Wait for task to commit
    await this.waitForTask(task.id);

    return {
      audioAssetId: asset.id,
      audioTrack,
      audioClip,
      subtitleTrack,
      subtitleClips: task.generatedClips ?? [],
      srtContent: task.srtContent ?? '',
      lyricsAlignment: task.lyricsAlignment,
      task,
    };
  }
}

