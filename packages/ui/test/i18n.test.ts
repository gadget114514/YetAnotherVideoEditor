import { describe, it, expect } from 'vitest';
import { LanguageManager } from '../src/i18n/LanguageManager.js';
import { formatTimecode, parseTimecode, Timebase } from '@yave/core';

describe('TestI18n', () => {
  it('switchLanguageAtRuntime', () => {
    const lm = LanguageManager.instance();
    lm.initialize();

    lm.setLanguage('en');
    expect(lm.translate('QObject', 'Export')).toBe('Export');

    let changedCount = 0;
    const unsub = lm.onLanguageChanged(() => {
      changedCount++;
    });

    lm.setLanguage('ja');
    expect(changedCount).toBe(1);
    expect(lm.currentLanguage()).toBe('ja');

    expect(lm.translate('QObject', 'Export')).toBe('書き出し');

    // Switch back
    lm.setLanguage('en');
    expect(lm.translate('QObject', 'Export')).toBe('Export');

    unsub();
  });

  it('timecodeIsLocaleIndependent', () => {
    // formatTimecode
    expect(formatTimecode(3600, Timebase.Fps60, false)).toBe('00:01:00:00');
    expect(formatTimecode(1800, Timebase.Fps30, true)).toBe('00:01:00;02');

    // parseTimecode
    const parsed1 = parseTimecode('00:01:00:00', Timebase.Fps60);
    expect(parsed1).not.toBeNull();
    expect(parsed1).toBe(3600);

    const parsed2 = parseTimecode('00:01:00;02', Timebase.Fps30);
    expect(parsed2).not.toBeNull();
    expect(parsed2).toBe(1800);
  });
});
