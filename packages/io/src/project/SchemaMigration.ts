import { kTracks, kClips, kLibrary, kFolders, kAssignments, kTransitions, kFilters, kEffectChain } from './JsonKeys.js';
import type { LoadResult } from './ProjectSerializer.js';

export function migrate_1_to_2(root: any, _result?: LoadResult): void {
  if (!root.storyBible) {
    root.storyBible = {};
  }

  const tracks = root[kTracks];
  if (Array.isArray(tracks)) {
    for (const t of tracks) {
      if (t && typeof t === 'object') {
        if (!('aiRole' in t)) t.aiRole = '';
        if (!('storyboardTrackId' in t)) t.storyboardTrackId = null;
        if (!('roleDefaults' in t)) t.roleDefaults = {};
      }
    }
  }
}

export function migrate_2_to_3(root: any, _result?: LoadResult): void {
  if (!root[kLibrary]) {
    root[kLibrary] = {
      [kFolders]: [],
      [kAssignments]: {},
    };
  }

  const tracks = root[kTracks];
  if (Array.isArray(tracks)) {
    for (const t of tracks) {
      if (t && typeof t === 'object') {
        if (!(kTransitions in t)) {
          t[kTransitions] = [];
        }

        const clips = t[kClips];
        if (Array.isArray(clips)) {
          for (const c of clips) {
            if (c && typeof c === 'object') {
              if (kEffectChain in c && !(kFilters in c)) {
                c[kFilters] = c[kEffectChain];
                delete c[kEffectChain];
              }
              if (!(kFilters in c)) {
                c[kFilters] = [];
              }
            }
          }
        }
      }
    }
  }
}

export function migrate_3_to_4(root: any, _result?: LoadResult): void {
  // Schema version 4: Web architecture adjustments
  if (!root.webSettings) {
    root.webSettings = {
      decoderPreference: 'hardware',
      renderBackend: 'webgl2',
    };
  }
}

export interface Migration {
  fromVersion: number;
  fn: (root: any, result?: LoadResult) => void;
}

export const migrations: Migration[] = [
  { fromVersion: 1, fn: migrate_1_to_2 },
  { fromVersion: 2, fn: migrate_2_to_3 },
  { fromVersion: 3, fn: migrate_3_to_4 },
];

export function applyMigrations(
  root: any,
  fromVersion: number,
  targetVersion: number,
  result?: LoadResult,
): void {
  let v = fromVersion;
  while (v < targetVersion) {
    const m = migrations.find((x) => x.fromVersion === v);
    if (!m) {
      if (result) {
        result.warnings.push(`No migration path from schema version ${v}.`);
      }
      break;
    }
    m.fn(root, result);
    v++;
    if (result) {
      result.migrated = true;
    }
  }
  root.schemaVersion = targetVersion;
}
