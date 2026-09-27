export type VideoFrameLike = {
  close: () => void;
  clone?: () => VideoFrameLike;
  displayWidth?: number;
  displayHeight?: number;
  timestamp?: number;
};

export class FrameCache {
  private cache_: Map<string, { frame: VideoFrameLike; lastAccessed: number }> = new Map();
  private maxCapacity_: number;
  private accessCounter_: number = 0;
  private openFramesCount_: number = 0;

  constructor(maxCapacity: number = 60) {
    this.maxCapacity_ = maxCapacity;
  }

  get size(): number {
    return this.cache_.size;
  }

  get capacity(): number {
    return this.maxCapacity_;
  }

  setCapacity(capacity: number): void {
    this.maxCapacity_ = Math.max(1, capacity);
    this.evictIfNeeded();
  }

  makeKey(assetId: string, frameIndex: number): string {
    return `${assetId}:${frameIndex}`;
  }

  get(assetId: string, frameIndex: number): VideoFrameLike | null {
    const key = this.makeKey(assetId, frameIndex);
    const entry = this.cache_.get(key);
    if (!entry) {
      return null;
    }
    entry.lastAccessed = ++this.accessCounter_;
    return entry.frame;
  }

  put(assetId: string, frameIndex: number, frame: VideoFrameLike): void {
    const key = this.makeKey(assetId, frameIndex);
    const existing = this.cache_.get(key);
    if (existing) {
      if (existing.frame !== frame) {
        this.safeClose(existing.frame);
      }
    } else {
      this.openFramesCount_++;
    }

    this.cache_.set(key, {
      frame,
      lastAccessed: ++this.accessCounter_,
    });

    this.evictIfNeeded();
  }

  has(assetId: string, frameIndex: number): boolean {
    return this.cache_.has(this.makeKey(assetId, frameIndex));
  }

  clear(): void {
    for (const entry of this.cache_.values()) {
      this.safeClose(entry.frame);
    }
    this.cache_.clear();
  }

  private evictIfNeeded(): void {
    while (this.cache_.size > this.maxCapacity_) {
      // Find oldest accessed
      let oldestKey: string | null = null;
      let oldestTime = Infinity;

      for (const [k, v] of this.cache_.entries()) {
        if (v.lastAccessed < oldestTime) {
          oldestTime = v.lastAccessed;
          oldestKey = k;
        }
      }

      if (oldestKey) {
        const item = this.cache_.get(oldestKey);
        if (item) {
          this.safeClose(item.frame);
        }
        this.cache_.delete(oldestKey);
      } else {
        break;
      }
    }
  }

  private safeClose(frame: VideoFrameLike): void {
    try {
      if (typeof frame.close === 'function') {
        frame.close();
      }
    } catch {
      // ignore
    } finally {
      this.openFramesCount_ = Math.max(0, this.openFramesCount_ - 1);
    }
  }
}
