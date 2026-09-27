import React, { useEffect, useRef } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { usePlaybackStore } from '../store/usePlaybackStore.js';
import { SubtitleClip, framesToSeconds } from '@yave/core';
import { Play, Pause, SkipBack, SkipForward, Repeat } from 'lucide-react';

export const PreviewPanel: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const { project, revision } = useProjectStore();
  const {
    currentFrame,
    isPlaying,
    loop,
    fps,
    togglePlay,
    seek,
    stepForward,
    stepBackward,
    setLoop,
  } = usePlaybackStore();

  // Playback timer loop
  useEffect(() => {
    if (!isPlaying) return;

    const intervalMs = 1000 / fps;
    const timer = setInterval(() => {
      const maxDur = project.timeline.duration();
      const next = currentFrame + 1;
      if (next > maxDur) {
        if (loop) {
          seek(0);
        } else {
          usePlaybackStore.getState().pause();
        }
      } else {
        seek(next);
      }
    }, intervalMs);

    return () => clearInterval(timer);
  }, [isPlaying, currentFrame, fps, loop, project]);

  // Render preview canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;

    // Clear background
    ctx.fillStyle = '#09090b';
    ctx.fillRect(0, 0, width, height);

    // Build timeline snapshot for this frame
    const snapshot = project.timeline.buildSnapshot(currentFrame);

    // Render layers
    for (const layer of snapshot.layers) {
      ctx.save();
      ctx.globalAlpha = Math.max(0, Math.min(1, layer.opacity));

      // Draw clip placeholder / content box
      const clip = project.timeline.findClip(layer.clipId);
      if (clip) {
        if (clip instanceof SubtitleClip) {
          // Render subtitle text
          ctx.fillStyle = clip.styleOverride.fillColor || '#ffffff';
          const fontSize = Math.round((clip.styleOverride.fontPointSize || 48) * (height / 1080));
          ctx.font = `600 ${fontSize}px "Noto Sans JP", sans-serif`;
          ctx.textAlign = 'center';
          ctx.textBaseline = 'bottom';
          ctx.shadowColor = 'rgba(0,0,0,0.8)';
          ctx.shadowBlur = 4;

          const lines = clip.plainText().split('\n');
          const yStart = height * 0.9 - (lines.length - 1) * (fontSize * 1.2);
          for (let i = 0; i < lines.length; i++) {
            ctx.fillText(lines[i]!, width * 0.5, yStart + i * fontSize * 1.2);
          }
        } else {
          // Render video/color/title layer
          const grad = ctx.createLinearGradient(0, 0, width, height);
          grad.addColorStop(0, '#1e293b');
          grad.addColorStop(1, '#0f172a');
          ctx.fillStyle = grad;
          ctx.fillRect(0, 0, width, height);

          // Center badge
          ctx.fillStyle = '#94a3b8';
          ctx.font = '14px Inter, sans-serif';
          ctx.textAlign = 'center';
          ctx.fillText(`${clip.name || 'Video Layer'} (Frame ${currentFrame})`, width / 2, height / 2);
        }
      }

      ctx.restore();
    }
  }, [currentFrame, project, revision]);

  const formatTimecode = (f: number) => {
    const sTotal = Math.floor(f / fps);
    const ff = f % fps;
    const hh = Math.floor(sTotal / 3600);
    const mm = Math.floor((sTotal % 3600) / 60);
    const ss = sTotal % 60;
    const pad = (n: number) => String(n).padStart(2, '0');
    return `${pad(hh)}:${pad(mm)}:${pad(ss)}:${pad(ff)}`;
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#121214', color: '#e4e4e7' }}>
      {/* Canvas Viewport */}
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', overflow: 'hidden', padding: '12px' }}>
        <canvas
          ref={canvasRef}
          width={640}
          height={360}
          style={{
            maxWidth: '100%',
            maxHeight: '100%',
            aspectRatio: '16 / 9',
            backgroundColor: '#000000',
            boxShadow: '0 4px 20px rgba(0,0,0,0.6)',
            borderRadius: '4px',
          }}
        />
      </div>

      {/* Control Transport Bar */}
      <div style={{ height: '48px', backgroundColor: '#18181b', borderTop: '1px solid #27272a', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 16px' }}>
        <div style={{ fontFamily: 'monospace', fontSize: '13px', fontWeight: 600, color: '#38bdf8' }}>
          {formatTimecode(currentFrame)}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={() => stepBackward(1)}
            title="Step Back 1 Frame"
            style={{ background: 'none', border: 'none', color: '#a1a1aa', cursor: 'pointer', padding: '4px' }}
          >
            <SkipBack size={18} />
          </button>
          <button
            onClick={togglePlay}
            title={isPlaying ? 'Pause' : 'Play'}
            style={{
              width: '32px',
              height: '32px',
              borderRadius: '50%',
              backgroundColor: '#3b82f6',
              border: 'none',
              color: '#fff',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              cursor: 'pointer',
            }}
          >
            {isPlaying ? <Pause size={16} /> : <Play size={16} style={{ marginLeft: '2px' }} />}
          </button>
          <button
            onClick={() => stepForward(1)}
            title="Step Forward 1 Frame"
            style={{ background: 'none', border: 'none', color: '#a1a1aa', cursor: 'pointer', padding: '4px' }}
          >
            <SkipForward size={18} />
          </button>
          <button
            onClick={() => setLoop(!loop)}
            title={loop ? 'Loop Enabled' : 'Loop Disabled'}
            style={{
              background: 'none',
              border: 'none',
              color: loop ? '#38bdf8' : '#71717a',
              cursor: 'pointer',
              padding: '4px',
              marginLeft: '6px',
            }}
          >
            <Repeat size={16} />
          </button>
        </div>

        <div style={{ fontSize: '11px', color: '#71717a' }}>
          {project.canvasSize.width}x{project.canvasSize.height} @ {fps}fps
        </div>
      </div>
    </div>
  );
};
