import type { Uuid } from '../id/Uuid.js';
import { Rational, Timebase } from '../time/Rational.js';
import { Track, type TrackType } from './Track.js';
import type { Clip } from './Clip.js';
import { BlendMode } from './BlendMode.js';
import {
  transitionShaderMode,
  resolveTransitionParams,
  resolveTransitionColor,
} from './Transition.js';
import type { RenderSnapshot, LayerItem } from '../snapshot/RenderSnapshot.js';

export class Timeline {
  private tracks_: Track[] = [];
  private revision_: number = 0;
  timebase: Rational = Timebase.Fps59_94;
  canvasSize: { width: number; height: number } = { width: 1920, height: 1080 };

  static defaultTrackName(type: TrackType, ordinal: number): string {
    switch (type) {
      case 'video':
        return `Video ${ordinal}`;
      case 'audio':
        return `Audio ${ordinal}`;
      case 'subtitle':
        return `Subtitle ${ordinal}`;
      case 'aiGenerated':
        return `AI ${ordinal}`;
      case 'storyboard':
        return `Storyboard ${ordinal}`;
      default:
        return `Track ${ordinal}`;
    }
  }

  get tracks(): readonly Track[] {
    return this.tracks_;
  }

  get trackCount(): number {
    return this.tracks_.length;
  }

  get revision(): number {
    return this.revision_;
  }

  trackAt(index: number): Track | null {
    if (index < 0 || index >= this.tracks_.length) return null;
    return this.tracks_[index] ?? null;
  }

  trackById(id: Uuid): Track | null {
    return this.tracks_.find((t) => t.id === id) ?? null;
  }

  indexOfTrack(trackOrId: Track | Uuid): number {
    const id = typeof trackOrId === 'string' ? trackOrId : trackOrId.id;
    return this.tracks_.findIndex((t) => t.id === id);
  }

  tracksOfType(type: TrackType): Track[] {
    return this.tracks_.filter((t) => t.type === type);
  }

  appendTrack(type: TrackType, name?: string): Track {
    return this.insertTrack(this.tracks_.length, type, name);
  }

  insertTrack(index: number, type: TrackType, name?: string): Track {
    const t = new Track(type);
    const ordinal = this.tracksOfType(type).length + 1;
    t.name = name && name.length > 0 ? name : Timeline.defaultTrackName(type, ordinal);

    const safeIndex = Math.max(0, Math.min(this.tracks_.length, index));
    this.tracks_.splice(safeIndex, 0, t);
    this.revision_++;
    return t;
  }

  appendTrackInstance(track: Track): void {
    this.tracks_.push(track);
    this.revision_++;
  }

  clear(): void {
    this.tracks_ = [];
    this.revision_++;
  }

  takeTrack(index: number): Track | null {
    if (index < 0 || index >= this.tracks_.length) return null;
    const [t] = this.tracks_.splice(index, 1);
    this.revision_++;
    return t ?? null;
  }

  removeTrack(index: number): boolean {
    return this.takeTrack(index) !== null;
  }

  takeTrackById(id: Uuid): Track | null {
    const idx = this.indexOfTrack(id);
    return idx !== -1 ? this.takeTrack(idx) : null;
  }

  reinsertTrack(index: number, track: Track): void {
    const safeIndex = Math.max(0, Math.min(this.tracks_.length, index));
    this.tracks_.splice(safeIndex, 0, track);
    this.revision_++;
  }

  moveTrack(fromIndex: number, toIndex: number): void {
    if (fromIndex === toIndex || fromIndex < 0 || toIndex < 0 || fromIndex >= this.tracks_.length || toIndex >= this.tracks_.length) {
      return;
    }
    const [t] = this.tracks_.splice(fromIndex, 1);
    if (t) {
      this.tracks_.splice(toIndex, 0, t);
      this.revision_++;
    }
  }

  findClip(clipId: Uuid, onTrack?: (track: Track) => void): Clip | null {
    for (const t of this.tracks_) {
      const c = t.clipById(clipId);
      if (c) {
        if (onTrack) onTrack(t);
        return c;
      }
    }
    return null;
  }

  findClipWithTrack(clipId: Uuid): { clip: Clip; track: Track } | null {
    for (const t of this.tracks_) {
      const c = t.clipById(clipId);
      if (c) {
        return { clip: c, track: t };
      }
    }
    return null;
  }

  duration(): number {
    let max = 0;
    for (const t of this.tracks_) {
      const dur = t.contentDuration();
      if (dur > max) max = dur;
    }
    return max;
  }

  buildSnapshot(frame: number): RenderSnapshot {
    const layers: LayerItem[] = [];
    let zIndex = 0;

    for (const track of this.tracks_) {
      if (!track.visible || !track.participatesInComposite()) {
        zIndex++;
        continue;
      }

      const tr = track.transitionAt(frame);
      if (tr) {
        const from = tr.fromClipId ? track.clipById(tr.fromClipId) : null;
        const to = tr.toClipId ? track.clipById(tr.toClipId) : null;
        const progress = tr.progressAt(frame);
        const mode = transitionShaderMode(tr.transitionId);
        const trParams = resolveTransitionParams(tr);
        const trColor = resolveTransitionColor(tr);

        const pushSide = (clip: Clip | null, incoming: boolean) => {
          if (!clip || !clip.enabled) return;
          const item = clip.makeLayerItem(frame, zIndex, track);
          if (item) {
            item.opacity *= track.opacity ?? 1.0;
            item.filters = clip.resolvedFilters();
            item.transition = {
              transitionId: tr.transitionId,
              progress,
              isIncoming: incoming,
              shaderMode: mode,
              params: trParams,
              color: trColor,
            };
            layers.push(item);
          }
        };

        pushSide(from, false);
        pushSide(to, true);
        zIndex++;
        continue;
      }

      const clip = track.clipAt(frame);
      if (!clip || !clip.enabled) {
        zIndex++;
        continue;
      }

      const item = clip.makeLayerItem(frame, zIndex, track);
      if (item) {
        item.opacity *= track.opacity ?? 1.0;
        item.blendMode =
          track.blendMode !== BlendMode.Normal ? track.blendMode : item.blendMode;
        item.filters = clip.resolvedFilters();
        layers.push(item);
      }
      zIndex++;
    }

    return {
      frameIndex: frame,
      timebase: this.timebase,
      layers,
    };
  }
}
