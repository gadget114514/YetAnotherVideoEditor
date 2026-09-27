/**
 * Delay line with power-of-two circular buffer for plugin delay compensation (PDC).
 */
export class DelayLine {
  private channels_: number = 0;
  private capacity_: number = 0;
  private mask_: number = 0;
  private delay_: number = 0;
  private writePos_: number = 0;
  private buffers_: Float32Array[] = [];

  /**
   * Prepares the delay line with given channel count and minimum capacity.
   * Actual capacity is rounded up to the nearest power of two.
   */
  prepare(channels: number, minCapacity: number): void {
    this.channels_ = Math.max(1, channels);
    let cap = 1;
    while (cap < minCapacity) {
      cap <<= 1;
    }
    this.capacity_ = Math.max(cap, 2);
    this.mask_ = this.capacity_ - 1;
    this.buffers_ = [];
    for (let c = 0; c < this.channels_; c++) {
      this.buffers_.push(new Float32Array(this.capacity_));
    }
    this.writePos_ = 0;
  }

  get capacity(): number {
    return this.capacity_;
  }

  get delay(): number {
    return this.delay_;
  }

  setDelay(samples: number): void {
    this.delay_ = Math.max(0, Math.min(samples, this.capacity_ - 1));
  }

  process(buffers: (Float32Array | number[])[], channels: number, numFrames: number): void {
    if (this.delay_ === 0 || this.capacity_ === 0) {
      return;
    }

    const chCount = Math.min(channels, this.channels_);
    const mask = this.mask_;
    const delay = this.delay_;

    for (let i = 0; i < numFrames; i++) {
      const readPos = (this.writePos_ - delay + this.capacity_) & mask;
      for (let c = 0; c < chCount; c++) {
        const inCh = buffers[c]!;
        const ring = this.buffers_[c]!;
        const inputSample = inCh[i]!;
        const delayedSample = ring[readPos]!;
        ring[this.writePos_] = inputSample;
        inCh[i] = delayedSample;
      }
      this.writePos_ = (this.writePos_ + 1) & mask;
    }
  }

  reset(): void {
    for (const b of this.buffers_) {
      b.fill(0);
    }
    this.writePos_ = 0;
  }
}

/**
 * Plugin Delay Compensation (PDC) calculator.
 */
export class DelayCompensator {
  /**
   * Computes compensation delay for each track chain and returns the total system latency.
   *
   * @param chains Latencies (in samples) for each track effect chain.
   * @param masterLatency Latency of the master effect chain.
   * @param outCompensation Output array to receive per-track compensation delays.
   * @returns Total latency (maxTrackLatency + masterLatency).
   */
  static computeForChains(
    chains: readonly number[],
    masterLatency: number = 0,
    outCompensation?: number[]
  ): number {
    let maxTrackLatency = 0;
    for (const l of chains) {
      if (l > maxTrackLatency) {
        maxTrackLatency = l;
      }
    }

    if (outCompensation) {
      outCompensation.length = 0;
      for (let i = 0; i < chains.length; i++) {
        outCompensation.push(maxTrackLatency - chains[i]!);
      }
    }

    return maxTrackLatency + masterLatency;
  }
}
