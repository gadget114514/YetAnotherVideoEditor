import {
  type Rational,
  RoundMode,
  secondsToFrames,
  SubtitleClip,
  SubtitleText,
  createUuid,
} from '@yave/core';

export interface SrtCue {
  index: number;
  startSeconds: number;
  endSeconds: number;
  rawText: string;
}

export interface SrtParseResult {
  ok: boolean;
  cues: SrtCue[];
  warnings: string[];
}

export function parseSrtTimecode(
  line: string,
  out: { start: number; end: number },
): boolean {
  const re =
    /(\d{1,2}):(\d{1,2}):(\d{1,2})[,.](\d{1,3})\s*-->\s*(\d{1,2}):(\d{1,2}):(\d{1,2})[,.](\d{1,3})/;
  const m = re.exec(line.trim());
  if (!m) return false;

  const toSecs = (h: string, mi: string, s: string, ms: string) => {
    const msPadded = ms.padEnd(3, '0').substring(0, 3);
    return (
      parseInt(h, 10) * 3600 +
      parseInt(mi, 10) * 60 +
      parseInt(s, 10) +
      parseInt(msPadded, 10) / 1000
    );
  };

  out.start = toSecs(m[1]!, m[2]!, m[3]!, m[4]!);
  out.end = toSecs(m[5]!, m[6]!, m[7]!, m[8]!);
  return true;
}

export function decodeSrtBytes(bytes: Uint8Array): { text: string; ok: boolean } {
  // 1. BOM detection
  if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
    const utf8 = new TextDecoder('utf-8', { fatal: true });
    try {
      return { text: utf8.decode(bytes.subarray(3)), ok: true };
    } catch {
      // ignore
    }
  }

  if (bytes.length >= 2 && bytes[0] === 0xff && bytes[1] === 0xfe) {
    const utf16le = new TextDecoder('utf-16le', { fatal: true });
    try {
      return { text: utf16le.decode(bytes.subarray(2)), ok: true };
    } catch {
      // ignore
    }
  }

  if (bytes.length >= 2 && bytes[0] === 0xfe && bytes[1] === 0xff) {
    const utf16be = new TextDecoder('utf-16be', { fatal: true });
    try {
      return { text: utf16be.decode(bytes.subarray(2)), ok: true };
    } catch {
      // ignore
    }
  }

  // 2. Strict UTF-8 verification
  try {
    const utf8 = new TextDecoder('utf-8', { fatal: true });
    return { text: utf8.decode(bytes), ok: true };
  } catch {
    // 3. Fallback to Shift_JIS
    try {
      const sjis = new TextDecoder('shift-jis', { fatal: true });
      return { text: sjis.decode(bytes), ok: true };
    } catch {
      // Non-fatal UTF-8 fallback with replacement characters
      const fallback = new TextDecoder('utf-8');
      return { text: fallback.decode(bytes), ok: false };
    }
  }
}

export class SrtParser {
  static parseTimecodeLine(
    line: string,
    out: { start: number; end: number },
  ): boolean {
    return parseSrtTimecode(line, out);
  }

  static parseText(text: string): SrtParseResult {
    const result: SrtParseResult = {
      ok: false,
      cues: [],
      warnings: [],
    };

    if (!text || text.trim() === '') {
      return result;
    }

    const normalized = text.replace(/\r\n/g, '\n').replace(/\r/g, '\n');

    enum Section {
      Index,
      Timecode,
      Body,
    }
    let section = Section.Index;
    let currentIndex = 0;
    let startSec = 0.0;
    let endSec = 0.0;
    let body = '';

    const flushCue = () => {
      if (section !== Section.Body) return;
      const rawText = body.trim();
      const cue: SrtCue = {
        index: currentIndex,
        startSeconds: startSec,
        endSeconds: endSec,
        rawText,
      };

      if (cue.endSeconds <= cue.startSeconds) {
        result.warnings.push(
          `Cue #${cue.index} has zero or negative duration; skipped.`,
        );
      } else if (cue.rawText.length === 0) {
        result.warnings.push(`Cue #${cue.index} has empty text; skipped.`);
      } else {
        result.cues.push(cue);
      }

      section = Section.Index;
      body = '';
    };

    const lines = normalized.split('\n');
    for (const rawLine of lines) {
      const line = rawLine;

      switch (section) {
        case Section.Index: {
          const trimmed = line.trim();
          if (trimmed === '') continue;

          const num = parseInt(trimmed, 10);
          const tcOut = { start: 0, end: 0 };
          if (!isNaN(num) && String(num) === trimmed && num >= 0) {
            flushCue();
            currentIndex = num;
            section = Section.Timecode;
          } else if (parseSrtTimecode(trimmed, tcOut)) {
            currentIndex++;
            startSec = tcOut.start;
            endSec = tcOut.end;
            section = Section.Body;
          } else if (!trimmed.toLowerCase().startsWith('webvtt')) {
            flushCue();
          }
          break;
        }
        case Section.Timecode: {
          const tcOut = { start: 0, end: 0 };
          if (parseSrtTimecode(line, tcOut)) {
            startSec = tcOut.start;
            endSec = tcOut.end;
            section = Section.Body;
            body = '';
          } else {
            result.warnings.push(`Malformed timecode line: ${line.trim()}`);
            section = Section.Index;
          }
          break;
        }
        case Section.Body: {
          if (line.trim() === '') {
            flushCue();
          } else {
            if (body !== '') body += '\n';
            body += line;
          }
          break;
        }
      }
    }
    flushCue();

    // Check overlaps
    for (let i = 1; i < result.cues.length; i++) {
      if (result.cues[i]!.startSeconds < result.cues[i - 1]!.endSeconds) {
        result.warnings.push(`Cue #${result.cues[i]!.index} overlaps the previous cue.`);
      }
    }

    result.ok = result.cues.length > 0;
    return result;
  }

  static parseBytes(bytes: Uint8Array): SrtParseResult {
    const { text, ok } = decodeSrtBytes(bytes);
    const res = SrtParser.parseText(text);
    if (!ok) {
      res.warnings.push('Unsupported or corrupted text encoding; characters may be lost.');
    }
    return res;
  }
}

export function convertCuesToClips(
  parsed: SrtParseResult,
  timebase: Rational,
  stylePresetId: string = 'default',
  warningsOut?: string[],
): SubtitleClip[] {
  const clips: SubtitleClip[] = [];
  let prevEnd = -Infinity;

  for (const cue of parsed.cues) {
    const startF = secondsToFrames(cue.startSeconds, timebase, RoundMode.Floor);
    const endF = secondsToFrames(cue.endSeconds, timebase, RoundMode.Ceil);

    if (endF <= startF) {
      if (warningsOut) {
        warningsOut.push(`Cue #${cue.index} has zero or negative duration; skipped.`);
      }
      continue;
    }

    if (startF < prevEnd && warningsOut) {
      warningsOut.push(`Cue #${cue.index} overlaps the previous cue.`);
    }
    prevEnd = endF;

    const clip = new SubtitleClip();
    clip.setId(createUuid());
    clip.setRange({ start: startF, duration: endF - startF });
    clip.setText(SubtitleText.fromSrtMarkup(cue.rawText));
    clip.setStylePresetId(stylePresetId);
    clips.push(clip);
  }

  return clips;
}
