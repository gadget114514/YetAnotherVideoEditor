import { Rational, type RationalData } from './Rational.js';

/**
 * Formats frame number to SMPTE timecode (HH:MM:SS:FF or HH:MM:SS;FF for drop frame).
 */
export function formatTimecode(
  frame: number,
  tb: RationalData,
  dropFrame: boolean = false
): string {
  let fNum = Math.max(0, Math.trunc(frame));
  const fps = tb.num > 0 ? Math.round(tb.den / tb.num) : 30;

  let h = 0;
  let m = 0;
  let s = 0;
  let f = 0;

  if (dropFrame && (fps === 30 || fps === 60)) {
    if (fps === 30) {
      const m10 = Math.trunc(fNum / 17982);
      let d = fNum % 17982;
      let m1 = 0;
      if (d >= 1800) {
        m1 = 1 + Math.trunc((d - 1800) / 1798);
        d = ((d - 1800) % 1798) + 2;
      }
      const totalMinutes = m10 * 10 + m1;
      h = Math.trunc(totalMinutes / 60);
      m = totalMinutes % 60;
      s = Math.trunc(d / 30);
      f = d % 30;
    } else {
      const m10 = Math.trunc(fNum / 35964);
      let d = fNum % 35964;
      let m1 = 0;
      if (d >= 3600) {
        m1 = 1 + Math.trunc((d - 3600) / 3596);
        d = ((d - 3600) % 3596) + 4;
      }
      const totalMinutes = m10 * 10 + m1;
      h = Math.trunc(totalMinutes / 60);
      m = totalMinutes % 60;
      s = Math.trunc(d / 60);
      f = d % 60;
    }

    const hh = String(h).padStart(2, '0');
    const mm = String(m).padStart(2, '0');
    const ss = String(s).padStart(2, '0');
    const ff = String(f).padStart(2, '0');
    return `${hh}:${mm}:${ss};${ff}`;
  } else {
    const totalSeconds = Math.trunc(fNum / fps);
    f = fNum % fps;
    h = Math.trunc(totalSeconds / 3600);
    m = Math.trunc((totalSeconds % 3600) / 60);
    s = totalSeconds % 60;

    const hh = String(h).padStart(2, '0');
    const mm = String(m).padStart(2, '0');
    const ss = String(s).padStart(2, '0');
    const ff = String(f).padStart(2, '0');
    return `${hh}:${mm}:${ss}:${ff}`;
  }
}

/**
 * Parses SMPTE timecode (HH:MM:SS:FF or HH:MM:SS;FF) to frame number.
 */
export function parseTimecode(s: string, tb: RationalData): number | null {
  const match = /^(\d{1,2}):(\d{1,2}):(\d{1,2})([:;])(\d{1,2})$/.exec(s);
  if (!match) {
    return null;
  }

  const h = parseInt(match[1]!, 10);
  const m = parseInt(match[2]!, 10);
  const sec = parseInt(match[3]!, 10);
  const sep = match[4]!;
  const f = parseInt(match[5]!, 10);

  const fps = tb.num > 0 ? Math.round(tb.den / tb.num) : 30;
  const isDrop = sep === ';';

  if (isDrop && (fps === 30 || fps === 60)) {
    const totalMinutes = h * 60 + m;
    let frames = (h * 3600 + m * 60 + sec) * fps + f;
    if (fps === 30) {
      const dropped = 2 * (totalMinutes - Math.trunc(totalMinutes / 10));
      frames -= dropped;
    } else {
      const dropped = 4 * (totalMinutes - Math.trunc(totalMinutes / 10));
      frames -= dropped;
    }
    return frames;
  } else {
    return (h * 3600 + m * 60 + sec) * fps + f;
  }
}
