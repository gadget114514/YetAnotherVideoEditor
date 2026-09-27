import { AudioClock } from './AudioClock.js';
import { type AudioRenderGraph } from './AudioRenderGraph.js';
import { renderAudioBlock } from './AudioRenderer.js';

export class AudioEngine {
  private ctx_: AudioContext | null = null;
  private clock_: AudioClock = new AudioClock(48000);
  private isPlaying_: boolean = false;
  private activeGraph_: AudioRenderGraph | null = null;
  private scriptNode_: ScriptProcessorNode | null = null;
  private gainNode_: GainNode | null = null;

  constructor() {
    // AudioContext will be initialized on first user interaction
  }

  get clock(): AudioClock {
    return this.clock_;
  }

  get sampleRate(): number {
    return this.clock_.sampleRate;
  }

  get isPlaying(): boolean {
    return this.isPlaying_;
  }

  async ensureContext(): Promise<AudioContext> {
    if (!this.ctx_ && typeof AudioContext !== 'undefined') {
      this.ctx_ = new AudioContext({ sampleRate: 48000 });
      this.clock_.setAudioContext(this.ctx_);

      this.gainNode_ = this.ctx_.createGain();
      this.gainNode_.connect(this.ctx_.destination);

      // Create realtime playback pump (using ScriptProcessorNode or AudioWorklet fallback)
      const bufferSize = 1024;
      this.scriptNode_ = this.ctx_.createScriptProcessor(bufferSize, 0, 2);
      this.scriptNode_.onaudioprocess = (e) => {
        if (!this.isPlaying_ || !this.activeGraph_) {
          for (let c = 0; c < e.outputBuffer.numberOfChannels; c++) {
            e.outputBuffer.getChannelData(c).fill(0);
          }
          return;
        }

        const outChannels = e.outputBuffer.numberOfChannels;
        const numFrames = e.outputBuffer.length;
        const blockStart = this.clock_.rawPlayedPosition();

        const rendered = renderAudioBlock(this.activeGraph_, blockStart, numFrames, outChannels);
        for (let c = 0; c < outChannels; c++) {
          e.outputBuffer.getChannelData(c).set(rendered[c]!);
        }

        this.clock_.advance(numFrames);
      };

      this.scriptNode_.connect(this.gainNode_);
    }

    if (this.ctx_ && this.ctx_.state === 'suspended') {
      await this.ctx_.resume();
    }

    return this.ctx_!;
  }

  setGraph(graph: AudioRenderGraph): void {
    this.activeGraph_ = graph;
  }

  setVolume(volume: number): void {
    if (this.gainNode_) {
      this.gainNode_.gain.value = Math.max(0, Math.min(1, volume));
    }
  }

  async play(): Promise<void> {
    if (this.isPlaying_) return;
    await this.ensureContext();
    this.isPlaying_ = true;
    this.clock_.start();
  }

  pause(): void {
    if (!this.isPlaying_) return;
    this.isPlaying_ = false;
    this.clock_.pause();
  }

  seek(samples: number): void {
    this.clock_.seek(samples);
  }

  dispose(): void {
    this.pause();
    if (this.scriptNode_) {
      this.scriptNode_.disconnect();
      this.scriptNode_ = null;
    }
    if (this.gainNode_) {
      this.gainNode_.disconnect();
      this.gainNode_ = null;
    }
    if (this.ctx_) {
      this.ctx_.close();
      this.ctx_ = null;
    }
  }
}
