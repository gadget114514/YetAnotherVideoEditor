/**
 * Planar 32-bit floating point audio buffer.
 */
export class DecodedAudio {
  sampleRate: number = 48000;
  channels: number = 2;
  channelData: Float32Array[] = [];

  constructor(frames: number = 0, channels: number = 2, sampleRate: number = 48000) {
    this.sampleRate = sampleRate;
    this.channels = channels;
    for (let c = 0; c < channels; c++) {
      this.channelData.push(new Float32Array(frames));
    }
  }

  frames(): number {
    return this.channelData.length > 0 ? this.channelData[0]!.length : 0;
  }

  planar(): Float32Array[] {
    return this.channelData;
  }
}

export interface IAudioSourceProvider {
  decodeAsset(assetId: string): DecodedAudio | null;
}
