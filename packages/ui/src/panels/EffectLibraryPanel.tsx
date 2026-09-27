import React, { useState } from 'react';
import {
  builtinFilter,
  builtinTransition,
  builtinTitle,
  VideoClip,
  TitleClip,
  type VideoFilterInstance,
} from '@yave/core';
import { useProjectStore } from '../store/useProjectStore.js';
import { Wand2, Layers, MoveRight, Type } from 'lucide-react';

export const EffectLibraryPanel: React.FC = () => {
  const [tab, setTab] = useState<'filters' | 'transitions' | 'titles'>('filters');
  const { project, executeCommand, touch } = useProjectStore();

  const filterList = [
    { id: builtinFilter.kColorAdjust, name: 'Color Adjust', desc: 'Brightness, contrast, saturation, gamma' },
    { id: builtinFilter.kBlur, name: 'Gaussian Blur', desc: 'Smooth spatial blur with direction' },
    { id: builtinFilter.kMono, name: 'Monochrome', desc: 'Black and white conversion' },
    { id: builtinFilter.kSepia, name: 'Sepia Tone', desc: 'Warm nostalgic vintage look' },
  ];

  const transitionList = [
    { id: builtinTransition.kDissolve, name: 'Cross Dissolve', desc: 'Standard smooth cross-fade' },
    { id: builtinTransition.kFadeToBlack, name: 'Fade to Color', desc: 'Dip through black or selected color' },
    { id: builtinTransition.kWipe, name: 'Linear Wipe', desc: 'Soft directional linear wipe' },
    { id: builtinTransition.kSlide, name: 'Slide', desc: 'Incoming clip slides over outgoing' },
    { id: builtinTransition.kPush, name: 'Push', desc: 'Incoming clip pushes outgoing clip' },
  ];

  const titleList = [
    { id: builtinTitle.kCenter, name: 'Center Title', desc: 'Bold centered opening title text' },
    { id: builtinTitle.kLowerThird, name: 'Lower Third', desc: 'News broadcast style speaker banner' },
    { id: builtinTitle.kCredits, name: 'Rolling Credits', desc: 'Closing credits text layout' },
    { id: builtinTitle.kSubtitleCaption, name: 'Subtitle Caption', desc: 'High-visibility dialog caption' },
  ];

  const handleAddTitle = (presetId: string) => {
    const tl = project.timeline;
    const track = tl.tracksOfType('subtitle')[0] || tl.appendTrack('subtitle');
    const title = new TitleClip();
    title.applyPreset(presetId);
    title.setRange({ start: track.contentDuration(), duration: 180 });
    track.insertClip(title);
    touch();
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      {/* Category Tabs */}
      <div style={{ display: 'flex', borderBottom: '1px solid #27272a', backgroundColor: '#202023' }}>
        <button
          onClick={() => setTab('filters')}
          style={{
            flex: 1,
            padding: '8px 4px',
            background: tab === 'filters' ? '#27272a' : 'none',
            border: 'none',
            borderBottom: tab === 'filters' ? '2px solid #3b82f6' : '2px solid transparent',
            color: tab === 'filters' ? '#fff' : '#a1a1aa',
            cursor: 'pointer',
            fontWeight: 500,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            fontSize: '11px',
          }}
        >
          <Wand2 size={13} />
          <span>Filters</span>
        </button>
        <button
          onClick={() => setTab('transitions')}
          style={{
            flex: 1,
            padding: '8px 4px',
            background: tab === 'transitions' ? '#27272a' : 'none',
            border: 'none',
            borderBottom: tab === 'transitions' ? '2px solid #3b82f6' : '2px solid transparent',
            color: tab === 'transitions' ? '#fff' : '#a1a1aa',
            cursor: 'pointer',
            fontWeight: 500,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            fontSize: '11px',
          }}
        >
          <MoveRight size={13} />
          <span>Transitions</span>
        </button>
        <button
          onClick={() => setTab('titles')}
          style={{
            flex: 1,
            padding: '8px 4px',
            background: tab === 'titles' ? '#27272a' : 'none',
            border: 'none',
            borderBottom: tab === 'titles' ? '2px solid #3b82f6' : '2px solid transparent',
            color: tab === 'titles' ? '#fff' : '#a1a1aa',
            cursor: 'pointer',
            fontWeight: 500,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            fontSize: '11px',
          }}
        >
          <Type size={13} />
          <span>Titles</span>
        </button>
      </div>

      {/* Item List */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '10px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
        {tab === 'filters' &&
          filterList.map((f) => (
            <div
              key={f.id}
              draggable
              onDragStart={(e) => {
                e.dataTransfer.setData('application/json', JSON.stringify({ type: 'filter', filterId: f.id }));
              }}
              style={{
                padding: '10px 12px',
                backgroundColor: '#27272a',
                border: '1px solid #3f3f46',
                borderRadius: '6px',
                cursor: 'grab',
              }}
            >
              <div style={{ fontWeight: 600, color: '#93c5fd', marginBottom: '2px' }}>{f.name}</div>
              <div style={{ fontSize: '11px', color: '#a1a1aa' }}>{f.desc}</div>
            </div>
          ))}

        {tab === 'transitions' &&
          transitionList.map((t) => (
            <div
              key={t.id}
              draggable
              onDragStart={(e) => {
                e.dataTransfer.setData('application/json', JSON.stringify({ type: 'transition', transitionId: t.id }));
              }}
              style={{
                padding: '10px 12px',
                backgroundColor: '#27272a',
                border: '1px solid #3f3f46',
                borderRadius: '6px',
                cursor: 'grab',
              }}
            >
              <div style={{ fontWeight: 600, color: '#fca5a5', marginBottom: '2px' }}>{t.name}</div>
              <div style={{ fontSize: '11px', color: '#a1a1aa' }}>{t.desc}</div>
            </div>
          ))}

        {tab === 'titles' &&
          titleList.map((t) => (
            <div
              key={t.id}
              onClick={() => handleAddTitle(t.id)}
              style={{
                padding: '10px 12px',
                backgroundColor: '#27272a',
                border: '1px solid #3f3f46',
                borderRadius: '6px',
                cursor: 'pointer',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}
            >
              <div>
                <div style={{ fontWeight: 600, color: '#d8b4fe', marginBottom: '2px' }}>{t.name}</div>
                <div style={{ fontSize: '11px', color: '#a1a1aa' }}>{t.desc}</div>
              </div>
              <span style={{ fontSize: '10px', color: '#a855f7', border: '1px solid #9333ea', borderRadius: '4px', padding: '2px 6px' }}>
                Add
              </span>
            </div>
          ))}
      </div>
    </div>
  );
};
