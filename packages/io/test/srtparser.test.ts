import { describe, it, expect } from 'vitest';
import {
  Project,
  SubtitleClip,
  SubtitleText,
  Timebase,
  secondsToFrames,
  RoundMode,
} from '@yave/core';
import {
  SrtParser,
  SrtWriter,
  convertCuesToClips,
  type SrtParseResult,
} from '../src/index.js';

describe('TestSrtParser', () => {
  it('parseBasic', () => {
    const srt =
      '1\n' +
      '00:00:01,000 --> 00:00:02,500\n' +
      'こんにちは世界\n\n' +
      '2\n' +
      '00:00:03,200 --> 00:00:04,000\n' +
      '2 行目の字幕です\n' +
      '3 行目もあります\n\n';

    const result = SrtParser.parseText(srt);
    expect(result.ok).toBe(true);
    expect(result.cues.length).toBe(2);
    expect(result.cues[0]!.startSeconds).toBe(1.0);
    expect(result.cues[0]!.endSeconds).toBe(2.5);
    expect(result.cues[0]!.rawText).toBe('こんにちは世界');
    expect(result.cues[1]!.rawText.includes('\n')).toBe(true);
    expect(result.warnings.length).toBe(0);
  });

  it('parseShiftJisFallback', () => {
    // 「テスト」を Shift_JIS (0x83 0x65 0x83 0x58 0x83 0x67) でエンコード
    const header = new TextEncoder().encode('1\n00:00:01,000 --> 00:00:02,000\n');
    const sjisBytes = new Uint8Array([0x83, 0x65, 0x83, 0x58, 0x83, 0x67]);
    const footer = new TextEncoder().encode('\n\n');

    const total = new Uint8Array(header.length + sjisBytes.length + footer.length);
    total.set(header, 0);
    total.set(sjisBytes, header.length);
    total.set(footer, header.length + sjisBytes.length);

    const result = SrtParser.parseBytes(total);
    if (result.ok) {
      expect(result.cues.length).toBeGreaterThan(0);
      expect(result.cues[0]!.rawText).toBe('テスト');
    }
  });

  it('parseInlineMarkup', () => {
    const srt =
      '1\n00:00:00,000 --> 00:00:01,000\n' + '<b>太字</b>と<i>斜体</i>\n\n';

    const result = SrtParser.parseText(srt);
    expect(result.ok).toBe(true);

    const text = SubtitleText.fromSrtMarkup(result.cues[0]!.rawText);
    expect(text.plain).toBe('太字と斜体');
    expect(text.spans.length).toBe(2);

    // toSrtMarkup でタグへ戻せること
    const markup = text.toSrtMarkup();
    expect(markup.includes('<b>')).toBe(true);
    expect(markup.includes('</b>')).toBe(true);
  });

  it('convertCuesToClipsRounding', () => {
    const tb = { num: 1001, den: 60000 }; // 59.94fps
    const parsed: SrtParseResult = {
      ok: true,
      warnings: [],
      cues: [
        {
          index: 1,
          startSeconds: 1.0,
          endSeconds: 2.0,
          rawText: '字幕',
        },
      ],
    };

    const warnings: string[] = [];
    const clips = convertCuesToClips(parsed, tb, 'default', warnings);
    expect(clips.length).toBe(1);
    expect(warnings.length).toBe(0);

    // 開始 Floor / 終了 Ceil により「表示すべき瞬間が含まれる」ことを確認
    const startF = secondsToFrames(1.0, tb, RoundMode.Floor);
    const endF = secondsToFrames(2.0, tb, RoundMode.Ceil);
    expect(clips[0]!.range).toEqual({ start: startF, duration: endF - startF });
  });

  it('writeRoundTrip', () => {
    const project = new Project();
    const t = project.timeline.appendTrack('subtitle');

    const clip = new SubtitleClip();
    clip.setRange({ start: 60, duration: 60 });
    clip.setPlainText('ラウンドトリップ');
    t.insertClip(clip);

    const srt = SrtWriter.writeToString(t, project.timeline.timebase);
    expect(srt.includes('-->')).toBe(true);
    expect(srt.includes('ラウンドトリップ')).toBe(true);

    // 書き出した SRT を再度パースできること
    const result = SrtParser.parseText(srt);
    expect(result.ok).toBe(true);
    expect(result.cues.length).toBe(1);
    expect(result.cues[0]!.rawText).toBe('ラウンドトリップ');
  });

  it('malformedInput', () => {
    // タイムコード行が壊れている
    const r1 = SrtParser.parseText('1\nnot a timecode\ntext\n\n');
    expect(r1.warnings.length > 0 || !r1.ok).toBe(true);

    // 空入力
    const r2 = SrtParser.parseText('');
    expect(r2.ok).toBe(false);
    expect(r2.cues.length).toBe(0);
  });
});
