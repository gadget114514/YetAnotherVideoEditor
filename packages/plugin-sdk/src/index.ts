// @yave/plugin-sdk

export interface PluginManifest {
  readonly id: string;
  readonly name: { readonly ja: string; readonly en: string };
  readonly version: string;
  readonly sdkVersion: string;
  readonly kind: 'subtitleEffect' | 'videoFilter' | 'transition' | 'audioEffect';
  readonly entry: string;
}

export interface ParameterSchemaItem {
  readonly key: string;
  readonly displayNameKey: string;
  readonly type: 'bool' | 'int' | 'double' | 'color' | 'string' | 'enum';
  readonly defaultValue: any;
  readonly minValue?: number;
  readonly maxValue?: number;
  readonly step?: number;
  readonly enumChoices?: readonly string[];
  readonly unitSuffix?: string;
}

export interface GlyphInfo {
  readonly charIndex: number;
  readonly char: string;
  readonly lineIndex: number;
  readonly isWhitespace: boolean;
  readonly layoutRect: { x: number; y: number; width: number; height: number };
  readonly baseColor: string;
}

export interface SubtitleGlyphRun {
  readonly lineCount: number;
  readonly blockSize: { width: number; height: number };
  readonly glyphs: readonly GlyphInfo[];
}

export interface GlyphTransform {
  transform: number[]; // 4x4 matrix or [posX, posY, scaleX, scaleY, rot]
  color: string;
  opacity: number;
  visible: boolean;
  blurRadius: number;
}

export interface SubtitleEffectFrame {
  readonly run: SubtitleGlyphRun;
  readonly canvasSize: { width: number; height: number };
  readonly clipStartFrame: number;
  readonly clipDuration: number;
  readonly currentFrame: number;
  readonly fps: number;
  glyphs: GlyphTransform[];
  blockTransform: number[];
  blockOpacity: number;
}

export interface SubtitleTimeInfo {
  readonly progress: number; // 0.0 .. 1.0
  readonly secondsFromIn: number;
  readonly secondsToOut: number;
  readonly clipDurationSec: number;
}

export interface ISubtitleEffect {
  readonly id: string;
  readonly displayName: string;
  readonly category: string;
  getParameterSchema(): readonly ParameterSchemaItem[];
  prepare?(run: SubtitleGlyphRun, params: Record<string, any>): void;
  apply(frame: SubtitleEffectFrame, time: SubtitleTimeInfo, params: Record<string, any>): void;
}

export interface IVideoFilter {
  readonly id: string;
  readonly displayName: string;
  getParameterSchema(): readonly ParameterSchemaItem[];
  applyGlslFragment?(): string;
  applyCanvas?(ctx: CanvasRenderingContext2D, width: number, height: number, params: Record<string, any>): void;
}
