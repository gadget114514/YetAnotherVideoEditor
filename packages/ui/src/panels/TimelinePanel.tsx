import React, { useRef, useState } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { usePlaybackStore } from '../store/usePlaybackStore.js';
import { useSelectionStore } from '../store/useSelectionStore.js';
import {
  type Track,
  type Clip,
  VideoClip,
  AudioClip,
  SubtitleClip,
  TitleClip,
  type VideoFilterInstance,
} from '@yave/core';
import {
  Eye,
  EyeOff,
  Volume2,
  VolumeX,
  Lock,
  Unlock,
  Scissors,
  Trash2,
  Plus,
  ZoomIn,
  ZoomOut,
} from 'lucide-react';

export const TimelinePanel: React.FC = () => {
  const { project, addTrack, removeClip, splitClipAt, addClipToTrack, setTrackProperty, touch } =
    useProjectStore();
  const { currentFrame, seek, fps } = usePlaybackStore();
  const { selectedClipId, selectedTrackId, selectClip, selectTrack } = useSelectionStore();

  const [zoom, setZoom] = useState<number>(0.5); // pixels per frame
  const timelineBodyRef = useRef<HTMLDivElement>(null);

  const duration = Math.max(1800, project.timeline.duration() + 600);
  const totalWidth = duration * zoom;

  const handleRulerClick = (e: React.MouseEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const clickX = e.clientX - rect.left;
    const targetFrame = Math.max(0, Math.round(clickX / zoom));
    seek(targetFrame);
  };

  const handleSplit = () => {
    if (!selectedClipId) return;
    splitClipAt(selectedClipId, currentFrame);
  };

  const handleDelete = () => {
    if (!selectedClipId) return;
    removeClip(selectedClipId);
    selectClip(null);
  };

  const handleDropOnTrack = (e: React.DragEvent, track: Track) => {
    e.preventDefault();
    const dataStr = e.dataTransfer.getData('application/json');
    if (!dataStr) return;

    try {
      const data = JSON.parse(dataStr);
      const rect = e.currentTarget.getBoundingClientRect();
      const dropX = e.clientX - rect.left;
      const dropFrame = Math.max(0, Math.round(dropX / zoom));

      if (data.type === 'asset') {
        let clip: Clip;
        if (data.kind === 'audio' || track.type === 'audio') {
          clip = new AudioClip(data.assetId);
          clip.setRange({ start: dropFrame, duration: 300 });
        } else {
          clip = new VideoClip(data.assetId);
          clip.setRange({ start: dropFrame, duration: 300 });
        }
        clip.setName(data.name || 'Imported Clip');
        addClipToTrack(track.id, clip);
      } else if (data.type === 'filter') {
        const target = track.clipAt(dropFrame);
        if (target) {
          target.addFilter({ filterId: data.filterId, params: {}, enabled: true });
          touch();
        }
      }
    } catch {
      // ignore invalid drag data
    }
  };

  const getClipColor = (clip: Clip) => {
    if (clip instanceof SubtitleClip) {
      return clip.type === 'title' ? '#9333ea' : '#d97706';
    }
    if (clip instanceof AudioClip) {
      return '#059669';
    }
    return '#2563eb';
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#141416', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Timeline Action Bar */}
      <div style={{ height: '36px', borderBottom: '1px solid #27272a', backgroundColor: '#1c1c1f', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={handleSplit}
            disabled={!selectedClipId}
            title="Split Clip at Playhead"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: selectedClipId ? '#27272a' : '#1e1e20',
              border: '1px solid #3f3f46',
              borderRadius: '4px',
              color: selectedClipId ? '#fff' : '#71717a',
              cursor: selectedClipId ? 'pointer' : 'default',
              fontSize: '11px',
            }}
          >
            <Scissors size={13} />
            <span>Split</span>
          </button>
          <button
            onClick={handleDelete}
            disabled={!selectedClipId}
            title="Delete Selected Clip"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: selectedClipId ? '#dc2626' : '#1e1e20',
              border: '1px solid #7f1d1d',
              borderRadius: '4px',
              color: selectedClipId ? '#fff' : '#71717a',
              cursor: selectedClipId ? 'pointer' : 'default',
              fontSize: '11px',
            }}
          >
            <Trash2 size={13} />
            <span>Delete</span>
          </button>

          <div style={{ height: '16px', width: '1px', backgroundColor: '#3f3f46', margin: '0 4px' }} />

          <button
            onClick={() => addTrack('video')}
            style={{ background: 'none', border: '1px solid #3f3f46', borderRadius: '4px', padding: '4px 8px', color: '#38bdf8', cursor: 'pointer', fontSize: '11px' }}
          >
            + Video
          </button>
          <button
            onClick={() => addTrack('audio')}
            style={{ background: 'none', border: '1px solid #3f3f46', borderRadius: '4px', padding: '4px 8px', color: '#34d399', cursor: 'pointer', fontSize: '11px' }}
          >
            + Audio
          </button>
          <button
            onClick={() => addTrack('subtitle')}
            style={{ background: 'none', border: '1px solid #3f3f46', borderRadius: '4px', padding: '4px 8px', color: '#fbbf24', cursor: 'pointer', fontSize: '11px' }}
          >
            + Subtitle
          </button>
        </div>

        {/* Zoom controls */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <button
            onClick={() => setZoom((z) => Math.max(0.1, z * 0.8))}
            style={{ background: 'none', border: 'none', color: '#a1a1aa', cursor: 'pointer', padding: '4px' }}
          >
            <ZoomOut size={15} />
          </button>
          <span style={{ fontSize: '11px', color: '#71717a', minWidth: '40px', textAlign: 'center' }}>
            {Math.round(zoom * 100)}%
          </span>
          <button
            onClick={() => setZoom((z) => Math.min(3.0, z * 1.2))}
            style={{ background: 'none', border: 'none', color: '#a1a1aa', cursor: 'pointer', padding: '4px' }}
          >
            <ZoomIn size={15} />
          </button>
        </div>
      </div>

      {/* Main Timeline Viewport */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Track Headers Column */}
        <div style={{ width: '180px', backgroundColor: '#18181b', borderRight: '1px solid #27272a', display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
          {/* Header spacer above ruler */}
          <div style={{ height: '28px', borderBottom: '1px solid #27272a', backgroundColor: '#1f1f23', padding: '0 8px', display: 'flex', alignItems: 'center', fontSize: '11px', color: '#71717a', fontWeight: 600 }}>
            TRACKS
          </div>

          {/* Track headers list */}
          <div style={{ flex: 1, overflowY: 'hidden' }}>
            {project.timeline.tracks.map((track) => (
              <div
                key={track.id}
                onClick={() => selectTrack(track.id)}
                style={{
                  height: `${track.uiHeight}px`,
                  borderBottom: '1px solid #27272a',
                  backgroundColor: selectedTrackId === track.id ? '#2e2e34' : '#1c1c20',
                  padding: '6px 8px',
                  display: 'flex',
                  flexDirection: 'column',
                  justifyContent: 'space-between',
                  boxSizing: 'border-box',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <span style={{ fontWeight: 600, color: '#e4e4e7' }}>{track.name}</span>
                  <span style={{ fontSize: '10px', color: '#71717a', textTransform: 'capitalize' }}>
                    {track.type}
                  </span>
                </div>

                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      track.visible = !track.visible;
                      touch();
                    }}
                    style={{ background: 'none', border: 'none', color: track.visible ? '#e4e4e7' : '#52525b', cursor: 'pointer', padding: 0 }}
                  >
                    {track.visible ? <Eye size={14} /> : <EyeOff size={14} />}
                  </button>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      track.muted = !track.muted;
                      touch();
                    }}
                    style={{ background: 'none', border: 'none', color: track.muted ? '#ef4444' : '#e4e4e7', cursor: 'pointer', padding: 0 }}
                  >
                    {track.muted ? <VolumeX size={14} /> : <Volume2 size={14} />}
                  </button>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      track.locked = !track.locked;
                      touch();
                    }}
                    style={{ background: 'none', border: 'none', color: track.locked ? '#f59e0b' : '#52525b', cursor: 'pointer', padding: 0 }}
                  >
                    {track.locked ? <Lock size={14} /> : <Unlock size={14} />}
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Right Scrollable Timeline Body */}
        <div
          ref={timelineBodyRef}
          style={{ flex: 1, overflow: 'auto', position: 'relative', backgroundColor: '#0f0f11' }}
        >
          {/* Time Ruler */}
          <div
            onClick={handleRulerClick}
            style={{
              height: '28px',
              width: `${totalWidth}px`,
              borderBottom: '1px solid #27272a',
              backgroundColor: '#18181b',
              position: 'sticky',
              top: 0,
              zIndex: 10,
              cursor: 'pointer',
              userSelect: 'none',
            }}
          >
            {/* Draw second markers every 60 frames */}
            {Array.from({ length: Math.ceil(duration / fps) }).map((_, sec) => {
              const x = sec * fps * zoom;
              return (
                <div
                  key={sec}
                  style={{
                    position: 'absolute',
                    left: `${x}px`,
                    top: 0,
                    bottom: 0,
                    borderLeft: '1px solid #3f3f46',
                    paddingLeft: '4px',
                    fontSize: '10px',
                    color: '#71717a',
                  }}
                >
                  {sec}s
                </div>
              );
            })}
          </div>

          {/* Tracks Area */}
          <div style={{ position: 'relative', width: `${totalWidth}px` }}>
            {project.timeline.tracks.map((track) => (
              <div
                key={track.id}
                onDragOver={(e) => e.preventDefault()}
                onDrop={(e) => handleDropOnTrack(e, track)}
                style={{
                  height: `${track.uiHeight}px`,
                  borderBottom: '1px solid #27272a',
                  position: 'relative',
                  backgroundColor: '#111113',
                }}
              >
                {/* Clips on this track */}
                {track.clips.map((clip) => {
                  const left = clip.range.start * zoom;
                  const width = clip.range.duration * zoom;
                  const isSelected = selectedClipId === clip.id;

                  return (
                    <div
                      key={clip.id}
                      onClick={(e) => {
                        e.stopPropagation();
                        selectClip(clip.id);
                      }}
                      style={{
                        position: 'absolute',
                        left: `${left}px`,
                        width: `${width}px`,
                        top: '4px',
                        bottom: '4px',
                        backgroundColor: getClipColor(clip),
                        borderRadius: '4px',
                        padding: '4px 6px',
                        cursor: 'pointer',
                        boxShadow: isSelected ? '0 0 0 2px #ffffff' : 'none',
                        display: 'flex',
                        flexDirection: 'column',
                        justifyContent: 'center',
                        overflow: 'hidden',
                        whiteSpace: 'nowrap',
                        textOverflow: 'ellipsis',
                        color: '#fff',
                        fontWeight: 500,
                        fontSize: '11px',
                        border: '1px solid rgba(255,255,255,0.15)',
                      }}
                    >
                      <span>{clip.name || (clip instanceof SubtitleClip ? clip.plainText() : 'Clip')}</span>
                    </div>
                  );
                })}
              </div>
            ))}

            {/* Playhead vertical red line */}
            <div
              style={{
                position: 'absolute',
                top: 0,
                bottom: 0,
                left: `${currentFrame * zoom}px`,
                width: '2px',
                backgroundColor: '#ef4444',
                pointerEvents: 'none',
                zIndex: 20,
              }}
            >
              <div
                style={{
                  position: 'absolute',
                  top: '-28px',
                  left: '-6px',
                  width: 0,
                  height: 0,
                  borderLeft: '7px solid transparent',
                  borderRight: '7px solid transparent',
                  borderTop: '9px solid #ef4444',
                }}
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
