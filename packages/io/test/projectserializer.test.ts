import { describe, it, expect } from 'vitest';
import * as fs from 'node:fs';
import * as path from 'node:path';
import {
  Project,
  VideoClip,
  AudioClip,
  SubtitleClip,
  TitleClip,
  TransitionInstance,
  SubtitleText,
  BlendMode,
  Timebase,
  ImportSubtitleCommand,
  OverlapPolicy,
} from '@yave/core';
import {
  ProjectSerializer,
  SrtParser,
  convertCuesToClips,
} from '../src/index.js';

describe('TestProjectSerializer', () => {
  it('saveLoadRoundTrip', () => {
    const src = new Project();
    src.setName('Round Trip');
    src.setTimebase(Timebase.Fps30);
    src.setCanvasSize({ width: 1920, height: 1080 });

    const tl = src.timeline;

    // ---- 映像トラック ----
    const video = tl.appendTrack('video', 'V1');
    const vc = new VideoClip();
    vc.setRange({ start: 1000, duration: 500 });
    vc.setSourceOffset(200);
    vc.speed = 1.5;
    vc.setOpacity(0.8);
    vc.setBlendMode(BlendMode.Multiply);
    vc.setFadeOut(30);
    expect(video.insertClip(vc)).toBe(true);

    // ---- 音声トラック ----
    const audio = tl.appendTrack('audio', 'A1');
    audio.setGain(0.75);
    audio.setPan(-0.25);
    const ac = new AudioClip();
    ac.setRange({ start: 0, duration: 1200 });
    ac.gain = 1.2;
    expect(audio.insertClip(ac)).toBe(true);

    // ---- 字幕トラック ----
    const subs = tl.appendTrack('subtitle', 'S1');
    const sc = new SubtitleClip();
    sc.setRange({ start: 500, duration: 300 });

    const text = new SubtitleText('これは字幕のテキストです。');
    text.addSpan({
      start: 4,
      length: 2,
      bold: true,
      color: '#ffcc00',
    });
    sc.setText(text);
    sc.setStyleOverride({ fontPointSize: 56.0 });
    sc.addEffect({
      instanceId: 'e0010000-0000-0000-0000-000000000000',
      effectId: 'yave.typewriter',
      enabled: true,
      params: { charsPerSecond: 20.0 },
    });
    sc.setWordTimings([
      { charStart: 0, charLength: 4, startSec: 0.0, endSec: 0.42 },
    ]);
    expect(subs.insertClip(sc)).toBe(true);

    // ---- 保存 / 読み込み ----
    const jsonStr = ProjectSerializer.saveToString(src);
    const loaded = new Project();
    const result = ProjectSerializer.loadFromString(loaded, jsonStr);

    expect(result.ok).toBe(true);
    expect(loaded.name).toBe('Round Trip');
    expect(loaded.timebase).toEqual(Timebase.Fps30);
    expect(loaded.canvasSize).toEqual({ width: 1920, height: 1080 });
    expect(loaded.timeline.trackCount).toBe(tl.trackCount);

    // ---- 各フィールドの一致確認 ----
    const lv = loaded.timeline.tracksOfType('video')[0]!;
    expect(lv.name).toBe('V1');
    const lvc = lv.clips[0]! as VideoClip;
    expect(lvc.range).toEqual({ start: 1000, duration: 500 });
    expect(lvc.sourceOffset()).toBe(200);
    expect(lvc.speed).toBe(1.5);
    expect(lvc.blendMode).toBe(BlendMode.Multiply);

    const la = loaded.timeline.tracksOfType('audio')[0]!;
    expect(la.gain).toBe(0.75);
    expect(la.pan).toBe(-0.25);
    const lac = la.clips[0]! as AudioClip;
    expect(lac.gain).toBe(1.2);

    const ls = loaded.timeline.tracksOfType('subtitle')[0]!;
    const lsc = ls.clips[0]! as SubtitleClip;
    expect(lsc.plainText()).toBe('これは字幕のテキストです。');
    expect(lsc.text.spans.length).toBe(1);
    expect(lsc.text.spans[0]!.bold).toBe(true);
    expect(lsc.effectStack.length).toBe(1);
    expect(lsc.effectStack[0]!.effectId).toBe('yave.typewriter');
    expect(lsc.effectStack[0]!.params['charsPerSecond']).toBe(20.0);
    expect(lsc.wordTimings.length).toBe(1);
    expect(lsc.wordTimings[0]!.startSec).toBe(0.0);
  });

  it('unknownFieldsPreserved', () => {
    const src = new Project();
    const tl = src.timeline;
    const video = tl.appendTrack('video');
    const vc = new VideoClip();
    vc.setRange({ start: 0, duration: 10 });
    video.insertClip(vc);

    const jsonStr = ProjectSerializer.saveToString(src);
    const root = JSON.parse(jsonStr);

    // 未知フィールドを追加する
    root.tracks[0].clips[0].futureField = 'keep-me';
    root.tracks[0].clips[0].futureNested = { a: 1, b: 2 };

    const modifiedJson = JSON.stringify(root);

    // 再読み込みして保存する
    const reloaded = new Project();
    const res = ProjectSerializer.loadFromString(reloaded, modifiedJson);
    expect(res.ok).toBe(true);

    const resavedJson = ProjectSerializer.saveToString(reloaded);
    const reread = JSON.parse(resavedJson);

    expect(reread.tracks[0].clips[0].futureField).toBe('keep-me');
    expect(reread.tracks[0].clips[0].futureNested).toEqual({ a: 1, b: 2 });
  });

  it('enumStringsInJson', () => {
    const src = new Project();
    const t = src.timeline.appendTrack('aiGenerated');
    const c = new VideoClip();
    c.setRange({ start: 0, duration: 5 });
    c.setBlendMode(BlendMode.Screen);
    t.insertClip(c);

    const jsonStr = ProjectSerializer.saveToString(src);
    const root = JSON.parse(jsonStr);

    expect(root.tracks[0].type).toBe('aiGenerated');
    expect(root.tracks[0].clips[0].type).toBe('video');
    expect(root.tracks[0].clips[0].blendMode).toBe('screen');
  });

  it('schemaVersionWarning', () => {
    const src = new Project();
    const jsonStr = ProjectSerializer.saveToString(src);
    const root = JSON.parse(jsonStr);
    root.schemaVersion = 999;

    const loaded = new Project();
    const result = ProjectSerializer.loadFromString(loaded, JSON.stringify(root));
    expect(result.ok).toBe(true);
    expect(result.loadedSchemaVersion).toBe(999);
    expect(
      result.warnings.some((w) => w.includes('newer version')),
    ).toBe(true);
  });

  it('autosaveCompact', () => {
    const src = new Project();
    const compactJson = ProjectSerializer.saveAutosave(src);
    expect(compactJson.includes('\n')).toBe(false);

    const loaded = new Project();
    const result = ProjectSerializer.loadAutosave(loaded, compactJson);
    expect(result.ok).toBe(true);
  });

  it('dndAudioAssetRoundTrip', () => {
    const src = new Project();
    const mediaPath = '/media/audio.wav';
    const asset = src.assets.registerAsset(mediaPath, 'audio');
    expect(asset.id).toBeDefined();

    const audio = src.timeline.appendTrack('audio', 'A1');
    const ac = new AudioClip(asset.id);
    ac.setRange({ start: 0, duration: 1200 });
    ac.gain = 1.0;
    expect(audio.insertClip(ac)).toBe(true);

    const jsonStr = ProjectSerializer.saveToString(src);
    const loaded = new Project();
    const result = ProjectSerializer.loadFromString(loaded, jsonStr);
    expect(result.ok).toBe(true);

    expect(loaded.assets.count()).toBe(1);
    const la = loaded.assets.asset(asset.id);
    expect(la).not.toBeNull();
    expect(la!.relativePath).toBe(mediaPath);

    const laudio = loaded.timeline.tracksOfType('audio')[0]!;
    const lclip = laudio.clips[0]! as AudioClip;
    expect(lclip.assetId).toBe(asset.id);
  });

  it('dndSrtImportRoundTrip', () => {
    const src = new Project();
    const srt =
      '1\n00:00:01,000 --> 00:00:02,000\n一つ目\n\n' +
      '2\n00:00:02,500 --> 00:00:03,500\n二つ目\n\n' +
      '3\n00:00:04,000 --> 00:00:05,000\n三つ目\n\n';

    const parsed = SrtParser.parseText(srt);
    expect(parsed.ok).toBe(true);
    const clips = convertCuesToClips(parsed, src.timeline.timebase, 'default');
    expect(clips.length).toBe(3);

    src.undoStack.push(
      new ImportSubtitleCommand(
        src,
        clips,
        OverlapPolicy.SplitToNewTracks,
        -1,
        'subtitle',
        'test.srt',
      ),
    );

    const subs = src.timeline.tracksOfType('subtitle');
    expect(subs.length).toBe(1);
    expect(subs[0]!.clipCount).toBe(3);

    const jsonStr = ProjectSerializer.saveToString(src);
    const loaded = new Project();
    const result = ProjectSerializer.loadFromString(loaded, jsonStr);
    expect(result.ok).toBe(true);

    const lsubs = loaded.timeline.tracksOfType('subtitle');
    expect(lsubs.length).toBe(1);
    expect(lsubs[0]!.clipCount).toBe(3);

    const found = lsubs[0]!.clips.map((c) => ({
      text: (c as SubtitleClip).plainText(),
      start: c.range.start,
      dur: c.range.duration,
    }));
    expect(found[0]!.text).toBe('一つ目');
    expect(found[1]!.text).toBe('二つ目');
    expect(found[2]!.text).toBe('三つ目');
    expect(found[0]!.start).toBeGreaterThan(0);
    expect(found[0]!.start).toBeLessThan(found[1]!.start);
  });

  it('loadSampleProjectFixture', () => {
    const fixturePath = path.resolve(
      __dirname,
      '../../../tests/fixtures/sample_project.yave',
    );
    const fileContent = fs.readFileSync(fixturePath, 'utf-8');

    const project = new Project();
    const result = ProjectSerializer.loadFromString(project, fileContent);

    expect(result.ok).toBe(true);
    expect(result.loadedSchemaVersion).toBe(1);
    expect(result.migrated).toBe(true); // migrated from schema 1 to 4
    expect(project.name).toBe('Sample Project');
    expect(project.canvasSize).toEqual({ width: 3840, height: 2160 });
    expect(project.timeline.trackCount).toBe(3);

    const videoTrack = project.timeline.tracksOfType('video')[0]!;
    expect(videoTrack.name).toBe('Background');
    expect(videoTrack.clipCount).toBe(1);
    expect(videoTrack.clips[0]!.range).toEqual({ start: 0, duration: 1800 });

    const audioTrack = project.timeline.tracksOfType('audio')[0]!;
    expect(audioTrack.name).toBe('Narration');
    expect(audioTrack.clipCount).toBe(1);
    expect(audioTrack.gain).toBe(0.8);

    const subTrack = project.timeline.tracksOfType('subtitle')[0]!;
    expect(subTrack.name).toBe('Subtitles JA');
    expect(subTrack.clipCount).toBe(1);
    const subClip = subTrack.clips[0]! as SubtitleClip;
    expect(subClip.plainText()).toContain('これは字幕のテキストです。');
    expect(subClip.effectStack.length).toBe(1);
    expect(subClip.effectStack[0]!.effectId).toBe('yave.fade');
  });

  it('transitionFilterSerializerRoundTrip', () => {
    const project = new Project();
    const tl = project.timeline;
    const track = tl.appendTrack('video');
    const trackId = track.id;

    const a = new VideoClip();
    a.setRange({ start: 0, duration: 100 });
    a.setSourceOffset(30);
    a.setMaxDurationFrames(300);
    a.addFilter({
      filterId: 'yave.filter.colorAdjust',
      params: { contrast: 1.25 },
      enabled: true,
    });
    expect(track.insertClip(a)).toBe(true);

    const b = new VideoClip();
    b.setRange({ start: 100, duration: 100 });
    b.setSourceOffset(30);
    b.setMaxDurationFrames(300);
    expect(track.insertClip(b)).toBe(true);

    const t = new TransitionInstance();
    t.transitionId = 'yave.trans.slide';
    t.centerFrame = 100;
    t.durationFrames = 20;
    expect(track.addTransition(t)).toBe(true);

    const title = new TitleClip();
    title.applyPreset('yave.title.lowerThird');
    title.setRange({ start: 300, duration: 60 });
    expect(track.insertClip(title)).toBe(true);

    // MediaFolderTree
    project.setMediaFolders({
      folders: [{ id: 'f0010000-0000-0000-0000-000000000000', name: 'Footage' }],
    });

    const jsonStr = ProjectSerializer.saveToString(project);
    const loaded = new Project();
    const result = ProjectSerializer.loadFromString(loaded, jsonStr);
    expect(result.ok).toBe(true);

    const ltrack = loaded.timeline.trackById(trackId);
    expect(ltrack).not.toBeNull();
    expect(ltrack!.clipCount).toBe(3);
    expect(ltrack!.transitions.length).toBe(1);
    expect(ltrack!.transitions[0]!.transitionId).toBe('yave.trans.slide');
    expect(ltrack!.transitions[0]!.durationFrames).toBe(20);

    const first = ltrack!.clipAt(0);
    expect(first).not.toBeNull();
    expect(first!.filters.length).toBe(1);
    expect(first!.filters[0]!.params['contrast']).toBe(1.25);

    const ltitle = ltrack!.clipAt(300) as TitleClip;
    expect(ltitle).not.toBeNull();
    expect(ltitle.type).toBe('title');
    expect(ltitle.presetId).toBe('yave.title.lowerThird');

    expect(loaded.mediaFolders.folders.length).toBe(1);
    expect(loaded.mediaFolders.folders[0]!.name).toBe('Footage');
  });
});

