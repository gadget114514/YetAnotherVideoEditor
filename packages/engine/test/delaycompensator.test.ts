import { describe, it, expect } from 'vitest';
import { DelayCompensator, DelayLine } from '../src/audio/DelayCompensator.js';

describe('TestDelayCompensator', () => {
  it('computeBasic', () => {
    // 3 tracks: latency 0 / 512 / 128
    const chains = [0, 512, 128];
    const compensation: number[] = [];
    const total = DelayCompensator.computeForChains(chains, 0, compensation);

    // Track with maximum latency gets 0 compensation, others get the difference
    expect(total).toBe(512);
    expect(compensation.length).toBe(3);
    expect(compensation[0]).toBe(512);
    expect(compensation[1]).toBe(0);
    expect(compensation[2]).toBe(384);
  });

  it('zeroLatency', () => {
    const compensation: number[] = [];
    const total = DelayCompensator.computeForChains([0, 0], 0, compensation);
    expect(total).toBe(0);
    expect(compensation[0]).toBe(0);

    // Master chain latency is added to the total
    const total2 = DelayCompensator.computeForChains([0, 0], 256);
    expect(total2).toBe(256);
  });

  it('masterChainOnly', () => {
    const compensation: number[] = [];
    // All tracks have latency 0, master has 100
    const total = DelayCompensator.computeForChains([0, 0, 0], 100, compensation);
    expect(total).toBe(100);
    for (const c of compensation) {
      expect(c).toBe(0);
    }
  });

  it('delayLineRoundTrip', () => {
    // Delay 4 samples: input pulse appears 4 samples later
    const line = new DelayLine();
    line.prepare(2, 8); // Capacity 8 (rounded up to power of 2)
    line.setDelay(4);

    const left = new Float32Array(16);
    const right = new Float32Array(16);
    left[0] = 1.0; // Impulse

    const buf = [left, right];
    line.process(buf, 2, 16);

    expect(left[4]).toBe(1.0);
    expect(left[0]).toBe(0.0);
    expect(right[4]).toBe(0.0); // Right channel had silence input

    // Continual processing preserves ring integrity
    line.process(buf, 2, 16);
    expect(Number.isFinite(left[15])).toBe(true);
  });

  it('delayLinePowerOfTwoCapacity', () => {
    // Capacity rounded up to power of two
    const line = new DelayLine();
    line.prepare(1, 10); // Requested 10 -> Actual capacity 16
    expect(line.capacity).toBe(16);
    line.setDelay(10);

    const inBuf = new Float32Array(32);
    for (let i = 0; i < 32; ++i) {
      inBuf[i] = i;
    }
    const buf = [inBuf];
    line.process(buf, 1, 32);

    // Delayed by 10 samples
    for (let i = 10; i < 32; ++i) {
      expect(inBuf[i]).toBe(i - 10);
    }
    for (let i = 0; i < 10; ++i) {
      expect(inBuf[i]).toBe(0);
    }
  });
});
