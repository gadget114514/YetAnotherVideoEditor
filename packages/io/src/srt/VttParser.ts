import { SrtParser, type SrtParseResult } from './SrtParser.js';

export class VttParser {
  static parseText(text: string): SrtParseResult {
    let stripped = text;
    if (stripped.toLowerCase().startsWith('webvtt')) {
      const eol = stripped.indexOf('\n');
      stripped = eol < 0 ? '' : stripped.substring(eol + 1);
    }
    return SrtParser.parseText(stripped);
  }
}
