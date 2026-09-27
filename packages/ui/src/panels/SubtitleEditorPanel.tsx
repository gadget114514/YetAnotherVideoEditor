import React, { useRef } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { usePlaybackStore } from '../store/usePlaybackStore.js';
import {
  SubtitleClip,
  ImportSubtitleCommand,
  EditSubtitleTextCommand,
  OverlapPolicy,
  framesToSeconds,
} from '@yave/core';
import { SrtParser, SrtWriter, convertCuesToClips } from '@yave/io';
import { FileText, Download, Upload, Plus, Trash2 } from 'lucide-react';

export const SubtitleEditorPanel: React.FC = () => {
  const { project, executeCommand, touch } = useProjectStore();
  const { currentFrame, fps, seek } = usePlaybackStore();
  const fileInputRef = useRef<HTMLInputElement>(null);

  const subTracks = project.timeline.tracksOfType('subtitle');
  const primaryTrack = subTracks[0];

  const subClips = primaryTrack
    ? (primaryTrack.clips.filter((c) => c instanceof SubtitleClip) as SubtitleClip[])
    : [];

  const handleImportSrt = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (ev) => {
      const text = ev.target?.result as string;
      if (!text) return;

      const parsed = SrtParser.parseText(text);
      if (!parsed.ok) {
        alert('Failed to parse SRT file: ' + parsed.warnings.join('\n'));
        return;
      }

      const clips = convertCuesToClips(parsed, project.timeline.timebase, 'default');
      executeCommand(
        new ImportSubtitleCommand(
          project,
          clips,
          OverlapPolicy.SplitToNewTracks,
          -1,
          'subtitle',
          file.name,
        ),
      );
    };
    reader.readAsText(file);
  };

  const handleExportSrt = () => {
    if (!primaryTrack) {
      alert('No subtitle track to export.');
      return;
    }

    const srtText = SrtWriter.writeToString(primaryTrack, project.timeline.timebase);
    const blob = new Blob([srtText], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${project.name || 'subtitles'}.srt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleAddAtPlayhead = () => {
    const tl = project.timeline;
    const track = primaryTrack || tl.appendTrack('subtitle');
    const sc = new SubtitleClip();
    sc.setRange({ start: currentFrame, duration: 180 });
    sc.setPlainText('新規字幕テキスト');
    track.insertClip(sc);
    touch();
  };

  const formatSec = (f: number) => {
    const s = framesToSeconds(f, project.timeline.timebase);
    const m = Math.floor(s / 60);
    const sec = Math.floor(s % 60);
    const ms = Math.floor((s % 1) * 100);
    const pad = (n: number) => String(n).padStart(2, '0');
    return `${pad(m)}:${pad(sec)}.${pad(ms)}`;
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Action Header */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
          <FileText size={16} className="text-amber-400" />
          <span>Subtitles ({subClips.length})</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <button
            onClick={handleAddAtPlayhead}
            title="Add Subtitle at Playhead"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: '#f59e0b',
              border: 'none',
              borderRadius: '4px',
              color: '#000',
              cursor: 'pointer',
              fontWeight: 600,
              fontSize: '11px',
            }}
          >
            <Plus size={13} />
            <span>Add</span>
          </button>
          <button
            onClick={() => fileInputRef.current?.click()}
            title="Import SRT File"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: '#27272a',
              border: '1px solid #3f3f46',
              borderRadius: '4px',
              color: '#e4e4e7',
              cursor: 'pointer',
              fontSize: '11px',
            }}
          >
            <Upload size={13} />
            <span>Import</span>
          </button>
          <button
            onClick={handleExportSrt}
            title="Export SRT File"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 8px',
              backgroundColor: '#27272a',
              border: '1px solid #3f3f46',
              borderRadius: '4px',
              color: '#e4e4e7',
              cursor: 'pointer',
              fontSize: '11px',
            }}
          >
            <Download size={13} />
            <span>Export</span>
          </button>
          <input
            ref={fileInputRef}
            type="file"
            accept=".srt,.vtt"
            style={{ display: 'none' }}
            onChange={handleImportSrt}
          />
        </div>
      </div>

      {/* Cues List */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {subClips.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '40px 16px', color: '#71717a' }}>
            <p>No subtitle cues on current track.</p>
            <p style={{ fontSize: '11px' }}>Click Add or Import SRT to start subtitling.</p>
          </div>
        ) : (
          subClips.map((sc, index) => (
            <div
              key={sc.id}
              onClick={() => seek(sc.range.start)}
              style={{
                backgroundColor: '#202024',
                border: '1px solid #2e2e34',
                borderRadius: '6px',
                padding: '8px 10px',
                display: 'flex',
                flexDirection: 'column',
                gap: '6px',
                cursor: 'pointer',
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '11px', fontWeight: 600, color: '#f59e0b' }}>
                  #{index + 1} ({formatSec(sc.range.start)} → {formatSec(sc.range.start + sc.range.duration)})
                </span>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    primaryTrack?.removeClip(sc.id);
                    touch();
                  }}
                  style={{ background: 'none', border: 'none', color: '#71717a', cursor: 'pointer', padding: '2px' }}
                >
                  <Trash2 size={13} />
                </button>
              </div>

              <textarea
                rows={2}
                value={sc.plainText()}
                onClick={(e) => e.stopPropagation()}
                onChange={(e) => executeCommand(new EditSubtitleTextCommand(project, sc.id, e.target.value))}
                style={{
                  width: '100%',
                  padding: '4px 6px',
                  backgroundColor: '#18181b',
                  border: '1px solid #3f3f46',
                  borderRadius: '4px',
                  color: '#fff',
                  boxSizing: 'border-box',
                  fontSize: '12px',
                  resize: 'vertical',
                }}
              />
            </div>
          ))
        )}
      </div>
    </div>
  );
};
