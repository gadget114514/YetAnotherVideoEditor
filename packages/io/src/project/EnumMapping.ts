import { BlendMode } from '@yave/core';

export function blendModeToString(mode: BlendMode): string {
  return String(mode);
}

export function stringToBlendMode(
  str: string,
  fallback: BlendMode = BlendMode.Normal,
): BlendMode {
  const vals = Object.values(BlendMode) as string[];
  if (vals.includes(str)) {
    return str as BlendMode;
  }
  return fallback;
}
