import { create } from 'zustand';

export interface SelectionState {
  selectedClipId: string | null;
  selectedTrackId: string | null;

  selectClip: (clipId: string | null) => void;
  selectTrack: (trackId: string | null) => void;
  clearSelection: () => void;
}

export const useSelectionStore = create<SelectionState>((set) => ({
  selectedClipId: null,
  selectedTrackId: null,

  selectClip: (clipId: string | null) =>
    set({ selectedClipId: clipId, selectedTrackId: null }),

  selectTrack: (trackId: string | null) =>
    set({ selectedTrackId: trackId, selectedClipId: null }),

  clearSelection: () => set({ selectedClipId: null, selectedTrackId: null }),
}));
