export interface RationalData {
  readonly num: number;
  readonly den: number;
}

export enum RoundMode {
  Floor = 'floor',
  Nearest = 'nearest',
  Ceil = 'ceil',
}

function gcd(a: number, b: number): number {
  let x = Math.abs(a);
  let y = Math.abs(b);
  while (y !== 0) {
    const t = x % y;
    x = y;
    y = t;
  }
  return x;
}

function gcdBigInt(a: bigint, b: bigint): bigint {
  let x = a < 0n ? -a : a;
  let y = b < 0n ? -b : b;
  while (y !== 0n) {
    const t = x % y;
    x = y;
    y = t;
  }
  return x;
}

export class Rational implements RationalData {
  readonly num: number;
  readonly den: number;

  constructor(num: number = 0, den: number = 1) {
    if (den === 0) {
      den = 1;
    }
    if (den < 0) {
      num = -num;
      den = -den;
    }
    this.num = Math.trunc(num);
    this.den = Math.trunc(den);
  }

  static reduced(num: number, den: number): Rational {
    if (den === 0) return new Rational(num, 1);
    if (den < 0) {
      num = -num;
      den = -den;
    }
    const g = gcd(num, den);
    return g > 0 ? new Rational(num / g, den / g) : new Rational(num, den);
  }

  toDouble(): number {
    return this.den !== 0 ? this.num / this.den : 0;
  }

  isZero(): boolean {
    return this.num === 0;
  }

  inverted(): Rational {
    return new Rational(this.den, this.num);
  }

  reduce(): Rational {
    return Rational.reduced(this.num, this.den);
  }

  mul(o: RationalData): Rational {
    const n = BigInt(this.num) * BigInt(o.num);
    const d = BigInt(this.den) * BigInt(o.den);
    const g = gcdBigInt(n, d);
    const rn = g > 0n ? n / g : n;
    const rd = g > 0n ? d / g : d;
    return new Rational(Number(rn), Number(rd));
  }

  div(o: RationalData): Rational {
    return this.mul(new Rational(o.den, o.num));
  }

  add(o: RationalData): Rational {
    const n = BigInt(this.num) * BigInt(o.den) + BigInt(o.num) * BigInt(this.den);
    const d = BigInt(this.den) * BigInt(o.den);
    const g = gcdBigInt(n, d);
    const rn = g > 0n ? n / g : n;
    const rd = g > 0n ? d / g : d;
    return new Rational(Number(rn), Number(rd));
  }

  sub(o: RationalData): Rational {
    const n = BigInt(this.num) * BigInt(o.den) - BigInt(o.num) * BigInt(this.den);
    const d = BigInt(this.den) * BigInt(o.den);
    const g = gcdBigInt(n, d);
    const rn = g > 0n ? n / g : n;
    const rd = g > 0n ? d / g : d;
    return new Rational(Number(rn), Number(rd));
  }

  equals(o: RationalData): boolean {
    const a = this.reduce();
    const b = Rational.reduced(o.num, o.den);
    return a.num === b.num && a.den === b.den;
  }

  compare(o: RationalData): number {
    const lhs = BigInt(this.num) * BigInt(o.den);
    const rhs = BigInt(o.num) * BigInt(this.den);
    if (lhs < rhs) return -1;
    if (lhs > rhs) return 1;
    return 0;
  }

  lessThan(o: RationalData): boolean {
    return this.compare(o) < 0;
  }

  greaterThan(o: RationalData): boolean {
    return this.compare(o) > 0;
  }

  lessOrEqual(o: RationalData): boolean {
    return this.compare(o) <= 0;
  }

  greaterOrEqual(o: RationalData): boolean {
    return this.compare(o) >= 0;
  }
}

export const Timebase = {
  Fps23_976: new Rational(1001, 24000),
  Fps24: new Rational(1, 24),
  Fps25: new Rational(1, 25),
  Fps29_97: new Rational(1001, 30000),
  Fps30: new Rational(1, 30),
  Fps59_94: new Rational(1001, 60000),
  Fps60: new Rational(1, 60),
} as const;

export const Timebases = Timebase;

export function secondsToFrames(seconds: number, tb: RationalData, mode: RoundMode = RoundMode.Nearest): number {
  if (tb.den === 0 || tb.num === 0) return 0;
  // frames = seconds * (tb.den / tb.num)
  const raw = (seconds * tb.den) / tb.num;
  switch (mode) {
    case RoundMode.Floor:
      return Math.floor(raw);
    case RoundMode.Nearest:
      return raw >= 0 ? Math.floor(raw + 0.5) : Math.ceil(raw - 0.5);
    case RoundMode.Ceil:
      return Math.ceil(raw);
  }
}

export function framesToSeconds(frames: number, tb: RationalData): number {
  if (tb.den === 0 || tb.num === 0) return 0;
  return (frames * tb.num) / tb.den;
}

export function rescaleFrames(frames: number, from: RationalData, to: RationalData, mode: RoundMode = RoundMode.Nearest): number {
  if (from.den === 0 || to.num === 0) return 0;
  const n = BigInt(Math.trunc(frames)) * BigInt(from.num) * BigInt(to.den);
  const d = BigInt(from.den) * BigInt(to.num);
  if (d === 0n) return 0;

  let q = n / d;
  const rem = n % d;

  switch (mode) {
    case RoundMode.Floor:
      if (rem !== 0n && n < 0n) {
        q -= 1n;
      }
      break;
    case RoundMode.Nearest: {
      const twiceRem = (rem < 0n ? -rem : rem) * 2n;
      const absD = d < 0n ? -d : d;
      if (twiceRem >= absD) {
        q += n < 0n ? -1n : 1n;
      }
      break;
    }
    case RoundMode.Ceil:
      if (rem !== 0n && ((d > 0n && rem > 0n) || (d < 0n && rem < 0n))) {
        q += 1n;
      }
      break;
  }

  return Number(q);
}
