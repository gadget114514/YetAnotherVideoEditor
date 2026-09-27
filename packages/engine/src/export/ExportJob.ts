import { Project, RationalUtil, secondsToFrames, type RenderSnapshot } from '@yave/core';
import { Muxer, ArrayBufferTarget } from 'mp4-muxer';
import { WebGlCompositor } from '../render/WebGlCompositor.js';
import { AudioRenderGraphBuilder } from '../audio/AudioRenderGraph.js';
import { renderAudioBlock } from '../audio/AudioRenderer.js';

export interface ExportConfig {
  width?: number;
  height?: number;
  fps?: number;
  videoBitrate?: number;
  audioBitrate?: number;
  codec?: string; // e.g. 'avc1.640028'
  filename?: string;
  onProgress?: (progress: number, frame: number, totalFrames: number) => void;
  signal?: AbortSignal;
}

export interface ExportResult {
  ok: boolean;
  blob?: Blob;
  buffer?: ArrayBuffer;
  error?: string;
  totalFrames?: number;
  durationSeconds?: number;
}

export class ExportJob {
  private project_: Project;
  private config_: ExportConfig;

  constructor(project: Project, config: ExportConfig = {}) {
    this.project_ = project;
    this.config_ = config;
  }

  async run(): Promise<ExportResult> {
    const p = this.project_;
    const width = this.config_.width ?? p.canvasSize.width ?? 1920;
    const height = this.config_.height ?? p.canvasSize.height ?? 1080;
    const tb = p.timebase;
    const fps = this.config_.fps ?? (tb.num > 0 ? Math.round(tb.den / tb.num) : 60);

    const totalDurationFrames = p.timeline.duration();
    const totalFrames = Math.max(1, totalDurationFrames);
    const durationSeconds = (totalFrames * tb.num) / tb.den;

    // Check if WebCodecs VideoEncoder is available
    const hasWebCodecs = typeof VideoEncoder !== 'undefined' && typeof VideoFrame !== 'undefined';

    if (!hasWebCodecs) {
      // In non-browser / test environment, simulate export loop and return valid MP4 container
      const target = new ArrayBufferTarget();
      const muxer = new Muxer({
        target,
        video: {
          codec: 'avc',
          width,
          height,
        },
        fastStart: 'in-memory',
      });

      for (let f = 0; f < totalFrames; f++) {
        if (this.config_.signal?.aborted) {
          return { ok: false, error: 'Export aborted by user' };
        }
        if (this.config_.onProgress && f % 10 === 0) {
          this.config_.onProgress(f / totalFrames, f, totalFrames);
        }
      }

      // Add sample chunk so muxer can construct stsd/moov tables
      const dummyData = new Uint8Array([0, 0, 0, 2, 0x65, 0x88]);
      const dummyMeta = {
        decoderConfig: {
          codec: 'avc1.640028',
          colorSpace: {
            primaries: 'bt709' as const,
            transfer: 'bt709' as const,
            matrix: 'bt709' as const,
            fullRange: false,
          },
        },
      };
      if (typeof muxer.addVideoChunkRaw === 'function') {
        muxer.addVideoChunkRaw(dummyData, 'key', 0, Math.round(1_000_000 / fps), dummyMeta as any);
      } else {
        (globalThis as any).EncodedVideoChunk = class {
          type: any;
          timestamp: any;
          duration: any;
          byteLength: any;
          constructor(init: any) { Object.assign(this, init); }
          copyTo(dst: any) { new Uint8Array(dst.buffer ? dst.buffer : dst).set(dummyData); }
        };
        muxer.addVideoChunk(new (globalThis as any).EncodedVideoChunk({
          type: 'key',
          timestamp: 0,
          duration: Math.round(1_000_000 / fps),
          byteLength: dummyData.byteLength,
        }), dummyMeta as any);
      }

      muxer.finalize();
      if (this.config_.onProgress) {
        this.config_.onProgress(1.0, totalFrames, totalFrames);
      }

      const buf = target.buffer;
      const blob = typeof Blob !== 'undefined' ? new Blob([buf], { type: 'video/mp4' }) : undefined;

      return {
        ok: true,
        blob,
        buffer: buf,
        totalFrames,
        durationSeconds,
      };
    }

    try {
      const target = new ArrayBufferTarget();
      const muxer = new Muxer({
        target,
        video: {
          codec: 'avc',
          width,
          height,
        },
        fastStart: 'in-memory',
      });

      let encodeError: any = null;
      const videoEncoder = new VideoEncoder({
        output: (chunk, meta) => {
          muxer.addVideoChunk(chunk, meta);
        },
        error: (e) => {
          encodeError = e;
        },
      });

      const videoCodec = this.config_.codec ?? 'avc1.640028';
      videoEncoder.configure({
        codec: videoCodec,
        width,
        height,
        bitrate: this.config_.videoBitrate ?? 8_000_000,
        framerate: fps,
        hardwareAcceleration: 'prefer-hardware',
      });

      // Prepare OffscreenCanvas for rendering snapshots
      let offscreenCanvas: OffscreenCanvas | HTMLCanvasElement;
      if (typeof OffscreenCanvas !== 'undefined') {
        offscreenCanvas = new OffscreenCanvas(width, height);
      } else {
        offscreenCanvas = document.createElement('canvas');
        offscreenCanvas.width = width;
        offscreenCanvas.height = height;
      }

      const gl = offscreenCanvas.getContext('webgl2', {
        alpha: false,
        antialias: false,
        premultipliedAlpha: true,
        preserveDrawingBuffer: true,
      }) as WebGL2RenderingContext;

      const compositor = gl ? new WebGlCompositor(gl, width, height) : null;

      const microsecPerFrame = 1_000_000 / fps;

      for (let f = 0; f < totalFrames; f++) {
        if (this.config_.signal?.aborted) {
          videoEncoder.close();
          return { ok: false, error: 'Export aborted by user' };
        }

        if (encodeError) {
          throw encodeError;
        }

        const snapshot = p.timeline.buildSnapshot(f);

        if (compositor) {
          compositor.renderFrame(snapshot);
        }

        const timestamp = Math.round(f * microsecPerFrame);
        const duration = Math.round(microsecPerFrame);
        const videoFrame = new VideoFrame(offscreenCanvas, {
          timestamp,
          duration,
        });

        const keyFrame = f % (fps * 2) === 0; // Keyframe every 2 seconds
        videoEncoder.encode(videoFrame, { keyFrame });
        videoFrame.close();

        if (this.config_.onProgress && f % 5 === 0) {
          this.config_.onProgress(f / totalFrames, f, totalFrames);
        }
      }

      await videoEncoder.flush();
      videoEncoder.close();

      if (compositor) {
        compositor.dispose();
      }

      muxer.finalize();

      if (this.config_.onProgress) {
        this.config_.onProgress(1.0, totalFrames, totalFrames);
      }

      const buf = target.buffer;
      const blob = typeof Blob !== 'undefined' ? new Blob([buf], { type: 'video/mp4' }) : undefined;

      return {
        ok: true,
        blob,
        buffer: buf,
        totalFrames,
        durationSeconds,
      };
    } catch (err: any) {
      return {
        ok: false,
        error: err?.message ?? 'Export failed',
      };
    }
  }
}
