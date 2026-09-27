import { create } from 'zustand';

export interface PlaybackState {
  currentFrame: number;
  isPlaying: boolean;
  loop: boolean;
  fps: number;
  volume: number;
  muted: boolean;

  play: () => void;
  pause: () => void;
  togglePlay: () => void;
  seek: (frame: number) => void;
  setCurrentFrame: (frame: number) => void;
  stepForward: (delta?: number) => void;
  stepBackward: (delta?: number) => void;
  stepFrames: (delta: number) => void;
  setLoop: (loop: boolean) => void;
  setVolume: (vol: number) => void;
  setMuted: (muted: boolean) => void;
  setFps: (fps: number) => void;
}

export const usePlaybackStore = create<PlaybackState>((set, get) => ({
  currentFrame: 0,
  isPlaying: false,
  loop: false,
  fps: 60,
  volume: 1.0,
  muted: false,

  play: () => set({ isPlaying: true }),
  pause: () => set({ isPlaying: false }),
  togglePlay: () => set((s) => ({ isPlaying: !s.isPlaying })),
  seek: (frame: number) => set({ currentFrame: Math.max(0, Math.trunc(frame)) }),
  setCurrentFrame: (frame: number) => set({ currentFrame: Math.max(0, Math.trunc(frame)) }),
  stepForward: (delta: number = 1) =>
    set((s) => ({ currentFrame: Math.max(0, s.currentFrame + delta) })),
  stepBackward: (delta: number = 1) =>
    set((s) => ({ currentFrame: Math.max(0, s.currentFrame - delta) })),
  stepFrames: (delta: number) =>
    set((s) => ({ currentFrame: Math.max(0, s.currentFrame + delta) })),
  setLoop: (loop: boolean) => set({ loop }),
  setVolume: (vol: number) => set({ volume: Math.max(0, Math.min(1, vol)) }),
  setMuted: (muted: boolean) => set({ muted }),
  setFps: (fps: number) => set({ fps }),
}));
