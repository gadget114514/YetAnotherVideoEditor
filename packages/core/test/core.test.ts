import { describe, it, expect } from 'vitest';
import { createUuid, Timebases } from '../src/index.js';

describe('core basic smoke tests', () => {
  it('generates valid UUIDs', () => {
    const id = createUuid();
    expect(id).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i);
  });

  it('defines standard timebases', () => {
    expect(Timebases.Fps60.num).toBe(1);
    expect(Timebases.Fps60.den).toBe(60);
    expect(Timebases.Fps59_94.num).toBe(1001);
    expect(Timebases.Fps59_94.den).toBe(60000);
  });
});
