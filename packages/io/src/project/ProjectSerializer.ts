import {
  Project,
  Timeline,
  Track,
  type TrackType,
  Clip,
  VideoClip,
  AudioClip,
  ColorClip,
  ImageClip,
  SubtitleClip,
  TitleClip,
  AiPlaceholderClip,
  CutClip,
  BlendMode,
  TransitionInstance,
  type Transition,
  SubtitleText,
  createUuid,
  type Uuid,
  type Rational,
  Timebase,
} from '@yave/core';

import {
  kSchemaVersion,
  kProject,
  kTracks,
  kClips,
  kAssets,
  kLibrary,
  kFolders,
  kAssignments,
  kParentId,
  kTrackId,
  kTrackName,
  kTrackType,
  kTrackVisible,
  kTrackLocked,
  kTransitions,
  kClipId,
  kClipType,
  kEnabled,
  kRange,
  kAssetId,
  kSourceOffset,
  kFilters,
  kPresetId,
  kText,
  kStylePresetId,
  kStyleOverride,
  kEffectStack,
  kWordTimings,
  videoClipKeys,
  subtitleClipKeys,
} from './JsonKeys.js';

import { blendModeToString, stringToBlendMode } from './EnumMapping.js';
import { applyMigrations } from './SchemaMigration.js';

export interface SaveOptions {
  indented?: boolean;
  collectGeneratedAssets?: boolean;
  collectSourceAssets?: boolean;
  embedThumbnails?: boolean;
  omitDefaultValues?: boolean;
}

export interface LoadResult {
  ok: boolean;
  errorMessage: string;
  warnings: string[];
  loadedSchemaVersion: number;
  migrated: boolean;
  missingAssetPaths: string[];
  missingPluginIds: string[];
}

export class ProjectSerializer {
  static readonly kCurrentSchemaVersion = 4;

  static serializeProject(p: Project, o: SaveOptions = {}): Record<string, any> {
    const root: Record<string, any> = { ...p.unknownFields };

    root[kSchemaVersion] = ProjectSerializer.kCurrentSchemaVersion;
    root['application'] = { name: 'YAVE', version: '1.0.0' };
    root['savedAt'] = new Date().toISOString();

    const proj: Record<string, any> = {
      name: p.name,
      timebase: { num: p.timebase.num, den: p.timebase.den },
      canvasSize: { width: p.canvasSize.width, height: p.canvasSize.height },
      sampleRate: p.sampleRate,
      channels: p.channels,
      duration: p.timeline.duration(),
      playhead: p.playhead,
      workRange: { ...p.workRange },
      colorSpace: p.colorSpace,
    };
    root[kProject] = proj;

    // Assets
    const assetsArr: any[] = [];
    for (const id of p.assets.allIds()) {
      const a = p.assets.asset(id);
      if (a) {
        assetsArr.push({
          id: a.id,
          kind: a.kind,
          relativePath: a.relativePath,
          hash: a.hash,
          durationFrames: a.durationFrames,
          frameRate: { num: a.frameRate.num, den: a.frameRate.den },
          resolution: { ...a.resolution },
          hasAudio: a.hasAudio,
        });
      }
    }
    root[kAssets] = assetsArr;

    // Subtitle Style Presets
    root['subtitleStylePresets'] = p.subtitleStylePresets.map((sp) => ({
      id: sp.id,
      name: sp.name,
      style: { ...sp.style },
    }));

    // Tracks
    root[kTracks] = ProjectSerializer.serializeTracks(p.timeline, o);

    // Media library folders
    root[kLibrary] = {
      [kFolders]: p.mediaFolders.folders.map((f) => ({
        id: f.id,
        name: f.name,
        [kParentId]: f.parentId ?? null,
      })),
      [kAssignments]: {},
    };

    // StoryBible
    if (p.storyBible && Object.keys(p.storyBible).length > 0) {
      root['storyBible'] = { ...p.storyBible };
    }

    return root;
  }

  static serializeTracks(tl: Timeline, o: SaveOptions): any[] {
    const arr: any[] = [];
    for (let i = 0; i < tl.trackCount; i++) {
      const t = tl.trackAt(i);
      if (t) {
        arr.push(ProjectSerializer.serializeTrack(t, o));
      }
    }
    return arr;
  }

  static serializeTrack(t: Track, o: SaveOptions): Record<string, any> {
    const obj: Record<string, any> = {};

    obj[kTrackId] = t.id;
    obj[kTrackName] = t.name;
    obj[kTrackType] = t.type;
    obj[kTrackVisible] = t.visible;
    obj[kTrackLocked] = t.locked;
    obj['height'] = t.uiHeight;
    obj['color'] = t.color;

    if (t.type === 'video' || t.type === 'aiGenerated') {
      obj['opacity'] = t.opacity;
      obj['blendMode'] = blendModeToString(t.blendMode);
    } else if (t.type === 'audio') {
      obj['gain'] = t.gain;
      obj['pan'] = t.pan;
      obj['muted'] = t.muted;
      obj['solo'] = t.solo;
    }

    // Clips
    const clipsArr: any[] = [];
    for (const c of t.clips) {
      clipsArr.push(ProjectSerializer.serializeClip(c, o));
    }
    obj[kClips] = clipsArr;

    // Transitions
    if (t.transitions.length > 0) {
      obj[kTransitions] = t.transitions.map((tr) => ({
        id: tr.id,
        transitionId: tr.transitionId,
        fromClipId: tr.fromClipId ?? null,
        toClipId: tr.toClipId ?? null,
        centerFrame: tr.centerFrame,
        duration: tr.durationFrames,
        params: { ...tr.params },
      }));
    }

    return obj;
  }

  static serializeClip(c: Clip, o: SaveOptions): Record<string, any> {
    const obj: Record<string, any> = { ...c.unknownFields };

    obj[kClipId] = c.id;
    obj[kClipType] = c.type;
    obj[kRange] = { start: c.range.start, duration: c.range.duration };

    if (c.name) obj['name'] = c.name;
    if (!o.omitDefaultValues || !c.enabled) obj[kEnabled] = c.enabled;
    if (!o.omitDefaultValues || c.locked) obj['locked'] = c.locked;
    if (!o.omitDefaultValues || c.opacity !== 1.0) obj['opacity'] = c.opacity;
    if (!o.omitDefaultValues || c.blendMode !== BlendMode.Normal) {
      obj['blendMode'] = blendModeToString(c.blendMode);
    }
    if (c.fadeInFrames() > 0) obj['fadeIn'] = c.fadeInFrames();
    if (c.fadeOutFrames() > 0) obj['fadeOut'] = c.fadeOutFrames();

    if (c.filters.length > 0) {
      obj[kFilters] = c.filters.map((f) => ({
        filterId: f.filterId,
        enabled: f.enabled ?? true,
        params: { ...f.params },
      }));
    }

    if (c instanceof VideoClip) {
      obj[kAssetId] = c.assetId;
      obj[kSourceOffset] = c.sourceOffset();
      if (!o.omitDefaultValues || c.speed !== 1.0) obj['speed'] = c.speed;
      if (!o.omitDefaultValues || c.reversed) obj['reversed'] = c.reversed;
    } else if (c instanceof AudioClip) {
      obj[kAssetId] = c.assetId;
      obj[kSourceOffset] = c.sourceOffset();
      if (!o.omitDefaultValues || c.gain !== 1.0) obj['gain'] = c.gain;
      if (!o.omitDefaultValues || c.pan !== 0.0) obj['pan'] = c.pan;
    } else if (c instanceof SubtitleClip) {
      obj[kStylePresetId] = c.stylePresetId;
      obj[kText] = {
        plain: c.text.plain,
        spans: c.text.spans.map((s) => ({ ...s })),
      };
      if (Object.keys(c.styleOverride).length > 0) {
        obj[kStyleOverride] = { ...c.styleOverride };
      }
      if (c.effectStack.length > 0) {
        obj[kEffectStack] = c.effectStack.map((e) => ({
          instanceId: e.instanceId,
          effectId: e.effectId,
          enabled: e.enabled,
          params: { ...e.params },
        }));
      }
      if (c.wordTimings.length > 0) {
        obj[kWordTimings] = c.wordTimings.map((w) => ({ ...w }));
      }
      if (c instanceof TitleClip && c.presetId) {
        obj[kPresetId] = c.presetId;
      }
    }

    return obj;
  }

  static deserializeProject(
    p: Project,
    root: Record<string, any>,
    r: LoadResult,
  ): boolean {
    p.unknownFields = extractUnknownFields(root, [
      kSchemaVersion,
      'application',
      'savedAt',
      kProject,
      kAssets,
      'subtitleStylePresets',
      kTracks,
      kLibrary,
      'storyBible',
      'masterAudio',
      'settings',
      'aiTasks',
      'markers',
      'webSettings',
    ]);

    const proj = root[kProject] || {};
    p.setName(proj.name || 'Untitled');
    if (proj.timebase) {
      p.setTimebase(proj.timebase);
    }
    if (proj.canvasSize) {
      p.setCanvasSize(proj.canvasSize);
    }
    if (typeof proj.sampleRate === 'number') p.sampleRate = proj.sampleRate;
    if (typeof proj.channels === 'number') p.channels = proj.channels;
    if (typeof proj.duration === 'number') p.duration = proj.duration;
    if (typeof proj.playhead === 'number') p.playhead = proj.playhead;
    if (proj.workRange) p.workRange = { ...proj.workRange };
    if (proj.colorSpace) p.colorSpace = proj.colorSpace;

    if (root.storyBible) {
      p.storyBible = { ...root.storyBible };
    }

    // Subtitle style presets
    if (Array.isArray(root.subtitleStylePresets)) {
      p.setSubtitleStylePresets(root.subtitleStylePresets);
    }

    // Media library folders
    const lib = root[kLibrary];
    if (lib && Array.isArray(lib[kFolders])) {
      p.setMediaFolders({
        folders: lib[kFolders].map((f: any) => ({
          id: f.id,
          name: f.name,
          parentId: f.parentId || undefined,
        })),
      });
    }

    // Tracks
    const tracksArr = root[kTracks];
    if (Array.isArray(tracksArr)) {
      p.timeline.clear();
      for (const tObj of tracksArr) {
        const track = ProjectSerializer.deserializeTrack(tObj, p, r);
        if (track) {
          p.timeline.appendTrackInstance(track);
        }
      }
    }

    return true;
  }

  static deserializeTrack(
    o: Record<string, any>,
    p: Project,
    r: LoadResult,
  ): Track | null {
    const type: TrackType = o[kTrackType] || 'video';
    const id: Uuid = o[kTrackId] || createUuid();
    const track = new Track(type, id);

    if (o[kTrackName]) track.name = o[kTrackName];
    if (typeof o[kTrackVisible] === 'boolean') track.visible = o[kTrackVisible];
    if (typeof o[kTrackLocked] === 'boolean') track.locked = o[kTrackLocked];
    if (typeof o['height'] === 'number') track.uiHeight = o['height'];
    if (o['color']) track.color = o['color'];

    if (type === 'video' || type === 'aiGenerated') {
      if (typeof o['opacity'] === 'number') track.opacity = o['opacity'];
      if (o['blendMode']) track.blendMode = stringToBlendMode(o['blendMode']);
    } else if (type === 'audio') {
      if (typeof o['gain'] === 'number') track.gain = o['gain'];
      if (typeof o['pan'] === 'number') track.pan = o['pan'];
      if (typeof o['muted'] === 'boolean') track.muted = o['muted'];
      if (typeof o['solo'] === 'boolean') track.solo = o['solo'];
    }

    // Clips
    const clipsArr = o[kClips];
    if (Array.isArray(clipsArr)) {
      for (const cObj of clipsArr) {
        const clip = ProjectSerializer.deserializeClip(cObj, p, r);
        if (clip) {
          track.insertClip(clip);
        }
      }
    }

    // Transitions
    const transArr = o[kTransitions];
    if (Array.isArray(transArr)) {
      const transitions: Transition[] = [];
      for (const trObj of transArr) {
        const t = new TransitionInstance();
        t.id = trObj.id || createUuid();
        t.transitionId = trObj.transitionId;
        t.fromClipId = trObj.fromClipId ?? undefined;
        t.toClipId = trObj.toClipId ?? undefined;
        t.centerFrame = trObj.centerFrame ?? 0;
        t.durationFrames = trObj.duration ?? 0;
        t.params = trObj.params ?? {};
        transitions.push(t);
      }
      track.setTransitions(transitions);
    }

    return track;
  }

  static deserializeClip(
    o: Record<string, any>,
    p: Project,
    r: LoadResult,
  ): Clip | null {
    const type = o[kClipType] || 'video';

    let clip: Clip;
    if (type === 'video') {
      const vc = new VideoClip(o[kAssetId] ?? undefined);
      if (typeof o[kSourceOffset] === 'number') vc.setSourceOffset(o[kSourceOffset]);
      if (typeof o['speed'] === 'number') vc.speed = o['speed'];
      if (typeof o['reversed'] === 'boolean') vc.reversed = o['reversed'];
      clip = vc;
    } else if (type === 'audio') {
      const ac = new AudioClip(o[kAssetId] ?? undefined);
      if (typeof o[kSourceOffset] === 'number') ac.setSourceOffset(o[kSourceOffset]);
      if (typeof o['gain'] === 'number') ac.gain = o['gain'];
      if (typeof o['pan'] === 'number') ac.pan = o['pan'];
      clip = ac;
    } else if (type === 'subtitle' || type === 'title') {
      const sc = type === 'title' ? new TitleClip() : new SubtitleClip();
      if (o[kStylePresetId]) sc.stylePresetId = o[kStylePresetId];
      if (o[kText]) {
        const subText = new SubtitleText(o[kText].plain || '');
        if (Array.isArray(o[kText].spans)) {
          for (const s of o[kText].spans) {
            subText.addSpan(s);
          }
        }
        sc.setText(subText);
      }
      if (o[kStyleOverride]) {
        sc.setStyleOverride(o[kStyleOverride]);
      }
      if (Array.isArray(o[kEffectStack])) {
        for (const e of o[kEffectStack]) {
          sc.addEffect(e);
        }
      }
      if (Array.isArray(o[kWordTimings])) {
        sc.setWordTimings(o[kWordTimings]);
      }
      if (sc instanceof TitleClip && o[kPresetId]) {
        sc.setPresetId(o[kPresetId]);
      }
      clip = sc;
    } else if (type === 'color') {
      clip = new ColorClip();
    } else if (type === 'image') {
      clip = new ImageClip(o[kAssetId] ?? undefined);
    } else if (type === 'aiPlaceholder') {
      clip = new AiPlaceholderClip();
    } else if (type === 'cut') {
      clip = new CutClip();
    } else {
      clip = new VideoClip();
    }

    // Common properties
    if (o[kClipId]) clip.setId(o[kClipId]);
    if (o[kRange]) {
      clip.setRange({
        start: o[kRange].start ?? 0,
        duration: o[kRange].duration ?? 0,
      });
    }
    if (o['name']) clip.setName(o['name']);
    if (typeof o[kEnabled] === 'boolean') clip.setEnabled(o[kEnabled]);
    if (typeof o['locked'] === 'boolean') clip.setLocked(o['locked']);
    if (typeof o['opacity'] === 'number') clip.setOpacity(o['opacity']);
    if (o['blendMode']) clip.setBlendMode(stringToBlendMode(o['blendMode']));
    if (typeof o['fadeIn'] === 'number') clip.setFadeIn(o['fadeIn']);
    if (typeof o['fadeOut'] === 'number') clip.setFadeOut(o['fadeOut']);
    if (o['generatedByTaskId']) clip.setGeneratedByTaskId(o['generatedByTaskId']);

    // Filters
    if (Array.isArray(o[kFilters])) {
      clip.setFilters(
        o[kFilters].map((f: any) => ({
          filterId: f.filterId,
          enabled: f.enabled ?? true,
          params: { ...(f.params || {}) },
        })),
      );
    }

    // Preserve unknown fields
    const known =
      clip instanceof SubtitleClip ? subtitleClipKeys : videoClipKeys;
    clip.setUnknownFields(extractUnknownFields(o, known));

    return clip;
  }

  static saveToString(project: Project, opts: SaveOptions = {}): string {
    const root = ProjectSerializer.serializeProject(project, opts);
    return JSON.stringify(root, null, opts.indented !== false ? 2 : undefined);
  }

  static loadFromString(
    project: Project,
    jsonString: string,
    projectFilePath?: string,
  ): LoadResult {
    const result: LoadResult = {
      ok: false,
      errorMessage: '',
      warnings: [],
      loadedSchemaVersion: 0,
      migrated: false,
      missingAssetPaths: [],
      missingPluginIds: [],
    };

    let root: Record<string, any>;
    try {
      root = JSON.parse(jsonString);
    } catch (e: any) {
      result.errorMessage = `Invalid JSON: ${e.message}`;
      return result;
    }

    const version = root[kSchemaVersion] ?? ProjectSerializer.kCurrentSchemaVersion;
    result.loadedSchemaVersion = version;

    if (version > ProjectSerializer.kCurrentSchemaVersion) {
      result.warnings.push(
        `This project was saved with a newer version of YAVE (schema ${version}). Some data may be lost if you save it with this version.`,
      );
    } else if (version < ProjectSerializer.kCurrentSchemaVersion) {
      applyMigrations(root, version, ProjectSerializer.kCurrentSchemaVersion, result);
    }

    if (!ProjectSerializer.deserializeProject(project, root, result)) {
      return result;
    }

    // Resolve assets
    const assetsArr = root[kAssets];
    if (Array.isArray(assetsArr)) {
      for (const aObj of assetsArr) {
        const id: Uuid = aObj.id || createUuid();
        const relPath: string = aObj.relativePath || '';
        project.assets.addResolvedAsset({
          id,
          kind: aObj.kind || 'video',
          relativePath: relPath,
          resolvedAbsolutePath: relPath,
          hash: aObj.hash || '',
          isMissing: false,
          durationFrames: aObj.durationFrames ?? 0,
          frameRate: aObj.frameRate || Timebase.Fps59_94,
          resolution: aObj.resolution || { width: 1920, height: 1080 },
          hasAudio: aObj.hasAudio ?? false,
        });
      }
    }

    result.ok = true;
    return result;
  }

  static saveAutosave(project: Project): string {
    return ProjectSerializer.saveToString(project, { indented: false, omitDefaultValues: true });
  }

  static loadAutosave(project: Project, json: string): LoadResult {
    return ProjectSerializer.loadFromString(project, json);
  }
}

function extractUnknownFields(
  obj: Record<string, any>,
  knownKeys: readonly string[],
): Record<string, any> {
  const unknown: Record<string, any> = {};
  for (const [key, val] of Object.entries(obj)) {
    if (!knownKeys.includes(key)) {
      unknown[key] = val;
    }
  }
  return unknown;
}
