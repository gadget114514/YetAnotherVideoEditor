// @yave/core
export type Uuid = string & { readonly __brand: unique symbol };

export function createUuid(): Uuid {
  return crypto.randomUUID() as Uuid;
}

export interface Rational {
  readonly num: number;
  readonly den: number;
}

export const Timebases = {
  Fps24: { num: 24, den: 1 },
  Fps23_976: { num: 24000, den: 1001 },
  Fps30: { num: 30, den: 1 },
  Fps29_97: { num: 30000, den: 1001 },
  Fps60: { num: 60, den: 1 },
  Fps59_94: { num: 60000, den: 1001 },
} as const;

export interface TimeRange {
  readonly start: number;
  readonly duration: number;
}
