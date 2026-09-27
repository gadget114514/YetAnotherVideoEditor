import { createUuid, type Uuid } from '../id/Uuid.js';
import { Clip, type ClipType } from './Clip.js';
import type { LayerItem } from '../snapshot/RenderSnapshot.js';
import { SubtitleText } from '../subtitle/SubtitleText.js';
import type { SubtitleStyleDiff } from '../subtitle/SubtitleStyle.js';

export interface SubtitleEffectInstance {
  instanceId: Uuid;
  effectId: string;
  pluginId?: string;
  enabled: boolean;
  params: Record<string, any>;
}

export namespace SubtitleEffectInstance {
  export function create(effectId: string): SubtitleEffectInstance {
    return {
      instanceId: createUuid(),
      effectId,
      pluginId: '',
      enabled: true,
      params: {},
    };
  }
}

export interface WordTiming {
  charStart: number;
  charLength: number;
  startSec: number;
  endSec: number;
}

export const builtinTitle = {
  kCenter: 'yave.title.center',
  kLowerThird: 'yave.title.lowerThird',
  kCredits: 'yave.title.credits',
  kSubtitleCaption: 'yave.title.subtitleCaption',
} as const;

export class SubtitleClip extends Clip {
  readonly type: ClipType = 'subtitle';
  private text_: SubtitleText = new SubtitleText();
  private stylePresetId_: string = 'default';
  private styleOverride_: SubtitleStyleDiff = {};
  private effectStack_: SubtitleEffectInstance[] = [];
  private wordTimings_: WordTiming[] = [];
  private contentRevision_: number = 0;

  get text(): SubtitleText {
    return this.text_;
  }
  setText(t: SubtitleText): void {
    this.text_ = t;
    this.bumpContentRevision();
  }

  plainText(): string {
    return this.text_.plain;
  }
  setPlainText(s: string): void {
    this.text_.setPlain(s);
    this.bumpContentRevision();
  }

  get stylePresetId(): string {
    return this.stylePresetId_;
  }
  set stylePresetId(id: string) {
    this.setStylePresetId(id);
  }
  setStylePresetId(id: string): void {
    this.stylePresetId_ = id;
    this.bumpContentRevision();
  }

  get styleOverride(): SubtitleStyleDiff {
    return this.styleOverride_;
  }
  set styleOverride(d: SubtitleStyleDiff) {
    this.setStyleOverride(d);
  }
  setStyleOverride(d: SubtitleStyleDiff): void {
    this.styleOverride_ = { ...d };
    this.bumpContentRevision();
  }
  clearStyleOverride(): void {
    this.styleOverride_ = {};
    this.bumpContentRevision();
  }

  get effectStack(): readonly SubtitleEffectInstance[] {
    return this.effectStack_;
  }
  get mutableEffectStack(): SubtitleEffectInstance[] {
    return this.effectStack_;
  }
  addEffect(inst: SubtitleEffectInstance): void {
    this.effectStack_.push({ ...inst, params: { ...inst.params } });
    this.bumpContentRevision();
  }
  insertEffect(index: number, inst: SubtitleEffectInstance): void {
    this.effectStack_.splice(index, 0, { ...inst, params: { ...inst.params } });
    this.bumpContentRevision();
  }
  removeEffect(index: number): void {
    if (index >= 0 && index < this.effectStack_.length) {
      this.effectStack_.splice(index, 1);
      this.bumpContentRevision();
    }
  }
  moveEffect(from: number, to: number): void {
    if (from >= 0 && from < this.effectStack_.length && to >= 0 && to < this.effectStack_.length) {
      const [item] = this.effectStack_.splice(from, 1);
      if (item) {
        this.effectStack_.splice(to, 0, item);
        this.bumpContentRevision();
      }
    }
  }
  setEffectEnabled(index: number, enabled: boolean): void {
    if (index >= 0 && index < this.effectStack_.length) {
      this.effectStack_[index]!.enabled = enabled;
      this.bumpContentRevision();
    }
  }

  hasMissingEffects(): boolean {
    return false;
  }
  hasBlockLevelEffect(): boolean {
    return false;
  }

  get wordTimings(): readonly WordTiming[] {
    return this.wordTimings_;
  }
  setWordTimings(w: WordTiming[]): void {
    this.wordTimings_ = [...w];
    this.bumpContentRevision();
  }
  hasWordTimings(): boolean {
    return this.wordTimings_.length > 0;
  }

  get contentRevision(): number {
    return this.contentRevision_;
  }
  bumpContentRevision(): void {
    this.contentRevision_++;
  }

  override sourceOffset(): number {
    return 0;
  }
  override maxDuration(): number {
    return -1;
  }

  clone(): SubtitleClip {
    const c = new SubtitleClip();
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.stylePresetId_ = this.stylePresetId_;
    const copyText = new SubtitleText(this.text_.plain);
    for (const span of this.text_.spans) {
      copyText.addSpan(span);
    }
    c.text_ = copyText;
    c.styleOverride_ = { ...this.styleOverride_ };
    c.effectStack_ = this.effectStack_.map((e) => ({ ...e, params: { ...e.params } }));
    c.wordTimings_ = this.wordTimings_.map((w) => ({ ...w }));
    c.contentRevision_ = this.contentRevision_;
    return c;
  }

  makeLayerItem(frame: number, zIndex: number, track: any): LayerItem | null {
    if (!this.enabled || !track.visible) return null;
    return {
      trackId: track.id,
      clipId: this.id,
      zIndex,
      opacity: this.effectiveOpacity(frame) * (track.opacity ?? 1.0),
      blendMode: this.blendMode,
      transform: { ...this.transform },
      crop: { ...this.crop },
      filters: [...this.filters],
    };
  }
}

export class TitleClip extends SubtitleClip {
  override readonly type: ClipType = 'title';
  private presetId_: string = '';

  get presetId(): string {
    return this.presetId_;
  }
  setPresetId(id: string): void {
    this.presetId_ = id;
  }

  applyPreset(presetId: string): void {
    this.presetId_ = presetId;
    const diff: SubtitleStyleDiff = {};

    if (presetId === builtinTitle.kCenter) {
      diff.fontPointSize = 96.0;
      diff.fontWeight = 900;
      diff.hAlign = 1; // Center
      diff.vAlign = 1; // Middle
      diff.anchor = { x: 0.5, y: 0.5 };
      this.setName('Title');
    } else if (presetId === builtinTitle.kLowerThird) {
      diff.fontPointSize = 56.0;
      diff.hAlign = 0; // Left
      diff.vAlign = 2; // Bottom
      diff.anchor = { x: 0.08, y: 0.78 };
      diff.boxEnabled = true;
      this.setName('Lower Third');
    } else if (presetId === builtinTitle.kCredits) {
      diff.fontPointSize = 44.0;
      diff.hAlign = 1;
      diff.vAlign = 1;
      diff.anchor = { x: 0.5, y: 0.5 };
      this.setName('Credits');
    } else {
      diff.fontPointSize = 48.0;
      diff.hAlign = 1;
      diff.vAlign = 2;
      diff.anchor = { x: 0.5, y: 0.92 };
      this.setName('Caption');
    }

    this.setStyleOverride(diff);
  }

  override clone(): TitleClip {
    const c = new TitleClip();
    c.setId(createUuid());
    this.copyBaseTo(c);
    c.setStylePresetId(this.stylePresetId);
    const copyText = new SubtitleText(this.text.plain);
    for (const span of this.text.spans) {
      copyText.addSpan(span);
    }
    c.setText(copyText);
    c.setStyleOverride(this.styleOverride);
    c.setWordTimings(this.wordTimings.map((w) => ({ ...w })));
    for (const e of this.effectStack) {
      c.addEffect({ ...e, params: { ...e.params } });
    }
    c.presetId_ = this.presetId_;
    return c;
  }
}
