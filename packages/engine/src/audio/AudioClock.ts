import { type RationalData, RationalUtil, secondsToFrames } from '@yave/core';

export class AudioClock {
  private sampleRate_: number = 48000;
  private playedPosition_: number = 0;
  private isRunning_: boolean = false;
  private startTimeContext_: number = 0;
  private audioContext_: AudioContext | null = null;

  constructor(sampleRate: number = 48000) {
    this.sampleRate_ = sampleRate;
  }

  setAudioContext(ctx: AudioContext): void {
    this.audioContext_ = ctx;
    this.sampleRate_ = ctx.sampleRate;
  }

  get sampleRate(): number {
    return this.sampleRate_;
  }

  get isRunning(): boolean {
    return this.isRunning_;
  }

  start(): void {
    if (!this.isRunning_) {
      this.isRunning_ = true;
      if (this.audioContext_) {
        this.startTimeContext_ = this.audioContext_.currentTime;
      }
    }
  }

  pause(): void {
    if (this.isRunning_) {
      if (this.audioContext_) {
        const elapsed = this.audioContext_.currentTime - this.startTimeContext_;
        this.playedPosition_ += Math.round(elapsed * this.sampleRate_);
      }
      this.isRunning_ = false;
    }
  }

  seek(samples: number): void {
    this.playedPosition_ = Math.max(0, Math.trunc(samples));
    if (this.audioContext_ && this.isRunning_) {
      this.startTimeContext_ = this.audioContext_.currentTime;
    }
  }

  seekFrame(frame: number, tb: RationalData): void {
    const f2s = this.sampleRate_ / RationalUtil.toDouble(tb);
    this.seek(Math.round(frame * f2s));
  }

  advance(samples: number): void {
    this.playedPosition_ += Math.max(0, Math.trunc(samples));
  }

  rawPlayedPosition(): number {
    if (!this.isRunning_ || !this.audioContext_) {
      return this.playedPosition_;
    }
    const elapsed = this.audioContext_.currentTime - this.startTimeContext_;
    return this.playedPosition_ + Math.round(elapsed * this.sampleRate_);
  }

  currentAudibleSample(systemLatencySamples: number = 0): number {
    return Math.max(0, this.rawPlayedPosition() - systemLatencySamples);
  }

  currentFrame(tb: RationalData, systemLatencySamples: number = 0): number {
    const sample = this.currentAudibleSample(systemLatencySamples);
    const seconds = sample / this.sampleRate_;
    return secondsToFrames(seconds, tb);
  }
}
