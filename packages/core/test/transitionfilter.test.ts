import { describe, it, expect } from 'vitest';
import {
  Project,
  Track,
  VideoClip,
  TransitionInstance,
  builtinFilter,
  builtinTransition,
  resolveFilterParams,
  AddFilterCommand,
  AddTransitionCommand,
} from '../src/index.js';

function makeClip(
  start: number,
  duration: number,
  sourceOffset: number,
  sourceLength: number,
): VideoClip {
  const c = new VideoClip();
  c.setRange({ start, duration });
  c.setSourceOffset(sourceOffset);
  c.setMaxDurationFrames(sourceLength);
  return c;
}

describe('TestTransitionFilter', () => {
  it('filterStackOrder', () => {
    const clip = makeClip(0, 100, 0, 100);

    clip.addFilter({ filterId: builtinFilter.kBlur, params: {}, enabled: true });
    clip.addFilter({ filterId: builtinFilter.kMono, params: {}, enabled: true });
    expect(clip.filters.length).toBe(2);

    clip.moveFilter(1, 0);
    expect(clip.filters[0]!.filterId).toBe(builtinFilter.kMono);

    // 無効な段は解決結果から落ちる
    clip.setFilterEnabled(0, false);
    expect(clip.resolvedFilters().length).toBe(1);
    expect(clip.resolvedFilters()[0]!.filterId).toBe(builtinFilter.kBlur);

    // clone はフィルタも複製する
    const copy = clip.clone();
    expect(copy.filters.length).toBe(2);
    expect(copy.id).not.toBe(clip.id);
  });

  it('filterParamsResolve', () => {
    const inst = {
      filterId: builtinFilter.kColorAdjust,
      params: { brightness: 0.25, contrast: 1.5, saturation: 0.5, gamma: 2.0 },
    };

    const p = resolveFilterParams(inst);
    expect(p[0]).toBe(0.25);
    expect(p[1]).toBe(1.5);
    expect(p[2]).toBe(0.5);
    expect(p[3]).toBe(2.0);

    // 未知のフィルタは全 0
    const unknown = { filterId: 'nope', params: {} };
    expect(resolveFilterParams(unknown)[0]).toBe(0.0);
  });

  it('transitionNeedsBoundary', () => {
    const track = new Track('video');
    expect(track.insertClip(makeClip(0, 100, 30, 300))).toBe(true);
    expect(track.insertClip(makeClip(100, 100, 30, 300))).toBe(true);

    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kDissolve;
    t.centerFrame = 50; // 境界ではない
    t.durationFrames = 20;

    const err = { error: '' };
    expect(track.addTransition(t, err)).toBe(false);
    expect(err.error).not.toBe('');
    expect(track.transitions.length).toBe(0);

    t.centerFrame = 100; // ちょうど境界
    expect(track.addTransition(t, err)).toBe(true);
    expect(track.transitions.length).toBe(1);
    expect(track.transitions[0]!.fromClipId).toBe(track.clips[0]!.id);
    expect(track.transitions[0]!.toClipId).toBe(track.clips[1]!.id);
  });

  it('transitionClampedToHandles', () => {
    const track = new Track('video');
    // 前のクリップは out 点の後ろに 10 フレームしか残っていない
    expect(track.insertClip(makeClip(0, 100, 0, 110))).toBe(true);
    expect(track.insertClip(makeClip(100, 100, 50, 300))).toBe(true);

    expect(track.maxTransitionDuration(100)).toBe(20); // min(10, 50) * 2

    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kDissolve;
    t.centerFrame = 100;
    t.durationFrames = 120; // ハンドルを大きく超える

    expect(track.addTransition(t)).toBe(true);
    expect(track.transitions[0]!.durationFrames).toBe(20); // 縮められる
  });

  it('transitionRejectedWithoutHandles', () => {
    const track = new Track('video');
    // 両側ともソースを使い切っている = ハンドルが無い
    expect(track.insertClip(makeClip(0, 100, 0, 100))).toBe(true);
    expect(track.insertClip(makeClip(100, 100, 0, 100))).toBe(true);

    expect(track.maxTransitionDuration(100)).toBe(0);

    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kDissolve;
    t.centerFrame = 100;
    t.durationFrames = 30;

    const err = { error: '' };
    expect(track.addTransition(t, err)).toBe(false);
    expect(err.error).not.toBe('');
  });

  it('transitionDroppedWhenClipRemoved', () => {
    const track = new Track('video');
    const a = makeClip(0, 100, 30, 300);
    const b = makeClip(100, 100, 30, 300);
    expect(track.insertClip(a)).toBe(true);
    expect(track.insertClip(b)).toBe(true);

    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kDissolve;
    t.centerFrame = 100;
    t.durationFrames = 20;
    expect(track.addTransition(t)).toBe(true);

    // 片側を消すと境界そのものが無くなるので、トランジションも消える
    track.removeClip(b.id);
    expect(track.transitions.length).toBe(0);
  });

  it('transitionProgress', () => {
    const t = new TransitionInstance();
    t.centerFrame = 100;
    t.durationFrames = 20;

    expect(t.startFrame()).toBe(90);
    expect(t.endFrame()).toBe(110);
    expect(t.contains(90)).toBe(true);
    expect(t.contains(110)).toBe(false);
    expect(t.progressAt(90)).toBe(0.0);
    expect(t.progressAt(100)).toBe(0.5);
    expect(t.progressAt(200)).toBe(1.0); // 区間外はクランプ
  });

  it('snapshotEmitsPairDuringTransition', () => {
    const project = new Project();
    const tl = project.timeline;
    const track = tl.appendTrack('video');

    expect(track.insertClip(makeClip(0, 100, 30, 300))).toBe(true);
    expect(track.insertClip(makeClip(100, 100, 30, 300))).toBe(true);

    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kWipe;
    t.centerFrame = 100;
    t.durationFrames = 20;
    expect(track.addTransition(t)).toBe(true);

    // 区間の外: 1 レイヤー
    const outside = tl.buildSnapshot(50);
    expect(outside.layers.length).toBe(1);
    expect(outside.layers[0]!.transition).toBeUndefined();

    // 区間の中: from / to の 2 レイヤーが同じ zIndex で出る (3.10)
    const inside = tl.buildSnapshot(95);
    expect(inside.layers.length).toBe(2);
    expect(inside.layers[0]!.transition).toBeDefined();
    expect(inside.layers[1]!.transition).toBeDefined();
    expect(inside.layers[0]!.zIndex).toBe(inside.layers[1]!.zIndex);
    expect(inside.layers[0]!.transition!.isIncoming).toBe(false);
    expect(inside.layers[1]!.transition!.isIncoming).toBe(true);
    expect(inside.layers[0]!.transition!.shaderMode).toBe(2); // wipe
  });

  it('commandsAreUndoable', () => {
    const project = new Project();
    const tl = project.timeline;
    const track = tl.appendTrack('video');

    const a = makeClip(0, 100, 30, 300);
    const b = makeClip(100, 100, 30, 300);
    expect(track.insertClip(a)).toBe(true);
    expect(track.insertClip(b)).toBe(true);

    // --- フィルタ ---
    const inst = { filterId: builtinFilter.kBlur, params: {} };
    project.undoStack.push(new AddFilterCommand(project, a.id, inst));
    expect(a.filters.length).toBe(1);

    project.undoStack.undo();
    expect(a.filters.length).toBe(0);
    project.undoStack.redo();
    expect(a.filters.length).toBe(1);

    // --- トランジション ---
    const t = new TransitionInstance();
    t.transitionId = builtinTransition.kDissolve;
    t.centerFrame = 100;
    t.durationFrames = 20;
    project.undoStack.push(new AddTransitionCommand(project, track.id, t));
    expect(track.transitions.length).toBe(1);

    project.undoStack.undo();
    expect(track.transitions.length).toBe(0);
    project.undoStack.redo();
    expect(track.transitions.length).toBe(1);
  });
});
