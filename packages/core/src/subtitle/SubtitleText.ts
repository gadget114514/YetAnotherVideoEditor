export interface TextSpan {
  start: number;
  length: number;
  bold?: boolean;
  italic?: boolean;
  underline?: boolean;
  color?: string; // hex color e.g. #ffcc00
  fontFamily?: string;
  sizeScale?: number;
  ruby?: string;
}

export class SubtitleText {
  private plain_: string = '';
  private spans_: TextSpan[] = [];

  constructor(plain: string = '') {
    this.plain_ = plain;
  }

  get plain(): string {
    return this.plain_;
  }

  setPlain(s: string): void {
    this.plain_ = s;
    this.normalizeSpans();
  }

  get spans(): readonly TextSpan[] {
    return this.spans_;
  }

  addSpan(s: TextSpan): void {
    if (s.length <= 0) return;
    this.spans_.push({ ...s });
  }

  clearSpans(): void {
    this.spans_ = [];
  }

  lineCount(): number {
    if (this.plain_.length === 0) return 0;
    let n = 1;
    for (let i = 0; i < this.plain_.length; i++) {
      if (this.plain_[i] === '\n') n++;
    }
    return n;
  }

  normalizeSpans(): void {
    const len = this.plain_.length;
    this.spans_ = this.spans_.filter((s) => s.start < len && s.start + s.length <= len);
  }

  static fromSrtMarkup(markup: string): SubtitleText {
    const out = new SubtitleText();
    let s = markup
      .replace(/<br\s*\/?>/gi, '\n');

    interface OpenTag {
      kind: 'b' | 'i' | 'u' | 'font';
      start: number;
      color?: string;
      fontFamily?: string;
    }
    const stack: OpenTag[] = [];

    const tagRe = /<\/?\s*(b|i|u|font)(\s+[^>]*)?>/gi;
    const colorAttrRe = /color\s*=\s*["']?(#[0-9a-fA-F]{3,8}|[a-zA-Z]+)["']?/i;
    const faceAttrRe = /face\s*=\s*["']([^"']*)["']?/i;

    let pos = 0;
    let match: RegExpExecArray | null;

    while ((match = tagRe.exec(s)) !== null) {
      const matchStart = match.index;
      if (matchStart > pos) {
        out.plain_ += s.substring(pos, matchStart);
      }

      const fullTag = match[0];
      const isClose = fullTag.startsWith('</');
      const tagName = match[1]!.toLowerCase() as 'b' | 'i' | 'u' | 'font';
      const attrs = match[2] || '';

      if (!isClose) {
        const tag: OpenTag = {
          kind: tagName,
          start: out.plain_.length,
        };
        if (tagName === 'font') {
          const cm = colorAttrRe.exec(attrs);
          if (cm) tag.color = cm[1];
          const fm = faceAttrRe.exec(attrs);
          if (fm) tag.fontFamily = fm[1];
        }
        stack.push(tag);
      } else {
        // Find matching tag in reverse
        for (let i = stack.length - 1; i >= 0; i--) {
          if (stack[i]!.kind === tagName) {
            const open = stack[i]!;
            const spanLen = out.plain_.length - open.start;
            if (spanLen > 0) {
              const sp: TextSpan = {
                start: open.start,
                length: spanLen,
              };
              if (open.kind === 'b') sp.bold = true;
              else if (open.kind === 'i') sp.italic = true;
              else if (open.kind === 'u') sp.underline = true;
              else if (open.kind === 'font') {
                if (open.color) sp.color = open.color;
                if (open.fontFamily) sp.fontFamily = open.fontFamily;
              }
              out.spans_.push(sp);
            }
            stack.splice(i);
            break;
          }
        }
      }

      pos = tagRe.lastIndex;
    }

    if (pos < s.length) {
      out.plain_ += s.substring(pos);
    }

    return out;
  }

  toSrtMarkup(): string {
    const n = this.plain_.length;
    if (n === 0) return '';

    const bold = new Array<boolean>(n).fill(false);
    const italic = new Array<boolean>(n).fill(false);
    const underline = new Array<boolean>(n).fill(false);
    const colors = new Array<string | undefined>(n);
    const fonts = new Array<string | undefined>(n);

    for (const s of this.spans_) {
      const end = Math.min(n, s.start + s.length);
      for (let i = s.start; i < end; i++) {
        if (s.bold) bold[i] = true;
        if (s.italic) italic[i] = true;
        if (s.underline) underline[i] = true;
        if (s.color) colors[i] = s.color;
        if (s.fontFamily) fonts[i] = s.fontFamily;
      }
    }

    const openTags = (i: number): string => {
      let t = '';
      const fontNow = fonts[i];
      const fontPrev = i > 0 ? fonts[i - 1] : undefined;
      const colorNow = colors[i];
      const colorPrev = i > 0 ? colors[i - 1] : undefined;

      if (fontNow && fontNow !== fontPrev) {
        t += `<font face="${fontNow}"${colorNow ? ` color="${colorNow}"` : ''}>`;
      } else if (colorNow && colorNow !== colorPrev) {
        t += `<font color="${colorNow}">`;
      }

      if (bold[i] && (i === 0 || !bold[i - 1])) t += '<b>';
      if (italic[i] && (i === 0 || !italic[i - 1])) t += '<i>';
      if (underline[i] && (i === 0 || !underline[i - 1])) t += '<u>';
      return t;
    };

    const closeTags = (i: number): string => {
      let t = '';
      if (underline[i] && (i + 1 >= n || !underline[i + 1])) t += '</u>';
      if (italic[i] && (i + 1 >= n || !italic[i + 1])) t += '</i>';
      if (bold[i] && (i + 1 >= n || !bold[i + 1])) t += '</b>';

      const fontNow = fonts[i];
      const fontNext = i + 1 < n ? fonts[i + 1] : undefined;
      const colorNow = colors[i];
      const colorNext = i + 1 < n ? colors[i + 1] : undefined;

      if (fontNow && fontNow !== fontNext) {
        t += '</font>';
      } else if (colorNow && colorNow !== colorNext) {
        t += '</font>';
      }
      return t;
    };

    let out = '';
    for (let i = 0; i < n; i++) {
      out += openTags(i);
      out += this.plain_[i];
      out += closeTags(i);
    }
    return out;
  }
}
