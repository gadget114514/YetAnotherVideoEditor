import { createUuid, type Uuid } from '../id/Uuid.js';
import type { Rational } from '../time/Rational.js';
import { Timebase } from '../time/Rational.js';

export type AssetKind = 'video' | 'audio' | 'image' | 'generated';

export interface Asset {
  id: Uuid;
  kind: AssetKind;
  relativePath: string;
  resolvedAbsolutePath: string;
  hash: string;
  isMissing: boolean;
  durationFrames: number;
  frameRate: Rational;
  resolution: { width: number; height: number };
  hasAudio: boolean;
  generatedByTaskId?: Uuid;
}

export class AssetLibrary {
  private assets_: Map<Uuid, Asset> = new Map();
  private refCounts_: Map<Uuid, number> = new Map();
  private proxies_: Map<Uuid, string> = new Map();

  registerAsset(absolutePath: string, kind: AssetKind = 'video'): Asset {
    // Check if path already exists
    for (const a of this.assets_.values()) {
      if (
        a.resolvedAbsolutePath === absolutePath ||
        a.relativePath === absolutePath
      ) {
        return a;
      }
    }

    const id = createUuid();
    const asset: Asset = {
      id,
      kind,
      relativePath: absolutePath,
      resolvedAbsolutePath: absolutePath,
      hash: '',
      isMissing: false,
      durationFrames: 0,
      frameRate: Timebase.Fps59_94,
      resolution: { width: 1920, height: 1080 },
      hasAudio: kind === 'audio' || kind === 'video',
    };

    this.assets_.set(id, asset);
    return asset;
  }

  addResolvedAsset(asset: Asset): Asset {
    this.assets_.set(asset.id, { ...asset });
    return this.assets_.get(asset.id)!;
  }

  removeAsset(id: Uuid): void {
    this.assets_.delete(id);
    this.refCounts_.delete(id);
    this.proxies_.delete(id);
  }

  asset(id: Uuid): Asset | null {
    return this.assets_.get(id) ?? null;
  }

  findByPath(normalizedRelativePath: string): Asset | null {
    for (const a of this.assets_.values()) {
      if (
        a.relativePath === normalizedRelativePath ||
        a.resolvedAbsolutePath === normalizedRelativePath
      ) {
        return a;
      }
    }
    return null;
  }

  allIds(): Uuid[] {
    return Array.from(this.assets_.keys());
  }

  count(): number {
    return this.assets_.size;
  }

  refCount(id: Uuid): number {
    return this.refCounts_.get(id) ?? 0;
  }

  retain(id: Uuid): void {
    this.refCounts_.set(id, this.refCount(id) + 1);
  }

  release(id: Uuid): void {
    const cur = this.refCount(id) - 1;
    if (cur <= 0) {
      this.refCounts_.delete(id);
    } else {
      this.refCounts_.set(id, cur);
    }
  }

  proxyPath(id: Uuid): string {
    return this.proxies_.get(id) ?? '';
  }

  setProxyPath(id: Uuid, path: string): void {
    this.proxies_.set(id, path);
  }
}
