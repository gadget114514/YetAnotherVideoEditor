import { describe, it, expect } from 'vitest';
import { Timebase } from '../src/time/Rational.js';
import { formatTimecode, parseTimecode } from '../src/time/Timecode.js';
import {
  createTimeRange,
  timeRangeEnd,
  timeRangeContainsFrame,
  timeRangeContainsRange,
  timeRangesIntersect,
} from '../src/time/TimeRange.js';

describe('Timecode', () => {
  it('formatTimecode non-drop frame', () => {
    // 30fps
    const tb30 = Timebase.Fps30;
    expect(formatTimecode(0, tb30)).toBe('00:00:00:00');
    expect(formatTimecode(30, tb30)).toBe('00:00:01:00');
    expect(formatTimecode(3661 * 30 + 15, tb30)).toBe('01:01:01:15');

    // Negative frame safety check
    expect(formatTimecode(-10, tb30)).toBe('00:00:00:00');
  });

  it('parseTimecode non-drop frame', () => {
    const tb30 = Timebase.Fps30;
    expect(parseTimecode('00:00:00:00', tb30)).toBe(0);
    expect(parseTimecode('00:00:01:00', tb30)).toBe(30);
    expect(parseTimecode('01:01:01:15', tb30)).toBe(3661 * 30 + 15);
    expect(parseTimecode('invalid', tb30)).toBeNull();
  });

  it('formatTimecode and parseTimecode drop-frame (30fps & 60fps)', () => {
    const tb30 = Timebase.Fps29_97;
    const tc30 = formatTimecode(1800, tb30, true);
    expect(tc30).toContain(';');
    expect(parseTimecode(tc30, tb30)).toBe(1800);

    const tb60 = Timebase.Fps59_94;
    const tc60 = formatTimecode(3600, tb60, true);
    expect(tc60).toContain(';');
    expect(parseTimecode(tc60, tb60)).toBe(3600);
  });
});

describe('TimeRange', () => {
  it('createTimeRange and timeRangeEnd', () => {
    const range = createTimeRange(10, 50);
    expect(range.start).toBe(10);
    expect(range.duration).toBe(50);
    expect(timeRangeEnd(range)).toBe(60);

    // Negative duration protection
    const negRange = createTimeRange(10, -5);
    expect(negRange.duration).toBe(0);
  });

  it('timeRangeContainsFrame', () => {
    const range = createTimeRange(10, 20); // 10..30
    expect(timeRangeContainsFrame(range, 9)).toBe(false);
    expect(timeRangeContainsFrame(range, 10)).toBe(true);
    expect(timeRangeContainsFrame(range, 29)).toBe(true);
    expect(timeRangeContainsFrame(range, 30)).toBe(false);
  });

  it('timeRangeContainsRange', () => {
    const outer = createTimeRange(10, 50); // 10..60
    const inner = createTimeRange(20, 10); // 20..30
    const edge = createTimeRange(10, 50); // 10..60
    const exceeding = createTimeRange(20, 50); // 20..70

    expect(timeRangeContainsRange(outer, inner)).toBe(true);
    expect(timeRangeContainsRange(outer, edge)).toBe(true);
    expect(timeRangeContainsRange(outer, exceeding)).toBe(false);
  });

  it('timeRangesIntersect', () => {
    const a = createTimeRange(10, 20); // 10..30
    const b = createTimeRange(20, 20); // 20..40
    const c = createTimeRange(30, 20); // 30..50

    expect(timeRangesIntersect(a, b)).toBe(true);
    expect(timeRangesIntersect(a, c)).toBe(false); // Adjacent is not intersecting
  });
});
