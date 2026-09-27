import { createUuid, type Uuid } from '../id/Uuid.js';
import type { TimeRange } from '../time/TimeRange.js';
import { timeRangeEnd, timeRangesIntersect } from '../time/TimeRange.js';
import { BlendMode } from './BlendMode.js';
import type { Clip } from './Clip.js';
import { type Transition, findTransitionDesc } from './Transition.js';

const kUnlimitedHandle = 1 << 30;

function handleAfter(c: Clip): number {
  const maxDur = c.maxDuration();
  if (maxDur < 0) return kUnlimitedHandle;
  const used = c.sourceOffset() + c.range.duration;
  return maxDur > used ? maxDur - used : 0;
}

function handleBefore(c: Clip): number {
  return c.maxDuration() < 0 ? kUnlimitedHandle : c.sourceOffset();
}

export type TrackType =
  | 'video'
  | 'audio'
  | 'subtitle'
  | 'aiGenerated'
  | 'storyboard'
  | 'unknown';

export class Track {
  readonly id: Uuid;
  readonly type: TrackType;
  name: string;
  allowOverlaps: boolean;
  color: string = '#3a5f8a';
  uiHeight: number = 64;
  visible: boolean = true;
  locked: boolean = false;
  muted: boolean = false;
  solo: boolean = false;
  opacity: number = 1.0;
  blendMode: BlendMode = BlendMode.Normal;
  gain: number = 1.0;
  pan: number = 0.0;
  defaultStylePresetId: string = 'default';

  private clips_: Clip[] = [];
  private transitions_: Transition[] = [];

  constructor(type: TrackType, id?: Uuid) {
    this.type = type;
    this.id = id ?? createUuid();
    this.name = type;
    this.allowOverlaps = type === 'subtitle';
  }

  get clips(): readonly Clip[] {
    return this.clips_;
  }

  get transitions(): readonly Transition[] {
    return this.transitions_;
  }

  get clipCount(): number {
    return this.clips_.length;
  }

  isMuted(): boolean {
    return this.muted;
  }
  setMuted(m: boolean): void {
    this.muted = m;
  }

  isVisible(): boolean {
    return this.visible;
  }
  setVisible(v: boolean): void {
    this.visible = v;
  }

  isLocked(): boolean {
    return this.locked;
  }
  setLocked(l: boolean): void {
    this.locked = l;
  }

  setName(n: string): void {
    this.name = n;
  }

  setGain(g: number): void {
    this.gain = g;
  }

  setPan(p: number): void {
    this.pan = p;
  }

  setSolo(s: boolean): void {
    this.solo = s;
  }

  setOpacity(o: number): void {
    this.opacity = o;
  }

  setBlendMode(b: BlendMode): void {
    this.blendMode = b;
  }

  setUiHeight(h: number): void {
    this.uiHeight = h;
  }

  participatesInComposite(): boolean {
    return this.type === 'video' || this.type === 'subtitle' || this.type === 'aiGenerated';
  }

  participatesInAudioGraph(): boolean {
    return this.type === 'audio' || this.type === 'aiGenerated';
  }

  acceptsClip(clip: Clip): boolean {
    switch (this.type) {
      case 'storyboard':
        return clip.type === 'cut';
      case 'video':
        return (
          clip.type === 'video' ||
          clip.type === 'image' ||
          clip.type === 'color' ||
          clip.type === 'title' ||
          clip.type === 'aiPlaceholder'
        );
      case 'audio':
        return clip.type === 'audio' || clip.type === 'aiPlaceholder';
      case 'subtitle':
        return clip.type === 'subtitle' || clip.type === 'title';
      case 'aiGenerated':
        return true;
      default:
        return false;
    }
  }

  insertClip(c: Clip): boolean {
    if (!c || c.range.duration <= 0) {
      return false;
    }

    const r = c.range;
    if (!this.allowOverlaps) {
      const idx = this.lowerBoundIndex(r.start);
      if (idx < this.clips_.length && this.clips_[idx]!.range.start < timeRangeEnd(r)) {
        return false;
      }
      if (idx > 0) {
        const prev = this.clips_[idx - 1]!;
        if (timeRangeEnd(prev.range) > r.start) {
          return false;
        }
      }
      this.clips_.splice(idx, 0, c);
    } else {
      const idx = this.lowerBoundIndex(r.start);
      this.clips_.splice(idx, 0, c);
    }

    return true;
  }

  takeClip(clipId: Uuid): Clip | null {
    const idx = this.clips_.findIndex((c) => c.id === clipId);
    if (idx !== -1) {
      const [removed] = this.clips_.splice(idx, 1);
      this.dropOrphanTransitions();
      return removed ?? null;
    }
    return null;
  }

  removeClip(clipId: Uuid): boolean {
    return this.takeClip(clipId) !== null;
  }

  removeClipsIn(r: TimeRange): Clip[] {
    const removed: Clip[] = [];
    const rEnd = timeRangeEnd(r);

    let idx = this.lowerBoundIndex(r.start);
    if (idx > 0) idx--;

    while (idx < this.clips_.length && this.clips_[idx]!.range.start < rEnd) {
      const c = this.clips_[idx]!;
      const cStart = c.range.start;
      const cEnd = timeRangeEnd(c.range);

      if (cStart >= r.start && cEnd <= rEnd) {
        removed.push(c);
        this.clips_.splice(idx, 1);
      } else {
        idx++;
      }
    }

    this.dropOrphanTransitions();
    return removed;
  }

  shiftClipsAfter(startFrame: number, delta: number): void {
    if (delta === 0) return;
    for (const c of this.clips_) {
      if (c.range.start >= startFrame) {
        c.setRange({
          start: c.range.start + delta,
          duration: c.range.duration,
        });
      }
    }
    this.resort();
    this.dropOrphanTransitions();
  }

  clipAt(frame: number): Clip | null {
    if (this.clips_.length === 0) return null;
    let low = 0;
    let high = this.clips_.length - 1;

    while (low <= high) {
      const mid = Math.floor((low + high) / 2);
      const c = this.clips_[mid]!;
      const end = timeRangeEnd(c.range);

      if (frame < c.range.start) {
        high = mid - 1;
      } else if (frame >= end) {
        low = mid + 1;
      } else {
        return c;
      }
    }

    return null;
  }

  clipById(id: Uuid): Clip | null {
    return this.clips_.find((c) => c.id === id) ?? null;
  }

  clipsIn(r: TimeRange): Clip[] {
    const out: Clip[] = [];
    if (this.clips_.length === 0) return out;

    const rEnd = timeRangeEnd(r);
    let idx = this.lowerBoundIndex(r.start);
    if (idx > 0) idx--;

    for (; idx < this.clips_.length && this.clips_[idx]!.range.start < rEnd; idx++) {
      const c = this.clips_[idx]!;
      if (timeRangesIntersect(c.range, r)) {
        out.push(c);
      }
    }

    return out;
  }

  nextClipStart(frame: number): number {
    const idx = this.lowerBoundIndex(frame);
    if (idx < this.clips_.length && this.clips_[idx]!.range.start >= frame) {
      return this.clips_[idx]!.range.start;
    }
    return -1;
  }

  prevClipEnd(frame: number): number {
    const idx = this.upperBoundIndex(frame - 1);
    if (idx > 0) {
      return timeRangeEnd(this.clips_[idx - 1]!.range);
    }
    return -1;
  }

  findFreeStart(fromFrame: number, duration: number): number {
    let cursor = Math.max(0, fromFrame);
    for (const c of this.clips_) {
      const end = timeRangeEnd(c.range);
      if (end <= cursor) continue;
      if (c.range.start >= cursor + duration) break;
      cursor = end;
    }
    return cursor;
  }

  contentDuration(): number {
    if (this.clips_.length === 0) return 0;
    let max = 0;
    for (const c of this.clips_) {
      const end = timeRangeEnd(c.range);
      if (end > max) max = end;
    }
    return max;
  }

  resort(): void {
    this.clips_.sort((a, b) => a.range.start - b.range.start);
  }

  clipEndingAt(boundaryFrame: number): Clip | null {
    return this.clips_.find((c) => timeRangeEnd(c.range) === boundaryFrame) ?? null;
  }

  clipStartingAt(boundaryFrame: number): Clip | null {
    return this.clips_.find((c) => c.range.start === boundaryFrame) ?? null;
  }

  maxTransitionDuration(boundaryFrame: number): number {
    const from = this.clipEndingAt(boundaryFrame);
    const to = this.clipStartingAt(boundaryFrame);
    if (!from && !to) return 0;

    let half = kUnlimitedHandle;
    if (from) {
      half = Math.min(half, handleAfter(from), from.range.duration);
    }
    if (to) {
      half = Math.min(half, handleBefore(to), to.range.duration);
    }

    return half > 0 && half < kUnlimitedHandle
      ? half * 2
      : half === kUnlimitedHandle
        ? kUnlimitedHandle
        : 0;
  }

  addTransition(t: Transition, errorOut?: { error?: string }): boolean {
    const fail = (msg: string) => {
      if (errorOut) errorOut.error = msg;
      return false;
    };

    if (!t.transitionId) return fail('Transition id is empty.');

    const from = this.clipEndingAt(t.centerFrame);
    const to = this.clipStartingAt(t.centerFrame);
    if (!from && !to) return fail('There is no clip boundary at this position.');

    const desc = findTransitionDesc(t.transitionId);
    if (desc && !desc.allowsMissingPartner && (!from || !to)) {
      return fail('This transition needs a clip on both sides.');
    }

    const maxDur = this.maxTransitionDuration(t.centerFrame);
    if (maxDur <= 0) return fail('The clips have no spare source frames for a transition.');

    t.durationFrames = Math.min(t.durationFrames, maxDur);
    if (t.durationFrames <= 0) return fail('Transition duration must be positive.');

    t.fromClipId = from?.id;
    t.toClipId = to?.id;

    const existingIdx = this.transitions_.findIndex((x) => x.centerFrame === t.centerFrame);
    if (existingIdx !== -1) {
      this.transitions_[existingIdx] = t;
    } else {
      this.transitions_.push(t);
    }

    return true;
  }

  removeTransition(id: Uuid): void {
    this.transitions_ = this.transitions_.filter((x) => x.id !== id);
  }

  transitionAt(frame: number): Transition | null {
    for (const t of this.transitions_) {
      if (t.contains(frame)) {
        return t;
      }
    }
    return null;
  }

  setTransitions(transitions: Transition[]): void {
    this.transitions_ = [...transitions];
    this.dropOrphanTransitions();
    for (const tr of this.transitions_) {
      const maxDur = this.maxTransitionDuration(tr.centerFrame);
      tr.durationFrames = Math.min(tr.durationFrames, maxDur);
    }
    this.dropOrphanTransitions();
  }

  private dropOrphanTransitions(): void {
    this.transitions_ = this.transitions_.filter((t) => {
      const from = this.clipEndingAt(t.centerFrame);
      const to = this.clipStartingAt(t.centerFrame);
      if (!from && !to) return false;
      if (t.fromClipId && (!from || from.id !== t.fromClipId)) return false;
      if (t.toClipId && (!to || to.id !== t.toClipId)) return false;
      return t.durationFrames > 0;
    });
  }

  private lowerBoundIndex(start: number): number {
    let low = 0;
    let high = this.clips_.length;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      if (this.clips_[mid]!.range.start < start) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return low;
  }

  private upperBoundIndex(start: number): number {
    let low = 0;
    let high = this.clips_.length;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      if (this.clips_[mid]!.range.start <= start) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return low;
  }
}
