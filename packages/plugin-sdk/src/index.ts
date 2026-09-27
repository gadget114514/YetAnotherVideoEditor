// @yave/plugin-sdk
export interface PluginManifest {
  readonly id: string;
  readonly name: { readonly ja: string; readonly en: string };
  readonly version: string;
  readonly sdkVersion: string;
  readonly kind: 'subtitleEffect' | 'videoFilter' | 'transition' | 'audioEffect';
  readonly entry: string;
}
