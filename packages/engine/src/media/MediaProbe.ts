function getNodeFs(): any {
  if (typeof process !== 'undefined') {
    if (typeof (process as any).getBuiltinModule === 'function') {
      try {
        return (process as any).getBuiltinModule('fs');
      } catch {
        // ignore
      }
    }
    try {
      const req = (globalThis as any).require;
      if (typeof req === 'function') {
        return req('fs');
      }
    } catch {
      // ignore
    }
  }
  return null;
}


export interface MediaInfo {
  ok: boolean;
  error: string;
  hasAudio: boolean;
  hasVideo: boolean;
  audioSampleRate: number;
  audioChannels: number;
  audioDurationFrames: number;
  videoWidth: number;
  videoHeight: number;
  videoDurationFrames: number;
  videoFramerate: number;
}

export class MediaProbe {
  /**
   * Probes a media source (file path string, Uint8Array, or ArrayBuffer).
   */
  static probe(source: string | Uint8Array | ArrayBuffer): MediaInfo {
    let bytes: Uint8Array;

    if (typeof source === 'string') {
      const nodeFs = getNodeFs();
      if (!nodeFs) {
        return this.errorResult(`File path probing is not supported in this environment: ${source}`);
      }
      try {
        if (!nodeFs.existsSync(source)) {
          return this.errorResult(`File not found: ${source}`);
        }
        bytes = nodeFs.readFileSync(source);
      } catch (err: any) {
        return this.errorResult(err?.message ?? 'Failed to read file');
      }
    } else if (source instanceof Uint8Array) {
      bytes = source;
    } else if (source instanceof ArrayBuffer) {
      bytes = new Uint8Array(source);
    } else {
      return this.errorResult('Invalid source format');
    }

    if (bytes.length < 12) {
      return this.errorResult('File too small to be a valid media container');
    }

    // Check RIFF / WAVE
    const riff = String.fromCharCode(bytes[0]!, bytes[1]!, bytes[2]!, bytes[3]!);
    const wave = String.fromCharCode(bytes[8]!, bytes[9]!, bytes[10]!, bytes[11]!);

    if (riff === 'RIFF' && wave === 'WAVE') {
      return this.probeWav(bytes);
    }

    // Default or unknown container
    return this.errorResult('Unrecognized media container format');
  }

  private static probeWav(bytes: Uint8Array): MediaInfo {
    let offset = 12;
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

    let sampleRate = 0;
    let channels = 0;
    let bitsPerSample = 0;
    let dataSize = 0;
    let foundFmt = false;
    let foundData = false;

    while (offset + 8 <= bytes.length) {
      const chunkId = String.fromCharCode(
        bytes[offset]!,
        bytes[offset + 1]!,
        bytes[offset + 2]!,
        bytes[offset + 3]!
      );
      const chunkSize = view.getUint32(offset + 4, true);
      offset += 8;

      if (chunkId === 'fmt ') {
        if (chunkSize >= 14) {
          channels = view.getUint16(offset + 2, true);
          sampleRate = view.getUint32(offset + 4, true);
          if (chunkSize >= 16) {
            bitsPerSample = view.getUint16(offset + 14, true);
          } else {
            bitsPerSample = 16;
          }
          foundFmt = true;
        }
      } else if (chunkId === 'data') {
        dataSize = chunkSize;
        foundData = true;
      }

      offset += chunkSize;
      // Chunks must be word-aligned (even byte boundary)
      if (chunkSize % 2 === 1) {
        offset++;
      }
    }

    if (!foundFmt || !foundData) {
      return this.errorResult('Corrupt or incomplete WAV file (missing fmt or data chunk)');
    }

    const bytesPerSample = Math.max(1, Math.trunc(bitsPerSample / 8));
    const blockAlign = channels * bytesPerSample;
    const durationFrames = blockAlign > 0 ? Math.trunc(dataSize / blockAlign) : 0;

    return {
      ok: true,
      error: '',
      hasAudio: true,
      hasVideo: false,
      audioSampleRate: sampleRate,
      audioChannels: channels,
      audioDurationFrames: durationFrames,
      videoWidth: 0,
      videoHeight: 0,
      videoDurationFrames: 0,
      videoFramerate: 0,
    };
  }

  private static errorResult(msg: string): MediaInfo {
    return {
      ok: false,
      error: msg,
      hasAudio: false,
      hasVideo: false,
      audioSampleRate: 0,
      audioChannels: 0,
      audioDurationFrames: 0,
      videoWidth: 0,
      videoHeight: 0,
      videoDurationFrames: 0,
      videoFramerate: 0,
    };
  }
}
