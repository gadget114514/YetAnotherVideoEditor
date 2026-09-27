import React from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { usePlaybackStore } from '../store/usePlaybackStore.js';
import { Volume2, VolumeX, Mic } from 'lucide-react';

export const AudioMixerPanel: React.FC = () => {
  const { project, setTrackProperty } = useProjectStore();
  const { volume, muted, setVolume, setMuted } = usePlaybackStore();

  const audioTracks = project.timeline.tracks.filter(
    (t) => t.type === 'audio' || t.type === 'video',
  );

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
        <Mic size={16} className="text-emerald-400" />
        <span>Audio Mixer</span>
      </div>

      {/* Channel Strips */}
      <div style={{ flex: 1, overflowX: 'auto', display: 'flex', padding: '16px', gap: '16px' }}>
        {/* Master Channel */}
        <div style={{ width: '80px', backgroundColor: '#202024', borderRadius: '6px', border: '1px solid #3f3f46', padding: '12px 8px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ fontWeight: 700, color: '#38bdf8', fontSize: '11px' }}>MASTER</div>

          {/* Fader */}
          <div style={{ height: '140px', display: 'flex', alignItems: 'center' }}>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value={volume}
              onChange={(e) => setVolume(parseFloat(e.target.value))}
              style={{
                writingMode: 'vertical-lr',
                direction: 'rtl',
                height: '130px',
                width: '24px',
                accentColor: '#38bdf8',
                cursor: 'pointer',
              }}
            />
          </div>

          <div style={{ fontSize: '10px', color: '#a1a1aa' }}>{Math.round(volume * 100)}%</div>

          <button
            onClick={() => setMuted(!muted)}
            style={{
              background: muted ? '#ef4444' : '#27272a',
              border: '1px solid #3f3f46',
              borderRadius: '4px',
              padding: '4px',
              color: '#fff',
              cursor: 'pointer',
            }}
          >
            {muted ? <VolumeX size={14} /> : <Volume2 size={14} />}
          </button>
        </div>

        <div style={{ width: '1px', backgroundColor: '#27272a' }} />

        {/* Per-Track Channels */}
        {audioTracks.map((track) => (
          <div
            key={track.id}
            style={{
              width: '80px',
              backgroundColor: '#202024',
              borderRadius: '6px',
              border: '1px solid #2e2e34',
              padding: '12px 8px',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}
          >
            <div style={{ fontWeight: 600, color: '#e4e4e7', fontSize: '11px', textAlign: 'center', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', width: '100%' }}>
              {track.name}
            </div>

            {/* Fader */}
            <div style={{ height: '140px', display: 'flex', alignItems: 'center' }}>
              <input
                type="range"
                min="0"
                max="2"
                step="0.05"
                value={track.gain}
                onChange={(e) => setTrackProperty(track.id, 'Gain' as any, parseFloat(e.target.value))}
                style={{
                  writingMode: 'vertical-lr',
                  direction: 'rtl',
                  height: '130px',
                  width: '24px',
                  accentColor: '#10b981',
                  cursor: 'pointer',
                }}
              />
            </div>

            <div style={{ fontSize: '10px', color: '#a1a1aa' }}>{track.gain.toFixed(2)}x</div>

            <button
              onClick={() => setTrackProperty(track.id, 'Muted' as any, !track.muted)}
              style={{
                background: track.muted ? '#ef4444' : '#27272a',
                border: '1px solid #3f3f46',
                borderRadius: '4px',
                padding: '4px',
                color: '#fff',
                cursor: 'pointer',
              }}
            >
              {track.muted ? <VolumeX size={14} /> : <Volume2 size={14} />}
            </button>
          </div>
        ))}
      </div>
    </div>
  );
};
