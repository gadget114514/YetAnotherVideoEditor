import { describe, it, expect } from 'vitest';
import { Rational, Timebase, RoundMode, secondsToFrames, framesToSeconds, rescaleFrames } from '../src/time/Rational.js';

describe('TestRational', () => {
  it('reduced', () => {
    const r = Rational.reduced(60000, 1001);
    expect(r.num).toBe(60000);
    expect(r.den).toBe(1001);

    // 約分
    const g = Rational.reduced(1920, 1080);
    expect(g.num).toBe(16);
    expect(g.den).toBe(9);

    // 負の分母は正規化される
    const neg = new Rational(1, -2);
    expect(neg.num).toBe(-1);
    expect(neg.den).toBe(2);

    // ゼロ除算の保護 (den=0 は 1 にフォールバック)
    const z = new Rational(5, 0);
    expect(z.den).toBe(1);
  });

  it('comparison', () => {
    expect(new Rational(1, 3).equals(new Rational(2, 6))).toBe(true);
    expect(new Rational(1, 3).equals(new Rational(1, 2))).toBe(false);
    expect(new Rational(1, 3).lessThan(new Rational(1, 2))).toBe(true);
    expect(new Rational(1, 2).greaterThan(new Rational(1, 3))).toBe(true);
    expect(new Rational(-1, 2).lessThan(new Rational(0, 1))).toBe(true);
    expect(new Rational(1, 2).lessOrEqual(new Rational(1, 2))).toBe(true);
  });

  it('overflowComparison', () => {
    const kMaxSafe = Number.MAX_SAFE_INTEGER;
    const a = new Rational(kMaxSafe, Math.floor(kMaxSafe / 2));
    const b = new Rational(kMaxSafe - 2, Math.floor(kMaxSafe / 2));
    expect(a.equals(b)).toBe(false);
    expect(a.greaterThan(b)).toBe(true);

    const big = new Rational(kMaxSafe - 1, kMaxSafe);
    expect(big.lessThan(new Rational(1, 1))).toBe(true);

    const x = new Rational(kMaxSafe, 1);
    const y = new Rational(1, kMaxSafe);
    expect(x.greaterThan(y)).toBe(true);
  });

  it('arithmetic', () => {
    const f30 = new Rational(1, 30);
    const f60 = new Rational(1, 60);

    const sum = f30.add(f60);
    expect(sum.toDouble()).toBeCloseTo(1.0 / 20.0, 6);

    const diff = f30.sub(f60);
    expect(diff.toDouble()).toBeCloseTo(1.0 / 60.0, 6);

    const frameLen = new Rational(1001, 60000);
    const seconds = frameLen.mul(new Rational(60000, 1001));
    expect(seconds.num).toBe(1);
    expect(seconds.den).toBe(1);
  });

  it('secondsToFrames', () => {
    const tb = new Rational(1001, 60000); // 59.94fps

    expect(secondsToFrames((1001.0 / 60000.0) * 10.0, tb, RoundMode.Nearest)).toBe(10);

    const awkward = 10.5 / 59.94;
    expect(secondsToFrames(awkward, tb, RoundMode.Floor)).toBe(
      Math.floor((awkward * tb.den) / tb.num)
    );
  });

  it('framesToSeconds', () => {
    const tb = new Rational(1001, 60000);
    expect(framesToSeconds(0, tb)).toBe(0.0);
    expect(framesToSeconds(60000, tb)).toBe(1001.0);
  });

  it('rescaleFrames', () => {
    const r = rescaleFrames(30, new Rational(1, 30), Timebase.Fps59_94, RoundMode.Nearest);
    expect(Math.abs(r - 59.94)).toBeLessThan(1.0);

    expect(rescaleFrames(12345, Timebase.Fps59_94, Timebase.Fps59_94, RoundMode.Nearest)).toBe(12345);

    const f = rescaleFrames(7, Timebase.Fps24, Timebase.Fps25, RoundMode.Floor);
    const c = rescaleFrames(7, Timebase.Fps24, Timebase.Fps25, RoundMode.Ceil);
    expect(f).toBeLessThanOrEqual(c);
  });

  it('dropFrameTimebase', () => {
    const testBases = [Timebase.Fps23_976, Timebase.Fps29_97, Timebase.Fps59_94];
    for (const tb of testBases) {
      for (const frames of [1, 10, 100, 1001, 60000]) {
        const sec = framesToSeconds(frames, tb);
        const back = secondsToFrames(sec, tb, RoundMode.Nearest);
        expect(back).toBe(frames);
      }
    }
  });
});
