import { describe, it, expect, vi } from 'vitest';
import { FrameCache, type VideoFrameLike } from '../src/media/FrameCache.js';

describe('FrameCache', () => {
  const createMockFrame = (): VideoFrameLike & { closed: boolean } => {
    const mock = {
      closed: false,
      close() {
        mock.closed = true;
      },
    };
    return mock;
  };

  it('basic put, get, has, size and capacity', () => {
    const cache = new FrameCache(10);
    expect(cache.capacity).toBe(10);
    expect(cache.size).toBe(0);

    const frame1 = createMockFrame();
    cache.put('asset1', 0, frame1);

    expect(cache.size).toBe(1);
    expect(cache.has('asset1', 0)).toBe(true);
    expect(cache.has('asset1', 1)).toBe(false);
    expect(cache.get('asset1', 0)).toBe(frame1);
    expect(cache.get('asset1', 1)).toBeNull();
  });

  it('overwriting key closes existing frame', () => {
    const cache = new FrameCache(5);
    const frame1 = createMockFrame();
    const frame2 = createMockFrame();

    cache.put('asset1', 0, frame1);
    expect(frame1.closed).toBe(false);

    cache.put('asset1', 0, frame2);
    expect(frame1.closed).toBe(true);
    expect(frame2.closed).toBe(false);
    expect(cache.get('asset1', 0)).toBe(frame2);
  });

  it('evicts LRU frame when capacity is exceeded', () => {
    const cache = new FrameCache(3);
    const f1 = createMockFrame();
    const f2 = createMockFrame();
    const f3 = createMockFrame();
    const f4 = createMockFrame();

    cache.put('asset', 1, f1);
    cache.put('asset', 2, f2);
    cache.put('asset', 3, f3);

    // Access f1 so f2 becomes the LRU
    cache.get('asset', 1);

    // Put f4 -> should evict f2
    cache.put('asset', 4, f4);

    expect(cache.size).toBe(3);
    expect(f2.closed).toBe(true);
    expect(f1.closed).toBe(false);
    expect(f3.closed).toBe(false);
    expect(f4.closed).toBe(false);

    expect(cache.has('asset', 2)).toBe(false);
    expect(cache.has('asset', 1)).toBe(true);
  });

  it('resizing capacity evicts surplus items', () => {
    const cache = new FrameCache(5);
    const frames = Array.from({ length: 5 }, () => createMockFrame());
    frames.forEach((f, idx) => cache.put('asset', idx, f));

    expect(cache.size).toBe(5);

    // Resize capacity to 2
    cache.setCapacity(2);
    expect(cache.capacity).toBe(2);
    expect(cache.size).toBe(2);

    // First 3 should have been evicted and closed
    expect(frames[0]!.closed).toBe(true);
    expect(frames[1]!.closed).toBe(true);
    expect(frames[2]!.closed).toBe(true);
    expect(frames[3]!.closed).toBe(false);
    expect(frames[4]!.closed).toBe(false);
  });

  it('clear closes all cached frames and resets size', () => {
    const cache = new FrameCache(5);
    const f1 = createMockFrame();
    const f2 = createMockFrame();

    cache.put('asset', 1, f1);
    cache.put('asset', 2, f2);

    cache.clear();

    expect(cache.size).toBe(0);
    expect(f1.closed).toBe(true);
    expect(f2.closed).toBe(true);
  });
});
