import { type SubtitleClip, framesToSeconds } from '@yave/core';
import {
  type SubtitleGlyphRun,
  type GlyphInfo,
  type GlyphTransform,
  type SubtitleEffectFrame,
  type SubtitleTimeInfo,
} from '@yave/plugin-sdk';
import { builtinSubtitleEffects } from './SubtitleEffects.js';

export interface RenderSubtitleOptions {
  canvasWidth: number;
  canvasHeight: number;
  currentFrame: number;
  fps?: number;
}

export class SubtitleRenderer {
  /**
   * Renders a subtitle clip onto a 2D canvas context with styles and animated effects applied.
   */
  static renderToContext(
    ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
    clip: SubtitleClip,
    options: RenderSubtitleOptions
  ): void {
    const text = clip.text.plain;
    if (!text || text.trim().length === 0) return;

    const style = clip.styleOverride;
    const fontSize = style.fontPointSize ?? 48;
    const fontFamily = style.fontFamily ?? 'Noto Sans JP, sans-serif';
    const fontWeight = style.fontWeight ?? 600;
    const fontItalic = style.italic ? 'italic ' : '';

    ctx.save();
    ctx.font = `${fontItalic}${fontWeight} ${fontSize}px ${fontFamily}`;

    // Layout glyphs
    const glyphs: GlyphInfo[] = [];
    const chars = Array.from(text);
    let totalWidth = 0;

    for (let i = 0; i < chars.length; i++) {
      const ch = chars[i]!;
      const metrics = ctx.measureText(ch);
      const w = metrics.width;
      glyphs.push({
        charIndex: i,
        char: ch,
        lineIndex: 0,
        isWhitespace: /\s/.test(ch),
        layoutRect: { x: totalWidth, y: 0, width: w, height: fontSize },
        baseColor: style.fillColor ?? '#ffffff',
      });
      totalWidth += w;
    }

    const run: SubtitleGlyphRun = {
      lineCount: 1,
      blockSize: { width: totalWidth, height: fontSize },
      glyphs,
    };

    // Calculate time metrics
    const clipStart = clip.range.start;
    const clipDur = clip.range.duration;
    const localFrame = Math.max(0, options.currentFrame - clipStart);
    const progress = clipDur > 0 ? Math.min(1.0, localFrame / clipDur) : 1.0;

    const fps = options.fps ?? 60;
    const secondsFromIn = localFrame / fps;
    const secondsToOut = Math.max(0, (clipDur - localFrame) / fps);
    const clipDurationSec = clipDur / fps;

    const timeInfo: SubtitleTimeInfo = {
      progress,
      secondsFromIn,
      secondsToOut,
      clipDurationSec,
    };

    // Setup animated effect frame
    const glyphTransforms: GlyphTransform[] = glyphs.map((g) => ({
      transform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
      color: g.baseColor,
      opacity: 1.0,
      visible: true,
      blurRadius: 0,
    }));

    const effectFrame: SubtitleEffectFrame = {
      run,
      canvasSize: { width: options.canvasWidth, height: options.canvasHeight },
      clipStartFrame: clipStart,
      clipDuration: clipDur,
      currentFrame: options.currentFrame,
      fps,
      glyphs: glyphTransforms,
      blockTransform: [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
      blockOpacity: clip.effectiveOpacity(options.currentFrame),
    };

    // Evaluate effects
    for (const inst of clip.effectStack) {
      const effect = builtinSubtitleEffects[inst.effectId];
      if (effect) {
        effect.apply(effectFrame, timeInfo, inst.params);
      }
    }

    if (effectFrame.blockOpacity <= 0) {
      ctx.restore();
      return;
    }

    // Alignment and base position
    const hAlign = style.hAlign ?? 'center';
    const vAlign = style.vAlign ?? 'bottom';

    let baseX = options.canvasWidth / 2 - totalWidth / 2;
    if (hAlign === 'left' || hAlign === 0) baseX = 80;
    else if (hAlign === 'right' || hAlign === 2) baseX = options.canvasWidth - 80 - totalWidth;

    let baseY = options.canvasHeight - 80;
    if (vAlign === 'top' || vAlign === 0) baseY = 80 + fontSize;
    else if (vAlign === 'middle' || vAlign === 1) baseY = options.canvasHeight / 2 + fontSize / 2;

    // Apply block translation
    const blockDx = effectFrame.blockTransform[12] ?? 0;
    const blockDy = effectFrame.blockTransform[13] ?? 0;
    baseX += blockDx;
    baseY += blockDy;

    // Optional Background Box
    if (style.boxEnabled) {
      const pad = 16;
      ctx.save();
      ctx.fillStyle = style.boxColor ?? 'rgba(0, 0, 0, 0.6)';
      ctx.globalAlpha = effectFrame.blockOpacity;
      ctx.fillRect(baseX - pad, baseY - fontSize - pad / 2, totalWidth + pad * 2, fontSize + pad);
      ctx.restore();
    }

    // Render individual glyphs
    let currentX = baseX;
    for (let i = 0; i < glyphs.length; i++) {
      const g = glyphs[i]!;
      const gt = effectFrame.glyphs[i]!;

      if (!gt.visible || gt.opacity <= 0) {
        currentX += g.layoutRect.width;
        continue;
      }

      ctx.save();
      ctx.globalAlpha = effectFrame.blockOpacity * gt.opacity;

      const glyphDx = gt.transform[12] ?? 0;
      const glyphDy = gt.transform[13] ?? 0;
      const gx = currentX + glyphDx;
      const gy = baseY + glyphDy;

      // Shadow
      if (style.shadowColor && (style.shadowBlur ?? 0) > 0) {
        ctx.shadowColor = style.shadowColor;
        ctx.shadowBlur = style.shadowBlur ?? 4;
        ctx.shadowOffsetX = style.shadowOffsetX ?? style.shadowOffset?.x ?? 2;
        ctx.shadowOffsetY = style.shadowOffsetY ?? style.shadowOffset?.y ?? 2;
      }

      // Outline
      if ((style.outlineWidth ?? 0) > 0) {
        ctx.strokeStyle = style.outlineColor ?? '#000000';
        ctx.lineWidth = (style.outlineWidth ?? 2) * 2;
        ctx.strokeText(g.char, gx, gy);
      }

      // Fill
      ctx.fillStyle = gt.color;
      ctx.fillText(g.char, gx, gy);

      ctx.restore();
      currentX += g.layoutRect.width;
    }

    ctx.restore();
  }
}
