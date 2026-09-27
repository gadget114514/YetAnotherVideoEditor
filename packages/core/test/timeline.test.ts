import { describe, it, expect } from 'vitest';
import {
  Project,
  VideoClip,
  BlendMode,
  AddClipCommand,
  AddTrackCommand,
  SplitClipCommand,
  RippleDeleteCommand,
} from '../src/index.js';

function makeClip(start: number, duration: number): VideoClip {
  const c = new VideoClip();
  c.setRange({ start, duration });
  return c;
}

describe('TestTimeline', () => {
  it('trackInsertRemove', () => {
    const project = new Project();
    const tl = project.timeline;

    const v = tl.appendTrack('video');
    const a = tl.appendTrack('audio');
    expect(tl.trackCount).toBe(2);
    expect(tl.indexOfTrack(v)).toBe(0);
    expect(tl.indexOfTrack(a)).toBe(1);

    // Z オーダー: index 0 が最背面
    const removed = tl.takeTrackById(a.id);
    expect(removed).not.toBeNull();
    expect(tl.trackCount).toBe(1);

    // 元の位置へ戻す
    tl.reinsertTrack(1, removed!);
    expect(tl.trackCount).toBe(2);
    expect(tl.trackById(a.id)).not.toBeNull();
  });

  it('clipOverlapRejection', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');

    expect(t.insertClip(makeClip(0, 100))).toBe(true);
    expect(t.insertClip(makeClip(50, 100))).toBe(false); // 完全重複 -> 拒否
    expect(t.insertClip(makeClip(-50, 60))).toBe(false); // 前方掛け -> 拒否
    expect(t.insertClip(makeClip(90, 20))).toBe(false); // 後方掛け -> 拒否
    expect(t.insertClip(makeClip(100, 50))).toBe(true); // 隣接 -> OK (半開区間)
    expect(t.clipCount).toBe(2);

    // 空クリップは拒否
    expect(t.insertClip(makeClip(200, 0))).toBe(false);
  });

  it('clipAtBinarySearch', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');

    for (let i = 0; i < 10; ++i) {
      expect(t.insertClip(makeClip(i * 100, 100))).toBe(true);
    }

    for (let f = 0; f < 1000; ++f) {
      const c = t.clipAt(f);
      expect(c).not.toBeNull();
      expect(c!.range.start).toBe(Math.floor(f / 100) * 100);
    }
    expect(t.clipAt(1000)).toBeNull(); // 半開区間の終端

    expect(t.contentDuration()).toBe(1000);
    expect(t.nextClipStart(-1)).toBe(0);
    expect(t.prevClipEnd(500)).toBe(500);
  });

  it('clipsIn', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('video');

    expect(t.insertClip(makeClip(0, 50))).toBe(true);
    expect(t.insertClip(makeClip(50, 50))).toBe(true);
    expect(t.insertClip(makeClip(200, 100))).toBe(true);

    const inRange = t.clipsIn({ start: 40, duration: 30 });
    expect(inRange.length).toBe(2); // [0,50) と [50,100)

    const none = t.clipsIn({ start: 150, duration: 40 });
    expect(none.length).toBe(0);
  });

  it('moveTrackZOrder', () => {
    const project = new Project();
    const tl = project.timeline;

    tl.appendTrack('video'); // A (index 0)
    tl.appendTrack('video'); // B (index 1)
    tl.appendTrack('video'); // C (index 2)

    const a = tl.trackAt(0)!;
    const c = tl.trackAt(2)!;

    tl.moveTrack(0, 2); // A を最前面へ
    expect(tl.trackAt(2)).toBe(a);
    expect(tl.indexOfTrack(c)).toBe(1);

    // A を元へ戻す
    const moved = tl.takeTrack(2)!;
    tl.reinsertTrack(0, moved);
    expect(tl.trackAt(0)).toBe(a);
    expect(tl.trackAt(2)).toBe(c);
  });

  it('snapshotBuilding', () => {
    const project = new Project();
    const tl = project.timeline;

    const bg = tl.appendTrack('video');
    const clip = makeClip(0, 1000);
    clip.opacity = 0.5;
    clip.blendMode = BlendMode.Add;
    expect(bg.insertClip(clip)).toBe(true);

    tl.appendTrack('audio'); // 音声はスナップショット外

    const snap = tl.buildSnapshot(500);
    expect(snap.frameIndex).toBe(500);
    expect(snap.timebase.num).toBe(tl.timebase.num);
    expect(snap.layers.length).toBe(1);
    expect(snap.layers[0]!.zIndex).toBe(0); // 最初の可視トラックが z=0
    expect(snap.layers[0]!.blendMode).toBe(BlendMode.Add);
    expect(snap.layers[0]!.opacity).toBe(0.5);

    // 範囲外フレームにはレイヤーがない
    const emptySnap = tl.buildSnapshot(99999);
    expect(emptySnap.layers.length).toBe(0);
  });

  it('splitCommandUndoRedo', () => {
    const project = new Project();
    const tl = project.timeline;
    const t = tl.appendTrack('video');

    const original = makeClip(0, 1000);
    original.setSourceOffset(500);
    original.name = 'orig';
    const originalId = original.id;

    const addCmd = new AddClipCommand(project, t.id, 0, original);
    project.undoStack.push(addCmd);
    expect(t.clipCount).toBe(1);

    project.undoStack.push(new SplitClipCommand(project, t.id, originalId, 400));
    expect(t.clipCount).toBe(2);
    expect(t.clipById(originalId)).toBeNull(); // 分割後は id 不在

    // 左右の範囲とソースオフセットを検証
    const clips = t.clips;
    expect(clips[0]!.range).toEqual({ start: 0, duration: 400 });
    expect(clips[1]!.range).toEqual({ start: 400, duration: 600 });
    expect(clips[1]!.sourceOffset()).toBe(900); // 500 + 400

    // ---- Undo ----
    project.undoStack.undo();
    expect(t.clipCount).toBe(1);
    expect(t.clipById(originalId)).not.toBeNull();
    expect(t.clipById(originalId)!.range).toEqual({ start: 0, duration: 1000 });

    // ---- Redo ----
    project.undoStack.redo();
    expect(t.clipCount).toBe(2);
  });

  it('rippleDelete', () => {
    const project = new Project();
    const tl = project.timeline;
    const t = tl.appendTrack('video');

    expect(t.insertClip(makeClip(0, 100))).toBe(true);
    expect(t.insertClip(makeClip(100, 100))).toBe(true);
    expect(t.insertClip(makeClip(200, 100))).toBe(true);

    const tracks = [0];
    project.undoStack.push(new RippleDeleteCommand(project, tracks, { start: 100, duration: 100 }));

    // 中央が削除され、後続が詰められる
    expect(t.clipCount).toBe(2);
    expect(t.clips[1]!.range).toEqual({ start: 100, duration: 100 });

    // Undo で元通り
    project.undoStack.undo();
    expect(t.clipCount).toBe(3);
    expect(t.clips[1]!.range).toEqual({ start: 100, duration: 100 });
    expect(t.clips[2]!.range).toEqual({ start: 200, duration: 100 });
  });

  it('undoStackIntegration', () => {
    const project = new Project();
    const tl = project.timeline;
    expect(tl.trackCount).toBe(0);

    // トラック追加も Undo 可能
    project.undoStack.push(new AddTrackCommand(project, 'video', 0));
    expect(tl.trackCount).toBe(1);

    project.undoStack.undo();
    expect(tl.trackCount).toBe(0);

    project.undoStack.redo();
    expect(tl.trackCount).toBe(1);
  });
});
