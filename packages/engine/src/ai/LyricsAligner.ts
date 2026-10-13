/**
 * Forced alignment of user-supplied "correct" lyrics against a recognition result, producing
 * re-timed subtitle cues (one per lyric line, split when too long).
 *
 * Recognised units (Whisper tokens when available, otherwise whole cues whose characters are spread
 * evenly) give a timed character stream. A global sequence alignment (Needleman-Wunsch) maps every
 * lyric character onto that stream, so each lyric line inherits the time span of the audio it matched.
 *
 * Interludes: only trustworthy matches become time anchors, so instrumental gaps between anchored lines
 * are preserved instead of being stretched over. Lines the recogniser missed are placed inside the gap
 * at the natural singing rate, with any spare time assigned to stanza breaks (blank lines in the lyrics).
 */

export interface RecognizedCue {
  readonly startSeconds: number;
  readonly endSeconds: number;
  readonly text: string;
}

export interface AlignedLyricLine {
  readonly index: number;
  readonly text: string;
  readonly startSeconds: number;
  readonly endSeconds: number;
  /** 0..1: share of the line's characters that matched the recognised text exactly. */
  readonly confidence: number;
  /** false when timing was inferred because nothing in the line could be anchored to the audio. */
  readonly anchored: boolean;
}

export interface LyricCue {
  readonly text: string;
  readonly startSeconds: number;
  readonly endSeconds: number;
  /** index of the lyric line this cue came from (a long line yields several cues) */
  readonly lineIndex: number;
}

export interface LyricsAlignmentResult {
  readonly lines: AlignedLyricLine[];
  readonly cues: LyricCue[];
  readonly srt: string;
  readonly averageConfidence: number;
  readonly recognizedChars: number;
}

export interface AlignLyricsOptions {
  /** Minimum duration of any output cue (default 0.4 s). */
  readonly minCueSeconds?: number;
  /** Seconds per character used when timing has to be invented and nothing was anchored (default 0.25 s). */
  readonly fallbackSecondsPerChar?: number;
  /** Split lines longer than this many (normalised) characters (default 24). */
  readonly maxCharsPerCue?: number;
  /** Split cues longer than this many seconds (default 8). */
  readonly maxCueSeconds?: number;
  /** Largest DP table (n*m) to compute; above it lines are laid out proportionally (default 36M). */
  readonly maxCells?: number;
}

const MATCH = 2;
const MISMATCH = -1;
const GAP = -1;
/** A mismatched pair only counts as a timing anchor when an exact match lies this close (in lyric chars). */
const ANCHOR_NEIGHBOURHOOD = 3;
const BREAK_AFTER = /[\s、。，．,.!?！？…・」』）)\]】〉》]/u;

/** Normalises text for comparison: NFKC, lowercase, katakana → hiragana; drops anything but letters/digits. */
export function normalizeChars(text: string): string[] {
  const out: string[] = [];
  for (const ch of text) {
    for (let c of ch.normalize('NFKC').toLowerCase()) {
      if (!/[\p{L}\p{N}]/u.test(c)) continue;
      const code = c.codePointAt(0)!;
      if (code >= 0x30a1 && code <= 0x30f6) c = String.fromCodePoint(code - 0x60);
      out.push(c);
    }
  }
  return out;
}

export interface LyricLineInput {
  readonly text: string;
  /** stanza number; changes at blank lines */
  readonly paragraph: number;
}

export function parseLyrics(lyrics: string): LyricLineInput[] {
  const out: LyricLineInput[] = [];
  let paragraph = 0;
  let pendingBreak = false;
  for (const raw of lyrics.replace(/\r\n?/g, '\n').split('\n')) {
    const text = raw.trim();
    if (!text) {
      if (out.length) pendingBreak = true;
      continue;
    }
    if (pendingBreak) {
      paragraph++;
      pendingBreak = false;
    }
    out.push({ text, paragraph });
  }
  return out;
}

export function splitLyricsLines(lyrics: string): string[] {
  return parseLyrics(lyrics).map((l) => l.text);
}

export function formatSrtTimestamp(seconds: number): string {
  const ms = Math.max(0, Math.round(seconds * 1000));
  const h = Math.floor(ms / 3600000);
  const m = Math.floor((ms % 3600000) / 60000);
  const s = Math.floor((ms % 60000) / 1000);
  const r = ms % 1000;
  const p = (n: number, w = 2) => String(n).padStart(w, '0');
  return `${p(h)}:${p(m)}:${p(s)},${p(r, 3)}`;
}

export function cuesToSrt(cues: readonly LyricCue[]): string {
  return (
    cues
      .map((c, i) => `${i + 1}\n${formatSrtTimestamp(c.startSeconds)} --> ${formatSrtTimestamp(c.endSeconds)}\n${c.text}`)
      .join('\n\n') + (cues.length ? '\n' : '')
  );
}

interface TimedChar {
  ch: string;
  start: number;
  end: number;
}

function expandRecognized(cues: readonly RecognizedCue[]): TimedChar[] {
  const out: TimedChar[] = [];
  for (const cue of cues) {
    const chars = normalizeChars(cue.text);
    if (chars.length === 0) continue;
    const span = Math.max(0, cue.endSeconds - cue.startSeconds);
    chars.forEach((ch, k) => {
      out.push({
        ch,
        start: cue.startSeconds + (span * k) / chars.length,
        end: cue.startSeconds + (span * (k + 1)) / chars.length,
      });
    });
  }
  return out;
}

/** Returns, for each lyric char index, the matched recognised index (or -1). */
function alignChars(a: readonly string[], b: readonly TimedChar[]): Int32Array {
  const n = a.length;
  const m = b.length;
  const map = new Int32Array(n).fill(-1);
  if (n === 0 || m === 0) return map;

  // trace: 0 = diag, 1 = up (skip lyric char), 2 = left (skip recognised char)
  const trace = new Uint8Array((n + 1) * (m + 1));
  let prev = new Int32Array(m + 1);
  let cur = new Int32Array(m + 1);
  for (let j = 1; j <= m; j++) {
    prev[j] = j * GAP;
    trace[j] = 2;
  }
  for (let i = 1; i <= n; i++) {
    cur[0] = i * GAP;
    trace[i * (m + 1)] = 1;
    const ai = a[i - 1];
    for (let j = 1; j <= m; j++) {
      const diag = prev[j - 1]! + (ai === b[j - 1]!.ch ? MATCH : MISMATCH);
      const up = prev[j]! + GAP;
      const left = cur[j - 1]! + GAP;
      let best = diag;
      let t = 0;
      if (up > best) {
        best = up;
        t = 1;
      }
      if (left > best) {
        best = left;
        t = 2;
      }
      cur[j] = best;
      trace[i * (m + 1) + j] = t;
    }
    [prev, cur] = [cur, prev];
  }

  let i = n;
  let j = m;
  while (i > 0 || j > 0) {
    const t = trace[i * (m + 1) + j];
    if (i > 0 && j > 0 && t === 0) {
      map[i - 1] = j - 1;
      i--;
      j--;
    } else if (i > 0 && (t === 1 || j === 0)) {
      i--;
    } else {
      j--;
    }
  }
  return map;
}

interface SplitPart {
  text: string;
  firstNorm: number; // normalised char index (inclusive)
  lastNorm: number; // inclusive
}

/** Splits a lyric line into `parts` chunks at natural break points; each chunk knows its normalised span. */
function splitLine(text: string, parts: number): SplitPart[] {
  const cps = Array.from(text);
  const normCount = cps.map((c) => normalizeChars(c).length);
  const prefix: number[] = [0];
  for (const n of normCount) prefix.push(prefix[prefix.length - 1]! + n);
  const total = prefix[prefix.length - 1]!;
  if (parts <= 1 || total <= 1) return [{ text, firstNorm: 0, lastNorm: Math.max(0, total - 1) }];

  // candidate cut positions: index into cps (cut before cps[pos])
  const natural: number[] = [];
  for (let p = 1; p < cps.length; p++) {
    if (BREAK_AFTER.test(cps[p - 1]!) && prefix[p]! > 0 && prefix[p]! < total) natural.push(p);
  }

  const cuts: number[] = [];
  let lastCut = 0;
  for (let k = 1; k < parts; k++) {
    const ideal = (total * k) / parts;
    const window = total / parts / 2;
    let best = -1;
    let bestDist = Infinity;
    for (const p of natural) {
      if (p <= lastCut) continue;
      const d = Math.abs(prefix[p]! - ideal);
      if (d <= window && d < bestDist) {
        best = p;
        bestDist = d;
      }
    }
    if (best < 0) {
      // hard cut at the code point where the cumulative count reaches `ideal`
      let p = lastCut + 1;
      while (p < cps.length - 1 && prefix[p]! < Math.round(ideal)) p++;
      best = p;
    }
    if (best > lastCut && best < cps.length) {
      cuts.push(best);
      lastCut = best;
    }
  }

  const bounds = [0, ...cuts, cps.length];
  const out: SplitPart[] = [];
  for (let b = 0; b < bounds.length - 1; b++) {
    const from = bounds[b]!;
    const to = bounds[b + 1]!;
    const chunk = cps.slice(from, to).join('').trim();
    const first = prefix[from]!;
    const last = prefix[to]! - 1;
    if (last < first || !chunk) {
      // punctuation-only chunk: glue to the previous one
      if (out.length) out[out.length - 1]!.text += cps.slice(from, to).join('');
      continue;
    }
    out.push({ text: chunk, firstNorm: first, lastNorm: last });
  }
  return out.length ? out : [{ text, firstNorm: 0, lastNorm: Math.max(0, total - 1) }];
}

export function alignLyrics(
  cues: readonly RecognizedCue[],
  lyrics: string | readonly string[],
  options: AlignLyricsOptions = {}
): LyricsAlignmentResult {
  const minCue = options.minCueSeconds ?? 0.4;
  const maxChars = Math.max(4, options.maxCharsPerCue ?? 24);
  const maxSeconds = Math.max(1, options.maxCueSeconds ?? 8);

  const input: LyricLineInput[] =
    typeof lyrics === 'string'
      ? parseLyrics(lyrics)
      : lyrics.filter((l) => l.trim().length > 0).map((l) => ({ text: l.trim(), paragraph: 0 }));
  const total = input.length;
  const recognized = expandRecognized(cues);

  const lineChars = input.map((l) => normalizeChars(l.text));
  const flat: string[] = [];
  const lineOf: number[] = [];
  const lineFirst: number[] = []; // index of line's first char in `flat`
  lineChars.forEach((chars, li) => {
    lineFirst.push(flat.length);
    for (const c of chars) {
      flat.push(c);
      lineOf.push(li);
    }
  });

  const maxCells = options.maxCells ?? 36_000_000;
  const map =
    flat.length * recognized.length <= maxCells
      ? alignChars(flat, recognized)
      : new Int32Array(flat.length).fill(-1);

  // exact matches, then anchors = exact matches + substitutions that sit next to one
  const exactAt = new Uint8Array(flat.length);
  for (let k = 0; k < flat.length; k++) {
    const j = map[k]!;
    if (j >= 0 && recognized[j]!.ch === flat[k]) exactAt[k] = 1;
  }
  const exactPrefix = new Int32Array(flat.length + 1);
  for (let k = 0; k < flat.length; k++) exactPrefix[k + 1] = exactPrefix[k]! + exactAt[k]!;
  const nearExact = (k: number) => {
    const lo = Math.max(0, k - ANCHOR_NEIGHBOURHOOD);
    const hi = Math.min(flat.length, k + ANCHOR_NEIGHBOURHOOD + 1);
    return exactPrefix[hi]! - exactPrefix[lo]! > 0;
  };

  const anchorStart = new Float64Array(flat.length).fill(NaN);
  const anchorEnd = new Float64Array(flat.length).fill(NaN);
  for (let k = 0; k < flat.length; k++) {
    const j = map[k]!;
    if (j < 0 || !(exactAt[k] || nearExact(k))) continue;
    anchorStart[k] = recognized[j]!.start;
    anchorEnd[k] = recognized[j]!.end;
  }

  const weight = (li: number) => Math.max(1, lineChars[li]!.length);
  const s = new Array<number>(total).fill(NaN);
  const e = new Array<number>(total).fill(NaN);
  const anchored = new Array<boolean>(total).fill(false);
  const exactCount = new Array<number>(total).fill(0);
  const firstAnchorIdx = new Array<number>(total).fill(-1); // within line
  const lastAnchorIdx = new Array<number>(total).fill(-1);

  for (let li = 0; li < total; li++) {
    const base = lineFirst[li]!;
    for (let c = 0; c < lineChars[li]!.length; c++) {
      const k = base + c;
      if (exactAt[k]) exactCount[li]!++;
      if (Number.isNaN(anchorStart[k])) continue;
      if (firstAnchorIdx[li]! < 0) firstAnchorIdx[li] = c;
      lastAnchorIdx[li] = c;
      s[li] = Number.isNaN(s[li]!) ? anchorStart[k]! : Math.min(s[li]!, anchorStart[k]!);
      e[li] = Number.isNaN(e[li]!) ? anchorEnd[k]! : Math.max(e[li]!, anchorEnd[k]!);
      anchored[li] = true;
    }
  }

  // singing rate from well-covered anchored lines
  const rates: number[] = [];
  for (let li = 0; li < total; li++) {
    if (!anchored[li]) continue;
    const covered = lastAnchorIdx[li]! - firstAnchorIdx[li]! + 1;
    if (covered >= 3 && e[li]! > s[li]!) rates.push((e[li]! - s[li]!) / covered);
  }
  rates.sort((x, y) => x - y);
  const secPerChar = rates.length ? rates[Math.floor(rates.length / 2)]! : (options.fallbackSecondsPerChar ?? 0.25);

  // widen partially-covered anchored lines to their full length at the singing rate
  const anchoredOnlyS = [...s];
  const anchoredOnlyE = [...e];
  for (let li = 0; li < total; li++) {
    if (!anchored[li]) continue;
    const len = lineChars[li]!.length;
    const lead = firstAnchorIdx[li]!;
    const trail = len - 1 - lastAnchorIdx[li]!;
    s[li] = s[li]! - lead * secPerChar;
    e[li] = e[li]! + trail * secPerChar;
  }
  // widening must not run into the evidence of the neighbouring anchored lines
  let prevEvidenceEnd = 0;
  for (let li = 0; li < total; li++) {
    if (!anchored[li]) continue;
    s[li] = Math.min(anchoredOnlyS[li]!, Math.max(s[li]!, prevEvidenceEnd));
    prevEvidenceEnd = anchoredOnlyE[li]!;
  }
  let nextEvidenceStart = Infinity;
  for (let li = total - 1; li >= 0; li--) {
    if (!anchored[li]) continue;
    e[li] = Math.max(anchoredOnlyE[li]!, Math.min(e[li]!, nextEvidenceStart));
    nextEvidenceStart = anchoredOnlyS[li]!;
  }

  // place runs of unanchored lines inside their gap; spare time goes to stanza breaks (interludes)
  const paragraph = (li: number) => input[li]!.paragraph;
  let i = 0;
  while (i < total) {
    if (anchored[i]) {
      i++;
      continue;
    }
    let j = i;
    while (j < total && !anchored[j]) j++;
    const n = j - i;
    const runWeight = Array.from({ length: n }, (_, k) => weight(i + k)).reduce((x, y) => x + y, 0);
    const need = runWeight * secPerChar;
    const before = i > 0 ? e[i - 1]! : null;
    const after = j < total ? s[j]! : null;

    const place = (from: number, spares: number[], scale: number) => {
      let t = from + spares[0]!;
      for (let k = 0; k < n; k++) {
        const dur = weight(i + k) * secPerChar * scale;
        s[i + k] = t;
        e[i + k] = t + dur;
        t += dur + spares[k + 1]!;
      }
    };

    if (before !== null && after !== null) {
      const gap = after - before;
      if (gap >= need) {
        // boundary weights: stanza break → likely interlude gets the spare time
        const w: number[] = [];
        w.push(paragraph(i) !== paragraph(i - 1) ? 4 : 0.5);
        for (let k = 1; k < n; k++) w.push(paragraph(i + k) !== paragraph(i + k - 1) ? 4 : 0);
        w.push(paragraph(j) !== paragraph(j - 1) ? 4 : 0.5);
        const sum = w.reduce((x, y) => x + y, 0);
        const spare = gap - need;
        place(before, w.map((x) => (spare * x) / sum), 1);
      } else {
        place(before, new Array(n + 1).fill(0), Math.max(gap, minCue * n) / need);
      }
    } else if (after !== null) {
      const from = Math.max(0, after - need);
      const avail = after - from;
      place(from, new Array(n + 1).fill(0), avail > 0 ? avail / need : 1);
    } else if (before !== null) {
      place(before, new Array(n + 1).fill(0), 1);
    } else {
      const recEnd = recognized.length ? recognized[recognized.length - 1]!.end : 0;
      const spanTotal = recEnd > 0 ? recEnd : need;
      place(0, new Array(n + 1).fill(0), spanTotal / need);
    }
    i = j;
  }

  // ordering / minimum duration
  for (let k = 0; k < total; k++) {
    if (k > 0 && s[k]! < e[k - 1]!) {
      const mid = (s[k]! + e[k - 1]!) / 2;
      e[k - 1] = Math.max(s[k - 1]! + 0.1, Math.min(e[k - 1]!, mid));
      s[k] = Math.max(s[k]!, e[k - 1]!);
    }
    if (e[k]! < s[k]! + minCue) e[k] = s[k]! + minCue;
  }

  const lines: AlignedLyricLine[] = input.map((l, k) => ({
    index: k,
    text: l.text,
    startSeconds: s[k]!,
    endSeconds: e[k]!,
    confidence: lineChars[k]!.length ? exactCount[k]! / lineChars[k]!.length : 0,
    anchored: anchored[k]!,
  }));

  // per-character times → cues (split long lines)
  const out: LyricCue[] = [];
  for (let li = 0; li < total; li++) {
    const len = lineChars[li]!.length;
    const ls = s[li]!;
    const le = e[li]!;
    const charStart = new Array<number>(len);
    // known points (index → start time) plus sentinels at both ends
    const known: Array<[number, number]> = [[-1, ls]];
    for (let c = 0; c < len; c++) {
      const a = anchorStart[lineFirst[li]! + c]!;
      if (!Number.isNaN(a) && anchored[li]) known.push([c, Math.min(Math.max(a, ls), le)]);
    }
    known.push([len, le]);
    let ki = 0;
    for (let c = 0; c < len; c++) {
      while (known[ki + 1]![0] <= c && ki + 1 < known.length - 1) ki++;
      const [i0, t0] = known[ki]!;
      const [i1, t1] = known[Math.min(ki + 1, known.length - 1)]!;
      charStart[c] = i1 === i0 ? t0 : t0 + ((t1 - t0) * (c - i0)) / (i1 - i0);
      if (known[ki]![0] === c) charStart[c] = known[ki]![1];
      if (c > 0 && charStart[c]! < charStart[c - 1]!) charStart[c] = charStart[c - 1]!;
    }

    const duration = le - ls;
    const parts = Math.max(Math.ceil(len / maxChars), Math.ceil(duration / maxSeconds), 1);
    const pieces = splitLine(input[li]!.text, Math.min(parts, Math.max(1, len)));

    pieces.forEach((p, pi) => {
      const pStart = len ? (charStart[p.firstNorm] ?? ls) : ls;
      let pEnd: number;
      if (pi === pieces.length - 1) {
        pEnd = le;
      } else {
        const nextStart = charStart[pieces[pi + 1]!.firstNorm] ?? le;
        pEnd = nextStart;
      }
      pEnd = Math.max(pEnd, pStart + Math.min(minCue, Math.max(0.05, le - ls)));
      out.push({ text: p.text, startSeconds: pStart, endSeconds: pEnd, lineIndex: li });
    });
  }

  const avg = lines.length ? lines.reduce((acc, l) => acc + l.confidence, 0) / lines.length : 0;
  return {
    lines,
    cues: out,
    srt: cuesToSrt(out),
    averageConfidence: avg,
    recognizedChars: recognized.length,
  };
}
