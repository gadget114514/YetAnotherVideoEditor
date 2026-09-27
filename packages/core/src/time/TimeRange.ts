export interface TimeRange {
  readonly start: number;
  readonly duration: number;
}

export function createTimeRange(start: number, duration: number): TimeRange {
  return { start: Math.trunc(start), duration: Math.max(0, Math.trunc(duration)) };
}

export function timeRangeEnd(r: TimeRange): number {
  return r.start + r.duration;
}

export function timeRangeContainsFrame(r: TimeRange, frame: number): boolean {
  return frame >= r.start && frame < r.start + r.duration;
}

export function timeRangeContainsRange(outer: TimeRange, inner: TimeRange): boolean {
  return inner.start >= outer.start && timeRangeEnd(inner) <= timeRangeEnd(outer);
}

export function timeRangesIntersect(a: TimeRange, b: TimeRange): boolean {
  return a.start < timeRangeEnd(b) && b.start < timeRangeEnd(a);
}
