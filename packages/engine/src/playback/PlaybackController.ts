import { Project, type RenderSnapshot, RationalUtil } from '@yave/core';
import { AudioEngine } from '../audio/AudioEngine.js';
import { AudioRenderGraphBuilder } from '../audio/AudioRenderGraph.js';
import { WebGlCompositor, type TextureProvider } from '../render/WebGlCompositor.js';
import { FrameCache } from '../media/FrameCache.js';

export class PlaybackController {
  private project_: Project;
  private audioEngine_: AudioEngine = new AudioEngine();
  private compositor_: WebGlCompositor | null = null;
  private frameCache_: FrameCache = new FrameCache(60);

  private isPlaying_: boolean = false;
  private currentFrame_: number = 0;
  private animFrameId_: number | null = null;
  private canvas_: HTMLCanvasElement | null = null;
  private frameListeners_: ((frame: number) => void)[] = [];

  constructor(project: Project) {
    this.project_ = project;
  }

  get isPlaying(): boolean {
    return this.isPlaying_;
  }

  get currentFrame(): number {
    return this.currentFrame_;
  }

  get frameCache(): FrameCache {
    return this.frameCache_;
  }

  get audioEngine(): AudioEngine {
    return this.audioEngine_;
  }

  setProject(project: Project): void {
    this.project_ = project;
    this.updateAudioGraph();
    this.renderCurrent();
  }

  attachCanvas(canvas: HTMLCanvasElement): void {
    this.canvas_ = canvas;
    const gl = canvas.getContext('webgl2', {
      alpha: false,
      antialias: false,
      premultipliedAlpha: true,
      preserveDrawingBuffer: false,
    });

    if (gl) {
      const { width, height } = this.project_.canvasSize;
      this.compositor_ = new WebGlCompositor(gl, width, height);
      this.renderCurrent();
    }
  }

  updateAudioGraph(): void {
    const graph = AudioRenderGraphBuilder.build(this.project_.timeline, this.project_);
    this.audioEngine_.setGraph(graph);
  }

  async play(): Promise<void> {
    if (this.isPlaying_) return;
    this.isPlaying_ = true;

    this.updateAudioGraph();
    this.audioEngine_.clock.seekFrame(this.currentFrame_, this.project_.timebase);
    await this.audioEngine_.play();

    this.startLoop();
  }

  pause(): void {
    if (!this.isPlaying_) return;
    this.isPlaying_ = false;
    this.audioEngine_.pause();
    this.stopLoop();
  }

  togglePlay(): void {
    if (this.isPlaying_) {
      this.pause();
    } else {
      this.play();
    }
  }

  seek(frame: number): void {
    this.currentFrame_ = Math.max(0, Math.trunc(frame));
    this.audioEngine_.clock.seekFrame(this.currentFrame_, this.project_.timebase);
    this.notifyFrame();
    this.renderCurrent();
  }

  stepFrames(delta: number): void {
    this.seek(this.currentFrame_ + delta);
  }

  onFrame(cb: (frame: number) => void): () => void {
    this.frameListeners_.push(cb);
    return () => {
      this.frameListeners_ = this.frameListeners_.filter((l) => l !== cb);
    };
  }

  private startLoop(): void {
    const tick = () => {
      if (!this.isPlaying_) return;

      const tb = this.project_.timebase;
      const frame = this.audioEngine_.clock.currentFrame(tb);
      if (frame !== this.currentFrame_) {
        this.currentFrame_ = frame;
        this.notifyFrame();
        this.renderCurrent();
      }

      this.animFrameId_ = requestAnimationFrame(tick);
    };

    this.animFrameId_ = requestAnimationFrame(tick);
  }

  private stopLoop(): void {
    if (this.animFrameId_ !== null) {
      cancelAnimationFrame(this.animFrameId_);
      this.animFrameId_ = null;
    }
  }

  private notifyFrame(): void {
    for (const cb of this.frameListeners_) {
      cb(this.currentFrame_);
    }
  }

  renderCurrent(): void {
    if (!this.compositor_) return;
    const snapshot = this.project_.timeline.buildSnapshot(this.currentFrame_);

    const provider: TextureProvider = {
      getTexture: (layer) => {
        const src = layer.source;
        if (src && src.kind === 'video') {
          return this.frameCache_.get(src.assetId, src.sourceFrameIndex) as any;
        }
        return null;
      },
    };

    this.compositor_.renderFrame(snapshot, provider);
  }

  dispose(): void {
    this.pause();
    this.audioEngine_.dispose();
    if (this.compositor_) {
      this.compositor_.dispose();
      this.compositor_ = null;
    }
    this.frameCache_.clear();
  }
}
