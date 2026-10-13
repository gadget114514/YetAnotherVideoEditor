import { describe, it, expect } from 'vitest';
import { Project } from '@yave/core';
import { AiOrchestrator, type IGenerationProvider } from '../src/ai/AiOrchestrator.js';
import {
  alignLyrics,
  formatSrtTimestamp,
  normalizeChars,
  splitLyricsLines,
  type RecognizedCue,
} from '../src/ai/LyricsAligner.js';

const cue = (s: number, e: number, text: string): RecognizedCue => ({ startSeconds: s, endSeconds: e, text });
const lyricsOf = (...lines: string[]): string => lines.join(String.fromCharCode(10));

describe('normalizeChars', () => {
  it('unifies katakana/hiragana, width, case and drops punctuation', () => {
    expect(normalizeChars('ＡＢｃ、カタカナ! ひらがな').join('')).toBe('abcかたかなひらがな');
  });
});

describe('splitLyricsLines', () => {
  it('drops blank lines and trims', () => {
    expect(splitLyricsLines('  a \r\n\r\n b\n')).toEqual(['a', 'b']);
  });
});

describe('alignLyrics', () => {
  it('replaces misrecognised text with the correct lyrics and keeps recognised timing', () => {
    const cues = [
      cue(1.0, 3.0, 'きらきらひかる おそらのほし'),
      cue(4.0, 6.0, 'まばたきしては みんなをみてる'),
    ];
    const lyrics = ['きらきら光る おそらの星', 'まばたきしては みんなを見てる'];
    const r = alignLyrics(cues, lyrics);

    expect(r.lines.map((l) => l.text)).toEqual(lyrics);
    expect(r.lines[0]!.startSeconds).toBeCloseTo(1.0, 1);
    expect(r.lines[0]!.endSeconds).toBeLessThanOrEqual(3.1);
    expect(r.lines[1]!.startSeconds).toBeGreaterThanOrEqual(3.9);
    expect(r.lines[1]!.endSeconds).toBeCloseTo(6.0, 1);
    expect(r.lines.every((l) => l.anchored)).toBe(true);
    expect(r.srt).toContain('きらきら光る おそらの星');
    expect(r.srt).toContain('00:00:01,');
  });

  it('interpolates lines the recogniser missed between their neighbours', () => {
    const cues = [cue(0, 2, 'あいうえお'), cue(10, 12, 'さしすせそ')];
    const r = alignLyrics(cues, ['あいうえお', 'かきくけこ', 'さしすせそ']);
    const mid = r.lines[1]!;
    expect(mid.anchored).toBe(false);
    expect(mid.startSeconds).toBeGreaterThanOrEqual(r.lines[0]!.endSeconds - 1e-6);
    expect(mid.endSeconds).toBeLessThanOrEqual(r.lines[2]!.startSeconds + 1e-6);
    expect(mid.confidence).toBe(0);
  });

  it('handles repeated choruses monotonically', () => {
    const cues = [cue(0, 2, 'らららりらら'), cue(5, 7, 'めろでぃー'), cue(10, 12, 'らららりらら')];
    const r = alignLyrics(cues, ['らららりらら', 'めろでぃー', 'らららりらら']);
    const starts = r.lines.map((l) => l.startSeconds);
    expect(starts[0]).toBeCloseTo(0, 1);
    expect(starts[1]).toBeCloseTo(5, 1);
    expect(starts[2]).toBeCloseTo(10, 1);
  });

  it('lays lines out proportionally when nothing was recognised', () => {
    const r = alignLyrics([], ['あいう', 'えお']);
    expect(r.lines).toHaveLength(2);
    expect(r.lines[0]!.anchored).toBe(false);
    expect(r.lines[1]!.startSeconds).toBeGreaterThanOrEqual(r.lines[0]!.endSeconds - 1e-6);
    expect(r.lines[1]!.endSeconds).toBeGreaterThan(r.lines[1]!.startSeconds);
  });

  it('never produces overlapping or inverted lines', () => {
    const cues = [cue(0, 1, 'あ'), cue(0.5, 1.5, 'い'), cue(1, 2, 'う')];
    const r = alignLyrics(cues, ['あ', 'い', 'う', 'え']);
    for (let i = 0; i < r.lines.length; i++) {
      expect(r.lines[i]!.endSeconds).toBeGreaterThan(r.lines[i]!.startSeconds);
      if (i > 0) expect(r.lines[i]!.startSeconds).toBeGreaterThanOrEqual(r.lines[i - 1]!.endSeconds - 1e-6);
    }
  });

  it('keeps an instrumental interlude out of the lines around it', () => {
    // verse 1 sung 0-4 s, ~16 s interlude, verse 2 sung 20-24 s
    const cues = [cue(0, 2, 'あいうえお'), cue(2, 4, 'かきくけこ'), cue(20, 22, 'さしすせそ'), cue(22, 24, 'たちつてと')];
    const r = alignLyrics(cues, lyricsOf('あいうえお', 'かきくけこ', '', 'さしすせそ', 'たちつてと'));
    expect(r.lines[1]!.endSeconds).toBeLessThanOrEqual(4.1);
    expect(r.lines[2]!.startSeconds).toBeGreaterThanOrEqual(19.9);
    expect(r.lines[2]!.startSeconds).toBeCloseTo(20, 0);
  });

  it('places missed lines after a stanza break instead of stretching across the interlude', () => {
    // second stanza line "なにぬねの" was not recognised; stanza break precedes it
    const cues = [cue(0, 2, 'あいうえお'), cue(30, 32, 'さしすせそ')];
    const lyrics = lyricsOf('あいうえお', '', 'なにぬねの', 'さしすせそ');
    const r = alignLyrics(cues, lyrics);
    const missed = r.lines[1]!;
    expect(missed.anchored).toBe(false);
    // the interlude (spare time) sits before the missed line, which then butts up against the next line
    expect(missed.startSeconds).toBeGreaterThan(10);
    expect(missed.endSeconds).toBeLessThanOrEqual(30.1);
    expect(missed.endSeconds - missed.startSeconds).toBeLessThan(5);
  });

  it('ignores hallucinated text that does not match the lyrics', () => {
    const cues = [
      cue(0, 2, 'あいうえお'),
      cue(8, 12, 'ごしちょうありがとうございました'),
      cue(20, 22, 'さしすせそ'),
    ];
    const r = alignLyrics(cues, lyricsOf('あいうえお', 'さしすせそ'));
    expect(r.lines[0]!.startSeconds).toBeCloseTo(0, 1);
    expect(r.lines[0]!.endSeconds).toBeLessThanOrEqual(2.1);
    expect(r.lines[1]!.startSeconds).toBeCloseTo(20, 1);
  });

  it('splits long lines into several cues with their own timing', () => {
    const line = 'きらきらひかる おそらのほし まばたきしては みんなをみてる';
    const cues = [cue(0, 8, 'きらきらひかる おそらのほし'), cue(8, 16, 'まばたきしては みんなをみてる')];
    const r = alignLyrics(cues, line, { maxCharsPerCue: 8 });
    expect(r.lines).toHaveLength(1);
    expect(r.cues.length).toBeGreaterThanOrEqual(3);
    expect(r.cues.map((c) => c.text).join(' ')).toBe(line);
    for (let i = 1; i < r.cues.length; i++) {
      expect(r.cues[i]!.startSeconds).toBeGreaterThanOrEqual(r.cues[i - 1]!.startSeconds);
    }
    expect(r.cues[0]!.text.length).toBeLessThanOrEqual(16);
    expect(r.cues.at(-1)!.endSeconds).toBeCloseTo(16, 0);
    expect(r.srt.split('-->').length - 1).toBe(r.cues.length);
  });

  it('formats SRT timestamps', () => {
    expect(formatSrtTimestamp(3661.5)).toBe('01:01:01,500');
  });
});

describe('AiOrchestrator lyrics option', () => {
  it('replaces provider output with the supplied lyrics and exposes the report', async () => {
    const provider: IGenerationProvider = {
      id: 'fake-stt',
      name: 'fake',
      async generate() {
        return {
          assetId: 'a' as any,
          mediaType: 'subtitle',
          srtContent:
            '1\n00:00:01,000 --> 00:00:03,000\nきらきらひかる\n\n2\n00:00:04,000 --> 00:00:06,000\nおそらのほし\n',
        };
      },
    };
    const orchestrator = new AiOrchestrator();
    orchestrator.registerProvider(provider);
    const res = await orchestrator.importAudioAndTranscribe({
      project: new Project(),
      audioName: 'song.wav',
      durationFrames: 600,
      providerId: 'fake-stt',
      lyrics: 'きらきら光る\nお空の星',
    });
    expect(res.srtContent).toContain('きらきら光る');
    expect(res.srtContent).not.toContain('きらきらひかる');
    expect(res.lyricsAlignment?.lines).toHaveLength(2);
    expect(res.subtitleClips).toHaveLength(2);
    expect((res.subtitleClips[0] as any).plainText()).toBe('きらきら光る');
  });

  it('uses token-level timing from the provider when available', async () => {
    const provider: IGenerationProvider = {
      id: 'token-stt',
      name: 'token',
      async generate() {
        return {
          assetId: 'b' as any,
          mediaType: 'subtitle',
          // coarse SRT says 0-10 s, but tokens reveal the real singing 4-6 s
          srtContent: '1\n00:00:00,000 --> 00:00:10,000\nあいう\n',
          data: {
            tokens: [
              { text: 'あ', start: 4, end: 4.7 },
              { text: 'い', start: 4.7, end: 5.3 },
              { text: 'う', start: 5.3, end: 6 },
            ],
          },
        };
      },
    };
    const orchestrator = new AiOrchestrator();
    orchestrator.registerProvider(provider);
    const res = await orchestrator.importAudioAndTranscribe({
      project: new Project(),
      audioName: 'song.wav',
      durationFrames: 900,
      providerId: 'token-stt',
      lyrics: 'あいう',
    });
    const line = res.lyricsAlignment!.lines[0]!;
    expect(line.startSeconds).toBeCloseTo(4, 1);
    expect(line.endSeconds).toBeCloseTo(6, 1);
  });
});
