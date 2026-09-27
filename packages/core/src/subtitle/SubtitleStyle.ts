export enum SubtitleStyleField {
  FontFamily = 'FontFamily',
  FontPointSize = 'FontPointSize',
  FontWeight = 'FontWeight',
  Italic = 'Italic',
  FillColor = 'FillColor',
  OutlineColor = 'OutlineColor',
  OutlineWidth = 'OutlineWidth',
  ShadowColor = 'ShadowColor',
  ShadowBlur = 'ShadowBlur',
  BoxEnabled = 'BoxEnabled',
  BoxColor = 'BoxColor',
  HAlign = 'HAlign',
  VAlign = 'VAlign',
  Anchor = 'Anchor',
  LineSpacing = 'LineSpacing',
  LetterSpacing = 'LetterSpacing',
  MaxWidthRatio = 'MaxWidthRatio',
  RotationDeg = 'RotationDeg',
  Scale = 'Scale',
  Opacity = 'Opacity',
  Vertical = 'Vertical',
}

export interface SubtitleStyleDiff {
  fontFamily?: string;
  fontPointSize?: number;
  fontWeight?: number;
  italic?: boolean;
  fillColor?: string;
  outlineColor?: string;
  outlineWidth?: number;
  shadowColor?: string;
  shadowBlur?: number;
  boxEnabled?: boolean;
  boxColor?: string;
  hAlign?: number;
  vAlign?: number;
  anchor?: { x: number; y: number };
  lineSpacing?: number;
  letterSpacing?: number;
  maxWidthRatio?: number;
  rotationDeg?: number;
  scale?: { x: number; y: number };
  opacity?: number;
  vertical?: boolean;
}

export interface SubtitleStyle extends Required<SubtitleStyleDiff> {
  id: string;
  name: string;
}
