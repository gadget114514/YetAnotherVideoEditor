export class TexturePool {
  private gl_: WebGL2RenderingContext;
  private pool_: { tex: WebGLTexture; width: number; height: number; lastUsed: number }[] = [];
  private currentFrame_: number = 0;

  constructor(gl: WebGL2RenderingContext) {
    this.gl_ = gl;
  }

  advanceFrame(): void {
    this.currentFrame_++;
    // Release textures unused for > 120 frames
    if (this.currentFrame_ % 60 === 0) {
      this.purgeUnused(120);
    }
  }

  acquire(width: number, height: number): WebGLTexture {
    const gl = this.gl_;
    const idx = this.pool_.findIndex(
      (item) => item.width === width && item.height === height
    );

    if (idx !== -1) {
      const [item] = this.pool_.splice(idx, 1);
      return item!.tex;
    }

    // Allocate new texture
    const tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA8,
      width,
      height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      null
    );
    gl.bindTexture(gl.TEXTURE_2D, null);

    return tex;
  }

  release(tex: WebGLTexture, width: number, height: number): void {
    this.pool_.push({
      tex,
      width,
      height,
      lastUsed: this.currentFrame_,
    });
  }

  purgeUnused(maxAge: number): void {
    const gl = this.gl_;
    const kept: typeof this.pool_ = [];
    for (const item of this.pool_) {
      if (this.currentFrame_ - item.lastUsed > maxAge) {
        gl.deleteTexture(item.tex);
      } else {
        kept.push(item);
      }
    }
    this.pool_ = kept;
  }

  dispose(): void {
    const gl = this.gl_;
    for (const item of this.pool_) {
      gl.deleteTexture(item.tex);
    }
    this.pool_ = [];
  }
}

export interface FboItem {
  fbo: WebGLFramebuffer;
  tex: WebGLTexture;
  width: number;
  height: number;
}

export class FboPool {
  private gl_: WebGL2RenderingContext;
  private pool_: (FboItem & { lastUsed: number })[] = [];
  private currentFrame_: number = 0;

  constructor(gl: WebGL2RenderingContext) {
    this.gl_ = gl;
  }

  advanceFrame(): void {
    this.currentFrame_++;
    if (this.currentFrame_ % 60 === 0) {
      this.purgeUnused(120);
    }
  }

  acquire(width: number, height: number): FboItem {
    const gl = this.gl_;
    const idx = this.pool_.findIndex(
      (item) => item.width === width && item.height === height
    );

    if (idx !== -1) {
      const [item] = this.pool_.splice(idx, 1);
      return { fbo: item!.fbo, tex: item!.tex, width, height };
    }

    // Create new FBO + Texture attachment
    const fbo = gl.createFramebuffer()!;
    const tex = gl.createTexture()!;

    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA8,
      width,
      height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      null
    );

    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
    gl.framebufferTexture2D(
      gl.FRAMEBUFFER,
      gl.COLOR_ATTACHMENT0,
      gl.TEXTURE_2D,
      tex,
      0
    );

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.bindTexture(gl.TEXTURE_2D, null);

    return { fbo, tex, width, height };
  }

  release(item: FboItem): void {
    this.pool_.push({
      ...item,
      lastUsed: this.currentFrame_,
    });
  }

  purgeUnused(maxAge: number): void {
    const gl = this.gl_;
    const kept: typeof this.pool_ = [];
    for (const item of this.pool_) {
      if (this.currentFrame_ - item.lastUsed > maxAge) {
        gl.deleteFramebuffer(item.fbo);
        gl.deleteTexture(item.tex);
      } else {
        kept.push(item);
      }
    }
    this.pool_ = kept;
  }

  dispose(): void {
    const gl = this.gl_;
    for (const item of this.pool_) {
      gl.deleteFramebuffer(item.fbo);
      gl.deleteTexture(item.tex);
    }
    this.pool_ = [];
  }
}
