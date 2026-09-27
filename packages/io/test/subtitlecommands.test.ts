import { describe, it, expect } from 'vitest';
import {
  Project,
  SubtitleClip,
  SubtitleStyleField,
  ImportSubtitleCommand,
  EditSubtitleTextCommand,
  SetSubtitleStyleCommand,
  OverlapPolicy,
} from '@yave/core';
import { SrtParser, convertCuesToClips } from '../src/index.js';

function clipsFromSrt(project: Project, srt: string): SubtitleClip[] {
  const parsed = SrtParser.parseText(srt);
  const warnings: string[] = [];
  return convertCuesToClips(
    parsed,
    project.timeline.timebase,
    'default',
    warnings,
  );
}

function findSubtitle(project: Project, text: string): SubtitleClip | null {
  const tl = project.timeline;
  for (let t = 0; t < tl.trackCount; ++t) {
    const track = tl.trackAt(t);
    if (!track) continue;
    for (const c of track.clips) {
      if (c instanceof SubtitleClip && c.plainText() === text) {
        return c;
      }
    }
  }
  return null;
}

describe('TestSubtitleCommands', () => {
  it('importSrtBulk', () => {
    const project = new Project();
    const srt =
      '1\n00:00:01,000 --> 00:00:02,000\n一つ目\n\n' +
      '2\n00:00:02,500 --> 00:00:03,500\n二つ目\n\n' +
      '3\n00:00:04,000 --> 00:00:05,000\n三つ目\n\n';

    const clips = clipsFromSrt(project, srt);
    expect(clips.length).toBe(3);

    const cmd = new ImportSubtitleCommand(
      project,
      clips,
      OverlapPolicy.SplitToNewTracks,
      -1,
      'subtitle',
      'test.srt',
    );
    project.undoStack.push(cmd);

    // 新しい字幕トラックが 1 本作られ、3 クリップが載っている
    const subs = project.timeline.tracksOfType('subtitle');
    expect(subs.length).toBe(1);
    expect(subs[0]!.clipCount).toBe(3);
    expect(cmd.insertedCount).toBe(3);
    expect(cmd.baseTrackIndex).toBe(0);

    // タイムスタンプが正しく変換されている
    const clip = findSubtitle(project, '一つ目');
    expect(clip).not.toBeNull();
    expect(clip!.range.start).toBeGreaterThan(0);
  });

  it('importSrtOverlapPolicy', () => {
    // 重なるキュー: 1〜3 秒 と 2〜4 秒
    const srt =
      '1\n00:00:01,000 --> 00:00:03,000\n先頭\n\n' +
      '2\n00:00:02,000 --> 00:00:04,000\n重なり\n\n';

    // 字幕トラックは重なりを許容するため、policy に関わらず 1 トラックへ全キューが入る
    {
      const p = new Project();
      const clips = clipsFromSrt(p, srt);
      const cmd = new ImportSubtitleCommand(
        p,
        clips,
        OverlapPolicy.SplitToNewTracks,
        -1,
        'subtitle',
        'test.srt',
      );
      p.undoStack.push(cmd);
      const subs = p.timeline.tracksOfType('subtitle');
      expect(subs.length).toBe(1);
      expect(subs[0]!.clipCount).toBe(2);
      expect(cmd.insertedCount).toBe(2);
    }

    // SkipOverlapping も字幕トラックでは無視され、全キューが入る
    {
      const p = new Project();
      const clips = clipsFromSrt(p, srt);
      const cmd = new ImportSubtitleCommand(
        p,
        clips,
        OverlapPolicy.SkipOverlapping,
        -1,
        'subtitle',
        'test.srt',
      );
      p.undoStack.push(cmd);
      const subs = p.timeline.tracksOfType('subtitle');
      expect(subs.length).toBe(1);
      expect(subs[0]!.clipCount).toBe(2);
      expect(cmd.insertedCount).toBe(2);
    }
  });

  it('importUndoRestores', () => {
    const project = new Project();
    const srt = '1\n00:00:01,000 --> 00:00:02,000\nあ\n\n';
    const clips = clipsFromSrt(project, srt);
    project.undoStack.push(
      new ImportSubtitleCommand(
        project,
        clips,
        OverlapPolicy.SplitToNewTracks,
        -1,
        'subtitle',
        'test.srt',
      ),
    );

    expect(project.timeline.trackCount).toBe(1);

    project.undoStack.undo();
    expect(project.timeline.trackCount).toBe(0);

    project.undoStack.redo();
    expect(project.timeline.trackCount).toBe(1);
    expect(project.timeline.trackAt(0)!.clipCount).toBe(1);
  });

  it('editText', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('subtitle');
    const clip = new SubtitleClip();
    clip.setRange({ start: 0, duration: 100 });
    clip.setPlainText('元の文章');
    t.insertClip(clip);

    const id = clip.id;
    project.undoStack.push(new EditSubtitleTextCommand(project, id, '編集後'));

    let sc = findSubtitle(project, '編集後');
    expect(sc).not.toBeNull();

    project.undoStack.undo();
    expect(findSubtitle(project, '元の文章')).not.toBeNull();

    project.undoStack.redo();
    expect(findSubtitle(project, '編集後')).not.toBeNull();
  });

  it('editTextMerge', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('subtitle');
    const clip = new SubtitleClip();
    clip.setRange({ start: 0, duration: 100 });
    clip.setPlainText('');
    t.insertClip(clip);

    const id = clip.id;
    project.undoStack.push(new EditSubtitleTextCommand(project, id, 'a'));
    project.undoStack.push(new EditSubtitleTextCommand(project, id, 'ab'));
    project.undoStack.push(new EditSubtitleTextCommand(project, id, 'abc'));

    // 連続入力は 1 コマンドにマージされ、1 回の Undo で消える
    project.undoStack.undo();
    expect(findSubtitle(project, '')).not.toBeNull();
  });

  it('setStyleField', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('subtitle');
    const clip = new SubtitleClip();
    clip.setRange({ start: 0, duration: 100 });
    clip.setPlainText('スタイル');
    t.insertClip(clip);

    const id = clip.id;
    project.undoStack.push(
      new SetSubtitleStyleCommand(
        project,
        id,
        SubtitleStyleField.FontPointSize,
        96.0,
      ),
    );
    project.undoStack.push(
      new SetSubtitleStyleCommand(
        project,
        id,
        SubtitleStyleField.FillColor,
        '#ff0000',
      ),
    );

    let sc = findSubtitle(project, 'スタイル');
    expect(sc).not.toBeNull();
    const d = sc!.styleOverride;
    expect(d.fontPointSize).toBe(96.0);
    expect(d.fillColor).toBe('#ff0000');

    // Undo で戻る
    project.undoStack.undo();
    sc = findSubtitle(project, 'スタイル');
    expect(sc!.styleOverride.fillColor).toBeUndefined();
  });

  it('setStyleUndo', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('subtitle');
    const clip = new SubtitleClip();
    clip.setRange({ start: 0, duration: 100 });
    clip.setPlainText('undo');
    t.insertClip(clip);

    const id = clip.id;
    project.undoStack.push(
      new SetSubtitleStyleCommand(
        project,
        id,
        SubtitleStyleField.FontFamily,
        'Impact',
      ),
    );

    let sc = findSubtitle(project, 'undo');
    expect(sc!.styleOverride.fontFamily).toBe('Impact');

    project.undoStack.undo();
    sc = findSubtitle(project, 'undo');
    expect(sc!.styleOverride.fontFamily).toBeUndefined();
  });
});
