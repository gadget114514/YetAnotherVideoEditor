import type { Uuid } from '../id/Uuid.js';
import type { Project } from '../model/Project.js';
import type { Command } from './Command.js';
import { BlendMode } from '../model/BlendMode.js';
import { AudioClip } from '../model/AudioClip.js';

export enum ClipProperty {
  Name = 'Name',
  Start = 'Start',
  Duration = 'Duration',
  SourceOffset = 'SourceOffset',
  Opacity = 'Opacity',
  BlendMode = 'BlendMode',
  FadeIn = 'FadeIn',
  FadeOut = 'FadeOut',
  Enabled = 'Enabled',
  Locked = 'Locked',
  Gain = 'Gain',
  Pan = 'Pan',
}

export class SetClipPropertyCommand implements Command {
  readonly name = 'SetClipProperty';
  private project_: Project;
  private clipId_: Uuid;
  private prop_: ClipProperty;
  private old_: any;
  private new_: any;

  constructor(project: Project, clipId: Uuid | string, prop: ClipProperty, value: any) {
    this.project_ = project;
    this.clipId_ = clipId as Uuid;
    this.prop_ = prop;
    this.new_ = value;

    const clip = this.project_.timeline.findClip(this.clipId_);
    if (clip) {
      this.old_ = this.readCurrent(clip);
    }
  }

  private readCurrent(clip: any): any {
    switch (this.prop_) {
      case ClipProperty.Name:
        return clip.name;
      case ClipProperty.Start:
        return clip.range.start;
      case ClipProperty.Duration:
        return clip.range.duration;
      case ClipProperty.SourceOffset:
        return clip.sourceOffset();
      case ClipProperty.Opacity:
        return clip.opacity;
      case ClipProperty.BlendMode:
        return clip.blendMode;
      case ClipProperty.FadeIn:
        return clip.fadeIn;
      case ClipProperty.FadeOut:
        return clip.fadeOut;
      case ClipProperty.Enabled:
        return clip.enabled;
      case ClipProperty.Locked:
        return clip.locked;
      case ClipProperty.Gain:
        return clip instanceof AudioClip ? clip.gain : 1.0;
      case ClipProperty.Pan:
        return clip instanceof AudioClip ? clip.pan : 0.0;
    }
  }

  private apply(val: any): void {
    const tl = this.project_.timeline;
    let ownerTrack: any = null;
    const clip = tl.findClip(this.clipId_, (t) => {
      ownerTrack = t;
    });
    if (!clip) return;

    switch (this.prop_) {
      case ClipProperty.Name:
        clip.setName(String(val));
        break;
      case ClipProperty.Start: {
        const dur = clip.range.duration;
        clip.setRange({ start: Number(val), duration: dur });
        if (ownerTrack) ownerTrack.resort();
        break;
      }
      case ClipProperty.Duration: {
        const start = clip.range.start;
        clip.setRange({ start, duration: Number(val) });
        break;
      }
      case ClipProperty.SourceOffset:
        clip.setSourceOffset(Number(val));
        break;
      case ClipProperty.Opacity:
        clip.setOpacity(Number(val));
        break;
      case ClipProperty.BlendMode:
        clip.setBlendMode(val as BlendMode);
        break;
      case ClipProperty.FadeIn:
        clip.setFadeIn(Number(val));
        break;
      case ClipProperty.FadeOut:
        clip.setFadeOut(Number(val));
        break;
      case ClipProperty.Enabled:
        clip.setEnabled(Boolean(val));
        break;
      case ClipProperty.Locked:
        clip.setLocked(Boolean(val));
        break;
      case ClipProperty.Gain:
        if (clip instanceof AudioClip) clip.setGain(Number(val));
        break;
      case ClipProperty.Pan:
        if (clip instanceof AudioClip) clip.setPan(Number(val));
        break;
    }
  }

  redo(): void {
    this.apply(this.new_);
  }

  undo(): void {
    this.apply(this.old_);
  }
}

export enum TrackProperty {
  Name = 'Name',
  Gain = 'Gain',
  Pan = 'Pan',
  Muted = 'Muted',
  Solo = 'Solo',
  Opacity = 'Opacity',
  BlendMode = 'BlendMode',
  Visible = 'Visible',
  Locked = 'Locked',
  UiHeight = 'UiHeight',
}

export class SetTrackPropertyCommand implements Command {
  readonly name = 'SetTrackProperty';
  private project_: Project;
  private trackId_: Uuid;
  private prop_: TrackProperty;
  private old_: any;
  private new_: any;

  constructor(project: Project, trackId: Uuid | string, prop: TrackProperty, value: any) {
    this.project_ = project;
    this.trackId_ = trackId as Uuid;
    this.prop_ = prop;
    this.new_ = value;

    const track = this.project_.timeline.trackById(this.trackId_);
    if (track) {
      this.old_ = this.readCurrent(track);
    }
  }

  private readCurrent(track: any): any {
    switch (this.prop_) {
      case TrackProperty.Name:
        return track.name;
      case TrackProperty.Gain:
        return track.gain;
      case TrackProperty.Pan:
        return track.pan;
      case TrackProperty.Muted:
        return track.muted;
      case TrackProperty.Solo:
        return track.solo;
      case TrackProperty.Opacity:
        return track.opacity;
      case TrackProperty.BlendMode:
        return track.blendMode;
      case TrackProperty.Visible:
        return track.visible;
      case TrackProperty.Locked:
        return track.locked;
      case TrackProperty.UiHeight:
        return track.uiHeight;
    }
  }

  private apply(val: any): void {
    const track = this.project_.timeline.trackById(this.trackId_);
    if (!track) return;

    switch (this.prop_) {
      case TrackProperty.Name:
        track.setName(String(val));
        break;
      case TrackProperty.Gain:
        track.setGain(Number(val));
        break;
      case TrackProperty.Pan:
        track.setPan(Number(val));
        break;
      case TrackProperty.Muted:
        track.setMuted(Boolean(val));
        break;
      case TrackProperty.Solo:
        track.solo = Boolean(val);
        break;
      case TrackProperty.Opacity:
        track.setOpacity(Number(val));
        break;
      case TrackProperty.BlendMode:
        track.setBlendMode(val as BlendMode);
        break;
      case TrackProperty.Visible:
        track.setVisible(Boolean(val));
        break;
      case TrackProperty.Locked:
        track.setLocked(Boolean(val));
        break;
      case TrackProperty.UiHeight:
        track.setUiHeight(Number(val));
        break;
    }
  }

  redo(): void {
    this.apply(this.new_);
  }

  undo(): void {
    this.apply(this.old_);
  }
}
