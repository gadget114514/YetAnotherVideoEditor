import React from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { useSelectionStore } from '../store/useSelectionStore.js';
import {
  ClipProperty,
  TrackProperty,
  BlendMode,
  AudioClip,
  VideoClip,
  SubtitleClip,
  TitleClip,
  SubtitleStyleField,
  SetSubtitleStyleCommand,
  EditSubtitleTextCommand,
} from '@yave/core';
import { Sliders, Settings2, Trash2 } from 'lucide-react';

export const InspectorPanel: React.FC = () => {
  const { project, setClipProperty, setTrackProperty, executeCommand, touch } =
    useProjectStore();
  const { selectedClipId, selectedTrackId } = useSelectionStore();

  const selectedClip = selectedClipId ? project.timeline.findClip(selectedClipId) : null;
  const selectedTrack = selectedTrackId ? project.timeline.trackById(selectedTrackId) : null;

  const blendModes = Object.values(BlendMode);

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
        <Sliders size={16} className="text-sky-400" />
        <span>Inspector</span>
      </div>

      <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {/* Clip Inspector */}
        {selectedClip ? (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ fontWeight: 600, color: '#60a5fa', textTransform: 'capitalize' }}>
              {selectedClip.type} Clip Properties
            </div>

            {/* Name */}
            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Name</label>
              <input
                type="text"
                value={selectedClip.name}
                onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Name, e.target.value)}
                style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
              />
            </div>

            {/* Timing */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div>
                <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Start (Frames)</label>
                <input
                  type="number"
                  value={selectedClip.range.start}
                  onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Start, parseInt(e.target.value, 10) || 0)}
                  style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
                />
              </div>
              <div>
                <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Duration</label>
                <input
                  type="number"
                  value={selectedClip.range.duration}
                  onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Duration, parseInt(e.target.value, 10) || 1)}
                  style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
                />
              </div>
            </div>

            {/* Opacity */}
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                <label style={{ color: '#a1a1aa', fontSize: '11px' }}>Opacity</label>
                <span style={{ fontSize: '11px' }}>{Math.round(selectedClip.opacity * 100)}%</span>
              </div>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value={selectedClip.opacity}
                onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Opacity, parseFloat(e.target.value))}
                style={{ width: '100%' }}
              />
            </div>

            {/* Blend Mode */}
            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Blend Mode</label>
              <select
                value={selectedClip.blendMode}
                onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.BlendMode, e.target.value as BlendMode)}
                style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
              >
                {blendModes.map((mode) => (
                  <option key={mode} value={mode}>
                    {mode}
                  </option>
                ))}
              </select>
            </div>

            {/* Audio Specific Controls */}
            {selectedClip instanceof AudioClip && (
              <>
                <div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <label style={{ color: '#a1a1aa', fontSize: '11px' }}>Gain</label>
                    <span style={{ fontSize: '11px' }}>{selectedClip.gain.toFixed(2)}x</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="2"
                    step="0.05"
                    value={selectedClip.gain}
                    onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Gain, parseFloat(e.target.value))}
                    style={{ width: '100%' }}
                  />
                </div>
                <div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                    <label style={{ color: '#a1a1aa', fontSize: '11px' }}>Pan</label>
                    <span style={{ fontSize: '11px' }}>{selectedClip.pan.toFixed(2)}</span>
                  </div>
                  <input
                    type="range"
                    min="-1"
                    max="1"
                    step="0.05"
                    value={selectedClip.pan}
                    onChange={(e) => setClipProperty(selectedClip.id, ClipProperty.Pan, parseFloat(e.target.value))}
                    style={{ width: '100%' }}
                  />
                </div>
              </>
            )}

            {/* Subtitle Specific Controls */}
            {selectedClip instanceof SubtitleClip && (
              <div style={{ borderTop: '1px solid #3f3f46', paddingTop: '10px', marginTop: '6px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <label style={{ color: '#f59e0b', fontSize: '11px', fontWeight: 600 }}>Subtitle Text</label>
                <textarea
                  rows={3}
                  value={selectedClip.plainText()}
                  onChange={(e) => executeCommand(new EditSubtitleTextCommand(project, selectedClip.id, e.target.value))}
                  style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box', resize: 'vertical' }}
                />

                <div>
                  <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Font Size (pt)</label>
                  <input
                    type="number"
                    value={selectedClip.styleOverride.fontPointSize || 48}
                    onChange={(e) => executeCommand(new SetSubtitleStyleCommand(project, selectedClip.id, SubtitleStyleField.FontPointSize, parseFloat(e.target.value) || 24))}
                    style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
                  />
                </div>

                <div>
                  <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Text Color</label>
                  <input
                    type="color"
                    value={selectedClip.styleOverride.fillColor || '#ffffff'}
                    onChange={(e) => executeCommand(new SetSubtitleStyleCommand(project, selectedClip.id, SubtitleStyleField.FillColor, e.target.value))}
                    style={{ width: '100%', height: '32px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', cursor: 'pointer' }}
                  />
                </div>
              </div>
            )}

            {/* Filters Stack */}
            {selectedClip.filters.length > 0 && (
              <div style={{ borderTop: '1px solid #3f3f46', paddingTop: '10px', marginTop: '6px' }}>
                <label style={{ color: '#93c5fd', fontSize: '11px', fontWeight: 600, display: 'block', marginBottom: '6px' }}>Active Filters</label>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                  {selectedClip.filters.map((f, idx) => (
                    <div
                      key={idx}
                      style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '6px 8px', backgroundColor: '#27272a', borderRadius: '4px', border: '1px solid #3f3f46' }}
                    >
                      <span>{f.filterId}</span>
                      <button
                        onClick={() => {
                          selectedClip.removeFilter(idx);
                          touch();
                        }}
                        style={{ background: 'none', border: 'none', color: '#ef4444', cursor: 'pointer' }}
                      >
                        <Trash2 size={13} />
                      </button>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        ) : selectedTrack ? (
          /* Track Inspector */
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ fontWeight: 600, color: '#34d399', textTransform: 'capitalize' }}>
              Track: {selectedTrack.name}
            </div>

            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Track Name</label>
              <input
                type="text"
                value={selectedTrack.name}
                onChange={(e) => setTrackProperty(selectedTrack.id, TrackProperty.Name, e.target.value)}
                style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
              />
            </div>

            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                <label style={{ color: '#a1a1aa', fontSize: '11px' }}>Track Gain</label>
                <span style={{ fontSize: '11px' }}>{selectedTrack.gain.toFixed(2)}x</span>
              </div>
              <input
                type="range"
                min="0"
                max="2"
                step="0.05"
                value={selectedTrack.gain}
                onChange={(e) => setTrackProperty(selectedTrack.id, TrackProperty.Gain, parseFloat(e.target.value))}
                style={{ width: '100%' }}
              />
            </div>

            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                <label style={{ color: '#a1a1aa', fontSize: '11px' }}>Pan</label>
                <span style={{ fontSize: '11px' }}>{selectedTrack.pan.toFixed(2)}</span>
              </div>
              <input
                type="range"
                min="-1"
                max="1"
                step="0.05"
                value={selectedTrack.pan}
                onChange={(e) => setTrackProperty(selectedTrack.id, TrackProperty.Pan, parseFloat(e.target.value))}
                style={{ width: '100%' }}
              />
            </div>
          </div>
        ) : (
          /* Project Overview */
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', color: '#a1a1aa' }}>
            <div style={{ fontWeight: 600, color: '#e4e4e7', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Settings2 size={16} />
              <span>Project Settings</span>
            </div>
            <div>
              <label style={{ display: 'block', fontSize: '11px', marginBottom: '4px' }}>Project Name</label>
              <input
                type="text"
                value={project.name}
                onChange={(e) => {
                  project.setName(e.target.value);
                  touch();
                }}
                style={{ width: '100%', padding: '6px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#fff', boxSizing: 'border-box' }}
              />
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div style={{ padding: '8px', backgroundColor: '#27272a', borderRadius: '4px' }}>
                <div style={{ fontSize: '10px' }}>Resolution</div>
                <div style={{ fontWeight: 600, color: '#fff', marginTop: '2px' }}>
                  {project.canvasSize.width} x {project.canvasSize.height}
                </div>
              </div>
              <div style={{ padding: '8px', backgroundColor: '#27272a', borderRadius: '4px' }}>
                <div style={{ fontSize: '10px' }}>Frame Rate</div>
                <div style={{ fontWeight: 600, color: '#fff', marginTop: '2px' }}>
                  {(project.timebase.den / project.timebase.num).toFixed(2)} fps
                </div>
              </div>
            </div>
            <p style={{ fontSize: '11px', lineHeight: '1.5', marginTop: '12px', color: '#71717a' }}>
              Select a clip or track in the timeline to inspect and edit its parameters in real time.
            </p>
          </div>
        )}
      </div>
    </div>
  );
};
