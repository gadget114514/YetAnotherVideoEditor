import { type RenderSnapshot, type LayerItem, BlendMode } from '@yave/core';
import { quadVertShader, compositeFragShader } from './shaders/compositeShaders.js';
import { filterColorFragShader, filterBlurFragShader } from './shaders/filterShaders.js';
import { transitionFragShader } from './shaders/transitionShaders.js';
import { TexturePool, FboPool, type FboItem } from './TexturePool.js';

export interface TextureProvider {
  getTexture(layer: LayerItem): WebGLTexture | TexImageSource | null;
}

export class WebGlCompositor {
  private gl_: WebGL2RenderingContext;
  private width_: number = 1920;
  private height_: number = 1080;

  private quadVao_: WebGLVertexArrayObject | null = null;
  private quadVbo_: WebGLBuffer | null = null;

  // Shader Programs
  private compositeProg_: WebGLProgram | null = null;
  private filterColorProg_: WebGLProgram | null = null;
  private filterBlurProg_: WebGLProgram | null = null;
  private transitionProg_: WebGLProgram | null = null;
  private presentProg_: WebGLProgram | null = null;

  // Ping-Pong FBOs
  private fboPool_: FboPool;
  private texturePool_: TexturePool;

  // Reusable 1x1 solid textures & canvas
  private solidColorTexMap_: Map<string, WebGLTexture> = new Map();
  private offscreenCanvas_: HTMLCanvasElement | OffscreenCanvas | null = null;
  private offscreenCtx_: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null = null;
  private textTexture_: WebGLTexture | null = null;

  constructor(gl: WebGL2RenderingContext, width: number = 1920, height: number = 1080) {
    this.gl_ = gl;
    this.width_ = width;
    this.height_ = height;
    this.fboPool_ = new FboPool(gl);
    this.texturePool_ = new TexturePool(gl);
    this.init();
  }

  private init(): void {
    const gl = this.gl_;

    // Setup Quad Geometry
    const quadVertices = new Float32Array([
      -1, -1,
       1, -1,
      -1,  1,
       1,  1,
    ]);
    this.quadVao_ = gl.createVertexArray();
    gl.bindVertexArray(this.quadVao_);
    this.quadVbo_ = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVbo_);
    gl.bufferData(gl.ARRAY_BUFFER, quadVertices, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.bindVertexArray(null);

    // Compile programs
    this.compositeProg_ = this.compileProgram(quadVertShader, compositeFragShader);
    this.filterColorProg_ = this.compileProgram(quadVertShader, filterColorFragShader);
    this.filterBlurProg_ = this.compileProgram(quadVertShader, filterBlurFragShader);
    this.transitionProg_ = this.compileProgram(quadVertShader, transitionFragShader);

    // Present Shader
    const presentFrag = `#version 300 es
    precision highp float;
    in vec2 vUv;
    out vec4 fragColor;
    uniform sampler2D uTex;
    void main() {
        fragColor = texture(uTex, vUv);
    }
    `;
    this.presentProg_ = this.compileProgram(quadVertShader, presentFrag);

    // Prepare offscreen canvas for subtitles
    if (typeof OffscreenCanvas !== 'undefined') {
      this.offscreenCanvas_ = new OffscreenCanvas(this.width_, this.height_);
      this.offscreenCtx_ = this.offscreenCanvas_.getContext('2d');
    } else if (typeof document !== 'undefined') {
      const c = document.createElement('canvas');
      c.width = this.width_;
      c.height = this.height_;
      this.offscreenCanvas_ = c;
      this.offscreenCtx_ = c.getContext('2d');
    }
  }

  resize(width: number, height: number): void {
    if (this.width_ !== width || this.height_ !== height) {
      this.width_ = width;
      this.height_ = height;
      this.fboPool_.dispose();
      this.texturePool_.dispose();
      if (this.offscreenCanvas_) {
        this.offscreenCanvas_.width = width;
        this.offscreenCanvas_.height = height;
      }
    }
  }

  renderFrame(snapshot: RenderSnapshot, provider?: TextureProvider): void {
    const gl = this.gl_;
    this.fboPool_.advanceFrame();
    this.texturePool_.advanceFrame();

    const w = snapshot.canvasSize?.width || this.width_;
    const h = snapshot.canvasSize?.height || this.height_;
    this.resize(w, h);

    // Acquire ping-pong FBOs
    let fboA = this.fboPool_.acquire(w, h);
    let fboB = this.fboPool_.acquire(w, h);

    // 1. Clear FBO A to transparent black
    gl.bindFramebuffer(gl.FRAMEBUFFER, fboA.fbo);
    gl.viewport(0, 0, w, h);
    gl.clearColor(0.0, 0.0, 0.0, 0.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    // 2. Iterate layers from back to front
    for (const layer of snapshot.layers) {
      let layerTex = this.resolveLayerTexture(layer, provider);
      if (!layerTex) {
        continue;
      }

      // Apply Filters if any
      if (layer.filters && layer.filters.length > 0) {
        layerTex = this.applyFilters(layerTex, layer.filters, w, h);
      }

      // Composite onto FBO B
      gl.bindFramebuffer(gl.FRAMEBUFFER, fboB.fbo);
      gl.viewport(0, 0, w, h);

      gl.useProgram(this.compositeProg_!);
      gl.bindVertexArray(this.quadVao_);

      // Unit 0: Layer texture
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, layerTex);
      gl.uniform1i(gl.getUniformLocation(this.compositeProg_!, 'uSrcTex'), 0);

      // Unit 1: Background texture (fboA)
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, fboA.tex);
      gl.uniform1i(gl.getUniformLocation(this.compositeProg_!, 'uDstTex'), 1);

      // Uniforms
      gl.uniform1f(gl.getUniformLocation(this.compositeProg_!, 'uOpacity'), layer.opacity ?? 1.0);
      const blendIndex = this.blendModeToIndex(layer.blendMode);
      gl.uniform1i(gl.getUniformLocation(this.compositeProg_!, 'uBlendMode'), blendIndex);

      const crop = layer.crop ? [layer.crop.x, layer.crop.y, layer.crop.width, layer.crop.height] : [0, 0, 1, 1];
      gl.uniform4f(
        gl.getUniformLocation(this.compositeProg_!, 'uCropRect'),
        crop[0] ?? 0,
        crop[1] ?? 0,
        crop[2] ?? 1,
        crop[3] ?? 1
      );

      const tr = layer.transform;
      const posX = tr?.position?.x ?? 0;
      const posY = tr?.position?.y ?? 0;
      const scaleX = tr?.scale?.x ?? 1;
      const scaleY = tr?.scale?.y ?? 1;
      const rot = ((tr?.rotation ?? 0) * Math.PI) / 180.0;
      const cosR = Math.cos(rot);
      const sinR = Math.sin(rot);

      const transformMat = new Float32Array([
        scaleX * cosR, scaleX * sinR, 0, 0,
       -scaleY * sinR, scaleY * cosR, 0, 0,
        0,             0,             1, 0,
        posX,          posY,          0, 1,
      ]);
      gl.uniformMatrix4fv(gl.getUniformLocation(this.compositeProg_!, 'uTransform'), false, transformMat);

      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

      // Swap ping-pong FBOs
      const tmp = fboA;
      fboA = fboB;
      fboB = tmp;
    }

    // 3. Present final FBO A to canvas default framebuffer
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    const canvasWidth = gl.canvas.width;
    const canvasHeight = gl.canvas.height;
    gl.viewport(0, 0, canvasWidth, canvasHeight);
    gl.clearColor(0.05, 0.05, 0.07, 1.0); // Dark background for letterbox
    gl.clear(gl.COLOR_BUFFER_BIT);

    // Aspect-ratio letterbox viewport
    const scale = Math.min(canvasWidth / w, canvasHeight / h);
    const vpW = Math.round(w * scale);
    const vpH = Math.round(h * scale);
    const vpX = Math.round((canvasWidth - vpW) / 2);
    const vpY = Math.round((canvasHeight - vpH) / 2);
    gl.viewport(vpX, vpY, vpW, vpH);

    gl.useProgram(this.presentProg_!);
    gl.bindVertexArray(this.quadVao_);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, fboA.tex);
    gl.uniform1i(gl.getUniformLocation(this.presentProg_!, 'uTex'), 0);

    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

    // Release temporary FBOs
    this.fboPool_.release(fboA);
    this.fboPool_.release(fboB);
  }

  private resolveLayerTexture(layer: LayerItem, provider?: TextureProvider): WebGLTexture | null {
    const gl = this.gl_;

    // Provider check
    if (provider) {
      const pTex = provider.getTexture(layer);
      if (pTex) {
        if (pTex instanceof WebGLTexture) {
          return pTex;
        }
        // TexImageSource (VideoFrame, HTMLImageElement, HTMLCanvasElement, etc.)
        const tex = this.texturePool_.acquire(this.width_, this.height_);
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, pTex as any);
        gl.bindTexture(gl.TEXTURE_2D, null);
        return tex;
      }
    }

    // Check layer source
    const src = layer.source;
    if (!src) return null;

    if (src.kind === 'color') {
      return this.getSolidColorTexture(src.color);
    }

    if (src.kind === 'subtitle') {
      return this.renderSubtitleToTexture(src.text, src.style);
    }

    if (src.kind === 'placeholder') {
      return this.getSolidColorTexture('#2d3748');
    }

    return null;
  }

  private getSolidColorTexture(colorHex: string): WebGLTexture {
    const gl = this.gl_;
    const cached = this.solidColorTexMap_.get(colorHex);
    if (cached) {
      return cached;
    }

    const { r, g, b, a } = this.parseColor(colorHex);
    const tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      1,
      1,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array([r, g, b, a])
    );
    gl.bindTexture(gl.TEXTURE_2D, null);

    this.solidColorTexMap_.set(colorHex, tex);
    return tex;
  }

  private renderSubtitleToTexture(text: string, style: any): WebGLTexture | null {
    if (!this.offscreenCtx_ || !this.offscreenCanvas_) return null;
    const ctx = this.offscreenCtx_;
    const gl = this.gl_;
    const w = this.width_;
    const h = this.height_;

    ctx.clearRect(0, 0, w, h);
    if (text) {
      const fontSize = style?.fontPointSize ?? 48;
      const fontFamily = style?.fontFamily ?? 'sans-serif';
      ctx.font = `${fontSize}px ${fontFamily}`;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'bottom';

      // Outline
      if (style?.outlineWidth && style?.outlineWidth > 0) {
        ctx.strokeStyle = style.outlineColor ?? '#000000';
        ctx.lineWidth = style.outlineWidth * 2;
        ctx.strokeText(text, w / 2, h - 80);
      }

      // Fill
      ctx.fillStyle = style?.fillColor ?? '#ffffff';
      ctx.fillText(text, w / 2, h - 80);
    }

    if (!this.textTexture_) {
      this.textTexture_ = gl.createTexture()!;
    }

    gl.bindTexture(gl.TEXTURE_2D, this.textTexture_);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, this.offscreenCanvas_ as any);
    gl.bindTexture(gl.TEXTURE_2D, null);

    return this.textTexture_;
  }

  private applyFilters(tex: WebGLTexture, filters: readonly any[], w: number, h: number): WebGLTexture {
    let currentTex = tex;
    const gl = this.gl_;

    for (const f of filters) {
      if (f.filterId === 'yave.filter.colorAdjust' || f.filterId === 'yave.filter.mono' || f.filterId === 'yave.filter.sepia') {
        const outFbo = this.fboPool_.acquire(w, h);
        gl.bindFramebuffer(gl.FRAMEBUFFER, outFbo.fbo);
        gl.viewport(0, 0, w, h);

        gl.useProgram(this.filterColorProg_!);
        gl.bindVertexArray(this.quadVao_);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, currentTex);
        gl.uniform1i(gl.getUniformLocation(this.filterColorProg_!, 'uTex'), 0);

        const params = f.params ?? [];
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uBrightness'), params[0] ?? 0.0);
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uContrast'), params[1] ?? 1.0);
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uSaturation'), params[2] ?? 1.0);
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uGamma'), params[3] ?? 1.0);
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uMono'), f.filterId === 'yave.filter.mono' ? (params[0] ?? 1.0) : 0.0);
        gl.uniform1f(gl.getUniformLocation(this.filterColorProg_!, 'uSepia'), f.filterId === 'yave.filter.sepia' ? (params[0] ?? 1.0) : 0.0);

        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
        currentTex = outFbo.tex;
      }
    }

    return currentTex;
  }

  private blendModeToIndex(bm?: string): number {
    switch (bm) {
      case BlendMode.Normal: return 0;
      case BlendMode.Add: return 1;
      case BlendMode.Multiply: return 2;
      case BlendMode.Screen: return 3;
      case BlendMode.Overlay: return 4;
      case BlendMode.Darken: return 5;
      case BlendMode.Lighten: return 6;
      case BlendMode.Difference: return 7;
      case BlendMode.Exclusion: return 8;
      case BlendMode.AlphaMask: return 9;
      default: return 0;
    }
  }

  private parseColor(hex: string): { r: number; g: number; b: number; a: number } {
    let clean = hex.replace('#', '');
    if (clean.length === 3) {
      clean = clean.split('').map((c) => c + c).join('');
    }
    if (clean.length === 6) {
      clean += 'ff';
    }
    const num = parseInt(clean, 16);
    return {
      r: (num >> 24) & 255,
      g: (num >> 16) & 255,
      b: (num >> 8) & 255,
      a: num & 255,
    };
  }

  private compileProgram(vertSrc: string, fragSrc: string): WebGLProgram {
    const gl = this.gl_;
    const vert = gl.createShader(gl.VERTEX_SHADER)!;
    gl.shaderSource(vert, vertSrc);
    gl.compileShader(vert);
    if (!gl.getShaderParameter(vert, gl.COMPILE_STATUS)) {
      throw new Error(`Vert shader error: ${gl.getShaderInfoLog(vert)}`);
    }

    const frag = gl.createShader(gl.FRAGMENT_SHADER)!;
    gl.shaderSource(frag, fragSrc);
    gl.compileShader(frag);
    if (!gl.getShaderParameter(frag, gl.COMPILE_STATUS)) {
      throw new Error(`Frag shader error: ${gl.getShaderInfoLog(frag)}`);
    }

    const prog = gl.createProgram()!;
    gl.attachShader(prog, vert);
    gl.attachShader(prog, frag);
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      throw new Error(`Program link error: ${gl.getProgramInfoLog(prog)}`);
    }

    return prog;
  }

  dispose(): void {
    const gl = this.gl_;
    this.fboPool_.dispose();
    this.texturePool_.dispose();
    for (const tex of this.solidColorTexMap_.values()) {
      gl.deleteTexture(tex);
    }
    this.solidColorTexMap_.clear();
    if (this.textTexture_) gl.deleteTexture(this.textTexture_);
    if (this.quadVbo_) gl.deleteBuffer(this.quadVbo_);
    if (this.quadVao_) gl.deleteVertexArray(this.quadVao_);
    if (this.compositeProg_) gl.deleteProgram(this.compositeProg_);
    if (this.filterColorProg_) gl.deleteProgram(this.filterColorProg_);
    if (this.filterBlurProg_) gl.deleteProgram(this.filterBlurProg_);
    if (this.transitionProg_) gl.deleteProgram(this.transitionProg_);
    if (this.presentProg_) gl.deleteProgram(this.presentProg_);
  }
}
