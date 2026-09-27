import {
  type Rational,
  type Track,
  framesToSeconds,
  SubtitleClip,
} from '@yave/core';

export function formatTimecode(seconds: number): string {
  if (seconds < 0.0) seconds = 0.0;
  const totalMs = Math.round(seconds * 1000.0);
  const h = Math.floor(totalMs / 3600000);
  const m = Math.floor((totalMs % 3600000) / 60000);
  const s = Math.floor((totalMs % 60000) / 1000);
  const ms = totalMs % 1000;

  const pad2 = (n: number) => String(n).padStart(2, '0');
  const pad3 = (n: number) => String(n).padStart(3, '0');

  return `${pad2(h)}:${pad2(m)}:${pad2(s)},${pad3(ms)}`;
}

export class SrtWriter {
  static writeToString(
    track: Track,
    timebase: Rational,
    warningsOut?: string[],
  ): string {
    const subs: SubtitleClip[] = [];
    for (const c of track.clips) {
      if (c instanceof SubtitleClip) {
        subs.push(c);
      } else if (warningsOut && c.range.duration > 0) {
        warningsOut.push(`Non-subtitle clip '${c.name}' was skipped.`);
      }
    }

    subs.sort((a, b) => a.range.start - b.range.start);

    let out = '';
    let index = 1;
    for (const sc of subs) {
      const r = sc.range;
      const startSec = framesToSeconds(r.start, timebase);
      const endSec = framesToSeconds(r.start + r.duration, timebase);

      out += `${index++}\n`;
      out += `${formatTimecode(startSec)} --> ${formatTimecode(endSec)}\n`;
      out += `${sc.text.toSrtMarkup()}\n\n`;

      if (sc.hasMissingEffects() && warningsOut) {
        warningsOut.push(
          `Clip '${sc.name}' has effects that cannot be stored in SRT; they will be lost.`,
        );
      }
    }

    return out;
  }
}
