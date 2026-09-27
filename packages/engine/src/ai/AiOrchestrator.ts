import {
  Project,
  VideoClip,
  AudioClip,
  AiPlaceholderClip,
  createUuid,
  type Uuid,
} from '@yave/core';

export type GenerationKind = 'video' | 'audio' | 'subtitle' | 'image';
export type AiTaskStatus = 'queued' | 'running' | 'cached' | 'committed' | 'failed' | 'cancelled';

export interface AiGenerationParams {
  readonly kind: GenerationKind;
  readonly modelId?: string;
  readonly providerId?: string;
  readonly prompt: string;
  readonly durationFrames?: number;
  readonly extraParams?: Record<string, any>;
}

export interface AiTaskRecord {
  readonly id: Uuid;
  readonly params: AiGenerationParams;
  status: AiTaskStatus;
  progress: number;
  outputAssetId?: Uuid;
  error?: string;
}

export interface IGenerationProvider {
  readonly id: string;
  readonly name: string;
  generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<{ assetId: Uuid; mediaType: GenerationKind }>;
}

export class MockGenerationProvider implements IGenerationProvider {
  readonly id = 'mock-provider';
  readonly name = 'Mock AI Generator';

  async generate(
    params: AiGenerationParams,
    onProgress: (progress: number) => void,
    signal?: AbortSignal
  ): Promise<{ assetId: Uuid; mediaType: GenerationKind }> {
    const steps = 5;
    for (let i = 1; i <= steps; i++) {
      if (signal?.aborted) {
        throw new Error('Generation cancelled');
      }
      onProgress(i / steps);
      await new Promise((r) => setTimeout(r, 50));
    }

    return {
      assetId: createUuid(),
      mediaType: params.kind,
    };
  }
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

  getTask(taskId: string): AiTaskRecord | null {
    return this.tasks_.get(taskId) ?? null;
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
      placeholder.setName(params.prompt.slice(0, 30));
      placeholder.prompt = params.prompt;
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
      audioClip.setName(task.params.prompt.slice(0, 30));
      track.insertClip(audioClip);
    } else {
      const videoClip = new VideoClip(task.outputAssetId);
      videoClip.setRange(range);
      videoClip.setName(task.params.prompt.slice(0, 30));
      track.insertClip(videoClip);
    }

    task.status = 'committed';
    return true;
  }
}
