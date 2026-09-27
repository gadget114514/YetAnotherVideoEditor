import type { Uuid } from '../id/Uuid.js';
import type { Project } from '../model/Project.js';
import type { Command } from './Command.js';
import type { Clip } from '../model/Clip.js';
import { SubtitleClip } from '../model/SubtitleClip.js';
import type { Track, TrackType } from '../model/Track.js';
import { SubtitleText } from '../subtitle/SubtitleText.js';
import { SubtitleStyleField, type SubtitleStyleDiff } from '../subtitle/SubtitleStyle.js';

export enum OverlapPolicy {
  SplitToNewTracks = 'SplitToNewTracks',
  TrimPrevious = 'TrimPrevious',
  SkipOverlapping = 'SkipOverlapping',
}

export class ImportSubtitleCommand implements Command {
  readonly name = 'ImportSubtitle';
  private project_: Project;
  private clips_: Clip[];
  private policy_: OverlapPolicy;
  private targetTrackIndex_: number;
  private trackType_: TrackType;
  private sourceFileName_: string;

  private baseTrackIndex_: number = -1;
  private createdTrackIndices_: number[] = [];
  private insertedClips_: { trackId: Uuid; clipId: Uuid }[] = [];

  constructor(
    project: Project,
    clips: Clip[],
    policy: OverlapPolicy = OverlapPolicy.SplitToNewTracks,
    targetTrackIndex: number = -1,
    trackType: TrackType = 'subtitle',
    sourceFileName: string = '',
  ) {
    this.project_ = project;
    this.clips_ = [...clips];
    this.policy_ = policy;
    this.targetTrackIndex_ = targetTrackIndex;
    this.trackType_ = trackType;
    this.sourceFileName_ = sourceFileName;
  }

  get insertedCount(): number {
    return this.insertedClips_.length;
  }

  get baseTrackIndex(): number {
    return this.baseTrackIndex_;
  }

  private ensureTrack(targetIndex: number): Track {
    const tl = this.project_.timeline;
    if (targetIndex >= 0 && targetIndex < tl.trackCount) {
      const t = tl.trackAt(targetIndex);
      if (t && t.type === this.trackType_) {
        return t;
      }
    }
    const created = tl.appendTrack(this.trackType_, this.sourceFileName_ || undefined);
    const newIdx = tl.indexOfTrack(created);
    this.createdTrackIndices_.push(newIdx);
    return created;
  }

  redo(): void {
    this.insertedClips_ = [];
    this.createdTrackIndices_ = [];

    const tl = this.project_.timeline;
    let track: Track;
    if (this.targetTrackIndex_ === -1) {
      track = tl.appendTrack(this.trackType_, this.sourceFileName_ || undefined);
      this.baseTrackIndex_ = tl.indexOfTrack(track);
      this.createdTrackIndices_.push(this.baseTrackIndex_);
    } else {
      track = this.ensureTrack(this.targetTrackIndex_);
      this.baseTrackIndex_ = tl.indexOfTrack(track);
    }

    for (const clip of this.clips_) {
      if (track.insertClip(clip)) {
        this.insertedClips_.push({ trackId: track.id, clipId: clip.id });
      }
    }
  }

  undo(): void {
    const tl = this.project_.timeline;
    // Remove inserted clips
    for (const entry of this.insertedClips_) {
      const trk = tl.trackById(entry.trackId);
      if (trk) {
        trk.removeClip(entry.clipId);
      }
    }
    this.insertedClips_ = [];

    // Remove created tracks in reverse order
    for (let i = this.createdTrackIndices_.length - 1; i >= 0; i--) {
      const idx = this.createdTrackIndices_[i]!;
      if (idx >= 0 && idx < tl.trackCount) {
        tl.removeTrack(idx);
      }
    }
    this.createdTrackIndices_ = [];
  }
}

export class EditSubtitleTextCommand implements Command {
  readonly name = 'EditSubtitleText';
  private project_: Project;
  private clipId_: Uuid;
  private old_: string = '';
  new_: string = '';

  constructor(project: Project, clipId: Uuid, newPlainText: string) {
    this.project_ = project;
    this.clipId_ = clipId;
    this.new_ = newPlainText;

    const clip = this.findSubtitleClip();
    if (clip) {
      this.old_ = clip.plainText();
    }
  }

  private findSubtitleClip(): SubtitleClip | null {
    const c = this.project_.timeline.findClip(this.clipId_);
    if (c instanceof SubtitleClip) return c;
    return null;
  }

  mergeWith(other: Command): boolean {
    if (other instanceof EditSubtitleTextCommand && other.clipId_ === this.clipId_) {
      this.new_ = other.new_;
      return true;
    }
    return false;
  }

  private apply(text: string): void {
    const clip = this.findSubtitleClip();
    if (!clip) return;
    const subText = new SubtitleText(text);
    for (const span of clip.text.spans) {
      subText.addSpan(span);
    }
    clip.setText(subText);
  }

  redo(): void {
    this.apply(this.new_);
  }

  undo(): void {
    this.apply(this.old_);
  }
}

export class SetSubtitleStyleCommand implements Command {
  readonly name = 'SetSubtitleStyle';
  private project_: Project;
  private clipId_: Uuid;
  private field_: SubtitleStyleField;
  private old_: any = undefined;
  private new_: any;

  constructor(
    project: Project,
    clipId: Uuid,
    field: SubtitleStyleField,
    value: any,
  ) {
    this.project_ = project;
    this.clipId_ = clipId;
    this.field_ = field;
    this.new_ = value;

    const clip = this.findSubtitleClip();
    if (clip) {
      this.old_ = this.getFieldValue(clip.styleOverride, this.field_);
    }
  }

  private findSubtitleClip(): SubtitleClip | null {
    const c = this.project_.timeline.findClip(this.clipId_);
    if (c instanceof SubtitleClip) return c;
    return null;
  }

  private getFieldValue(d: SubtitleStyleDiff, field: SubtitleStyleField): any {
    switch (field) {
      case SubtitleStyleField.FontFamily:
        return d.fontFamily;
      case SubtitleStyleField.FontPointSize:
        return d.fontPointSize;
      case SubtitleStyleField.FontWeight:
        return d.fontWeight;
      case SubtitleStyleField.Italic:
        return d.italic;
      case SubtitleStyleField.FillColor:
        return d.fillColor;
      case SubtitleStyleField.OutlineColor:
        return d.outlineColor;
      case SubtitleStyleField.OutlineWidth:
        return d.outlineWidth;
      case SubtitleStyleField.ShadowColor:
        return d.shadowColor;
      case SubtitleStyleField.ShadowBlur:
        return d.shadowBlur;
      case SubtitleStyleField.BoxEnabled:
        return d.boxEnabled;
      case SubtitleStyleField.BoxColor:
        return d.boxColor;
      case SubtitleStyleField.HAlign:
        return d.hAlign;
      case SubtitleStyleField.VAlign:
        return d.vAlign;
      case SubtitleStyleField.Anchor:
        return d.anchor;
      case SubtitleStyleField.LineSpacing:
        return d.lineSpacing;
      case SubtitleStyleField.LetterSpacing:
        return d.letterSpacing;
      case SubtitleStyleField.MaxWidthRatio:
        return d.maxWidthRatio;
      case SubtitleStyleField.RotationDeg:
        return d.rotationDeg;
      case SubtitleStyleField.Scale:
        return d.scale;
      case SubtitleStyleField.Opacity:
        return d.opacity;
      case SubtitleStyleField.Vertical:
        return d.vertical;
    }
  }

  private setFieldValue(
    d: SubtitleStyleDiff,
    field: SubtitleStyleField,
    val: any,
  ): void {
    const isSet = val !== undefined && val !== null;
    switch (field) {
      case SubtitleStyleField.FontFamily:
        d.fontFamily = isSet ? String(val) : undefined;
        break;
      case SubtitleStyleField.FontPointSize:
        d.fontPointSize = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.FontWeight:
        d.fontWeight = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.Italic:
        d.italic = isSet ? Boolean(val) : undefined;
        break;
      case SubtitleStyleField.FillColor:
        d.fillColor = isSet ? String(val) : undefined;
        break;
      case SubtitleStyleField.OutlineColor:
        d.outlineColor = isSet ? String(val) : undefined;
        break;
      case SubtitleStyleField.OutlineWidth:
        d.outlineWidth = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.ShadowColor:
        d.shadowColor = isSet ? String(val) : undefined;
        break;
      case SubtitleStyleField.ShadowBlur:
        d.shadowBlur = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.BoxEnabled:
        d.boxEnabled = isSet ? Boolean(val) : undefined;
        break;
      case SubtitleStyleField.BoxColor:
        d.boxColor = isSet ? String(val) : undefined;
        break;
      case SubtitleStyleField.HAlign:
        d.hAlign = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.VAlign:
        d.vAlign = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.Anchor:
        d.anchor = isSet ? val : undefined;
        break;
      case SubtitleStyleField.LineSpacing:
        d.lineSpacing = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.LetterSpacing:
        d.letterSpacing = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.MaxWidthRatio:
        d.maxWidthRatio = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.RotationDeg:
        d.rotationDeg = isSet ? Number(val) : undefined;
        break;
      case SubtitleStyleField.Scale:
        d.scale = isSet ? val : undefined;
        break;
      case SubtitleStyleField.Opacity:
        d.opacity = isSet ? Math.max(0, Math.min(1, Number(val))) : undefined;
        break;
      case SubtitleStyleField.Vertical:
        d.vertical = isSet ? Boolean(val) : undefined;
        break;
    }
  }

  private apply(val: any): void {
    const clip = this.findSubtitleClip();
    if (!clip) return;
    const diff: SubtitleStyleDiff = { ...clip.styleOverride };
    this.setFieldValue(diff, this.field_, val);
    clip.setStyleOverride(diff);
  }

  redo(): void {
    this.apply(this.new_);
  }

  undo(): void {
    this.apply(this.old_);
  }
}
