export interface VideoFilterInstance {
  filterId: string;
  params: Record<string, any>;
  enabled?: boolean;
}

export const builtinFilter = {
  kColorAdjust: 'yave.filter.colorAdjust',
  kBlur: 'yave.filter.blur',
  kMono: 'yave.filter.mono',
  kSepia: 'yave.filter.sepia',
} as const;

export interface VideoFilterDesc {
  filterId: string;
  displayNameKey: string;
  defaultParams: Record<string, any>;
}

export const builtinVideoFilters: readonly VideoFilterDesc[] = [
  {
    filterId: builtinFilter.kColorAdjust,
    displayNameKey: 'filter.colorAdjust.name',
    defaultParams: { brightness: 0.0, contrast: 1.0, saturation: 1.0, gamma: 1.0 },
  },
  {
    filterId: builtinFilter.kBlur,
    displayNameKey: 'filter.blur.name',
    defaultParams: { radius: 8.0, direction: 0 },
  },
  {
    filterId: builtinFilter.kMono,
    displayNameKey: 'filter.mono.name',
    defaultParams: { amount: 1.0 },
  },
  {
    filterId: builtinFilter.kSepia,
    displayNameKey: 'filter.sepia.name',
    defaultParams: { amount: 1.0 },
  },
];

export function findVideoFilterDesc(filterId: string): VideoFilterDesc | null {
  return builtinVideoFilters.find((d) => d.filterId === filterId) ?? null;
}

export function resolveFilterParams(inst: VideoFilterInstance): number[] {
  const out = new Array<number>(8).fill(0.0);
  const p = inst.params || {};

  if (inst.filterId === builtinFilter.kColorAdjust) {
    out[0] = typeof p['brightness'] === 'number' ? p['brightness'] : 0.0;
    out[1] = typeof p['contrast'] === 'number' ? p['contrast'] : 1.0;
    out[2] = typeof p['saturation'] === 'number' ? p['saturation'] : 1.0;
    out[3] = typeof p['gamma'] === 'number' ? p['gamma'] : 1.0;
  } else if (inst.filterId === builtinFilter.kBlur) {
    out[0] = typeof p['radius'] === 'number' ? p['radius'] : 8.0;
    out[1] = typeof p['direction'] === 'number' ? p['direction'] : 0.0;
  } else if (inst.filterId === builtinFilter.kMono || inst.filterId === builtinFilter.kSepia) {
    out[0] = typeof p['amount'] === 'number' ? p['amount'] : 1.0;
  }
  return out;
}
