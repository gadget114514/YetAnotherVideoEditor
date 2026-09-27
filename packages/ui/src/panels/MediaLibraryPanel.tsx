import React, { useRef } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { VideoClip, AudioClip, ImageClip } from '@yave/core';
import { Folder, Film, Music, Image as ImageIcon, Upload, Plus } from 'lucide-react';

export const MediaLibraryPanel: React.FC = () => {
  const { project, addClipToTrack, revision, touch } = useProjectStore();
  const fileInputRef = useRef<HTMLInputElement>(null);

  const assets = project.assets.allIds().map((id) => project.assets.asset(id)!).filter(Boolean);

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (!files || files.length === 0) return;

    for (let i = 0; i < files.length; i++) {
      const file = files[i]!;
      const kind = file.type.startsWith('audio/')
        ? 'audio'
        : file.type.startsWith('image/')
          ? 'image'
          : 'video';

      const asset = project.assets.registerAsset(file.name, kind);
      asset.durationFrames = kind === 'image' ? 300 : 600;
    }
    touch();
  };

  const handleAddAssetToTimeline = (asset: any) => {
    const tl = project.timeline;
    if (asset.kind === 'audio') {
      const audioTracks = tl.tracksOfType('audio');
      const track = audioTracks[0] || tl.appendTrack('audio');
      const clip = new AudioClip(asset.id);
      clip.setName(asset.relativePath);
      clip.setRange({ start: track.contentDuration(), duration: asset.durationFrames || 300 });
      addClipToTrack(track.id, clip);
    } else if (asset.kind === 'image') {
      const videoTracks = tl.tracksOfType('video');
      const track = videoTracks[0] || tl.appendTrack('video');
      const clip = new ImageClip(asset.id);
      clip.setName(asset.relativePath);
      clip.setRange({ start: track.contentDuration(), duration: asset.durationFrames || 180 });
      addClipToTrack(track.id, clip);
    } else {
      const videoTracks = tl.tracksOfType('video');
      const track = videoTracks[0] || tl.appendTrack('video');
      const clip = new VideoClip(asset.id);
      clip.setName(asset.relativePath);
      clip.setRange({ start: track.contentDuration(), duration: asset.durationFrames || 300 });
      addClipToTrack(track.id, clip);
    }
  };

  const getIcon = (kind: string) => {
    switch (kind) {
      case 'audio':
        return <Music size={16} className="text-emerald-400" />;
      case 'image':
        return <ImageIcon size={16} className="text-sky-400" />;
      default:
        return <Film size={16} className="text-blue-400" />;
    }
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Toolbar */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
          <Folder size={16} className="text-amber-400" />
          <span>Media Library ({assets.length})</span>
        </div>
        <button
          onClick={() => fileInputRef.current?.click()}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            padding: '4px 10px',
            backgroundColor: '#2563eb',
            border: 'none',
            borderRadius: '4px',
            color: '#fff',
            cursor: 'pointer',
            fontSize: '11px',
            fontWeight: 500,
          }}
        >
          <Upload size={13} />
          <span>Import</span>
        </button>
        <input
          ref={fileInputRef}
          type="file"
          multiple
          accept="video/*,audio/*,image/*"
          style={{ display: 'none' }}
          onChange={handleFileChange}
        />
      </div>

      {/* Asset List */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
        {assets.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '40px 16px', color: '#71717a' }}>
            <p style={{ margin: '0 0 8px 0' }}>No media assets loaded.</p>
            <p style={{ fontSize: '11px', margin: 0 }}>Click Import or drag video/audio files here.</p>
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))', gap: '8px' }}>
            {assets.map((asset) => (
              <div
                key={asset.id}
                draggable
                onDragStart={(e) => {
                  e.dataTransfer.setData(
                    'application/json',
                    JSON.stringify({ type: 'asset', assetId: asset.id, kind: asset.kind, name: asset.relativePath }),
                  );
                }}
                style={{
                  backgroundColor: '#27272a',
                  borderRadius: '6px',
                  padding: '8px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '6px',
                  cursor: 'grab',
                  border: '1px solid #3f3f46',
                  transition: 'background-color 0.15s',
                }}
              >
                <div style={{ height: '54px', backgroundColor: '#18181b', borderRadius: '4px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                  {getIcon(asset.kind)}
                </div>
                <div style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', fontWeight: 500 }}>
                  {asset.relativePath}
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', color: '#a1a1aa', fontSize: '10px' }}>
                  <span style={{ textTransform: 'capitalize' }}>{asset.kind}</span>
                  <button
                    onClick={() => handleAddAssetToTimeline(asset)}
                    title="Add to timeline"
                    style={{
                      background: 'none',
                      border: 'none',
                      color: '#60a5fa',
                      cursor: 'pointer',
                      padding: '2px',
                    }}
                  >
                    <Plus size={14} />
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
