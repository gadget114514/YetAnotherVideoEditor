import React, { useEffect, useState, useRef } from 'react';
import type { PlatformHost } from '@yave/platform';
import { Layout, Model, type TabNode, type IJsonModel } from 'flexlayout-react';
import 'flexlayout-react/style/dark.css';

import { useProjectStore } from './store/useProjectStore.js';
import { usePlaybackStore } from './store/usePlaybackStore.js';

import { MediaLibraryPanel } from './panels/MediaLibraryPanel.js';
import { EffectLibraryPanel } from './panels/EffectLibraryPanel.js';
import { PreviewPanel } from './panels/PreviewPanel.js';
import { TimelinePanel } from './panels/TimelinePanel.js';
import { InspectorPanel } from './panels/InspectorPanel.js';
import { SubtitleEditorPanel } from './panels/SubtitleEditorPanel.js';
import { AudioMixerPanel } from './panels/AudioMixerPanel.js';
import { AiPanel } from './panels/AiPanel.js';
import { ExportModal } from './dialogs/ExportModal.js';

import {
  Undo,
  Redo,
  FolderOpen,
  Save,
  FilePlus,
  Play,
  Film,
  Download,
} from 'lucide-react';

export interface AppProps {
  readonly host: PlatformHost;
}

const defaultLayout: IJsonModel = {
  global: {
    tabEnableClose: false,
    tabSetEnableMaximize: true,
  },
  borders: [],
  layout: {
    type: 'row',
    weight: 100,
    children: [
      {
        type: 'column',
        weight: 22,
        children: [
          {
            type: 'tabset',
            weight: 55,
            children: [
              { type: 'tab', name: 'Media Library', component: 'media-library' },
              { type: 'tab', name: 'Effects', component: 'effect-library' },
            ],
          },
          {
            type: 'tabset',
            weight: 45,
            children: [
              { type: 'tab', name: 'Audio Mixer', component: 'audio-mixer' },
              { type: 'tab', name: 'AI Panel', component: 'ai-panel' },
            ],
          },
        ],
      },
      {
        type: 'column',
        weight: 54,
        children: [
          {
            type: 'tabset',
            weight: 58,
            children: [
              { type: 'tab', name: 'Preview Monitor', component: 'preview' },
            ],
          },
          {
            type: 'tabset',
            weight: 42,
            children: [
              { type: 'tab', name: 'Timeline', component: 'timeline' },
            ],
          },
        ],
      },
      {
        type: 'column',
        weight: 24,
        children: [
          {
            type: 'tabset',
            weight: 55,
            children: [
              { type: 'tab', name: 'Inspector', component: 'inspector' },
            ],
          },
          {
            type: 'tabset',
            weight: 45,
            children: [
              { type: 'tab', name: 'Subtitles', component: 'subtitle-editor' },
            ],
          },
        ],
      },
    ],
  },
};

export const App: React.FC<AppProps> = ({ host }) => {
  const [model] = useState<Model>(() => Model.fromJson(defaultLayout));
  const [crossOriginIsolated, setCrossOriginIsolated] = useState<boolean>(false);
  const [isExportOpen, setIsExportOpen] = useState<boolean>(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const {
    project,
    undo,
    redo,
    canUndo,
    canRedo,
    saveProject,
    loadProject,
    initDefaultProject,
    revision,
  } = useProjectStore();

  const { togglePlay, isPlaying } = usePlaybackStore();

  useEffect(() => {
    setCrossOriginIsolated(typeof window !== 'undefined' && window.crossOriginIsolated);
  }, []);

  // Global Keyboard Shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Don't intercept if user is typing in an input or textarea
      const target = e.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA') {
        return;
      }

      if (e.key === ' ' || e.code === 'Space') {
        e.preventDefault();
        togglePlay();
      } else if ((e.ctrlKey || e.metaKey) && e.key === 'z') {
        e.preventDefault();
        if (e.shiftKey) {
          redo();
        } else {
          undo();
        }
      } else if ((e.ctrlKey || e.metaKey) && e.key === 'y') {
        e.preventDefault();
        redo();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [togglePlay, undo, redo]);

  const handleOpenProject = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (ev) => {
      const json = ev.target?.result as string;
      if (!json) return;
      const res = loadProject(json);
      if (!res.ok) {
        alert('Failed to load project: ' + res.errorMessage);
      }
    };
    reader.readAsText(file);
  };

  const handleSaveProject = () => {
    const json = saveProject({ indented: true });
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${project.name || 'project'}.yave`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const factory = (node: TabNode) => {
    const component = node.getComponent();
    switch (component) {
      case 'media-library':
        return <MediaLibraryPanel />;
      case 'effect-library':
        return <EffectLibraryPanel />;
      case 'preview':
        return <PreviewPanel />;
      case 'timeline':
        return <TimelinePanel />;
      case 'inspector':
        return <InspectorPanel />;
      case 'subtitle-editor':
        return <SubtitleEditorPanel />;
      case 'audio-mixer':
        return <AudioMixerPanel />;
      case 'ai-panel':
        return <AiPanel />;
      default:
        return <div>Panel: {component}</div>;
    }
  };

  return (
    <div
      style={{
        width: '100vw',
        height: '100vh',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: '#121214',
        color: '#e4e4e7',
        fontFamily: 'Inter, system-ui, -apple-system, sans-serif',
        userSelect: 'none',
        overflow: 'hidden',
      }}
    >
      {/* Top Application Header & Toolbar */}
      <header
        style={{
          height: '42px',
          backgroundColor: '#18181b',
          borderBottom: '1px solid #27272a',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '0 12px',
        }}
      >
        {/* Brand & Menu */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div
              style={{
                width: '26px',
                height: '26px',
                borderRadius: '6px',
                background: 'linear-gradient(135deg, #3b82f6, #8b5cf6)',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: '#fff',
              }}
            >
              <Film size={15} />
            </div>
            <span style={{ fontWeight: 700, fontSize: '14px', letterSpacing: '0.5px', color: '#f4f4f5' }}>
              YAVE
            </span>
          </div>

          {/* Quick Actions */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <button
              onClick={initDefaultProject}
              title="New Project"
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                background: 'none',
                border: 'none',
                color: '#a1a1aa',
                cursor: 'pointer',
                padding: '4px 8px',
                fontSize: '12px',
                borderRadius: '4px',
              }}
            >
              <FilePlus size={14} />
              <span>New</span>
            </button>
            <button
              onClick={() => fileInputRef.current?.click()}
              title="Open Project (.yave)"
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                background: 'none',
                border: 'none',
                color: '#a1a1aa',
                cursor: 'pointer',
                padding: '4px 8px',
                fontSize: '12px',
                borderRadius: '4px',
              }}
            >
              <FolderOpen size={14} />
              <span>Open</span>
            </button>
            <input
              ref={fileInputRef}
              type="file"
              accept=".yave,.json"
              style={{ display: 'none' }}
              onChange={handleOpenProject}
            />
            <button
              onClick={handleSaveProject}
              title="Save Project"
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                background: 'none',
                border: 'none',
                color: '#a1a1aa',
                cursor: 'pointer',
                padding: '4px 8px',
                fontSize: '12px',
                borderRadius: '4px',
              }}
            >
              <Save size={14} />
              <span>Save</span>
            </button>

            <button
              onClick={() => setIsExportOpen(true)}
              title="Export Timeline to MP4"
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                backgroundColor: '#0284c7',
                border: 'none',
                color: '#ffffff',
                cursor: 'pointer',
                padding: '4px 10px',
                fontSize: '12px',
                borderRadius: '4px',
                fontWeight: 600,
              }}
            >
              <Download size={14} />
              <span>Export</span>
            </button>

            <div style={{ width: '1px', height: '16px', backgroundColor: '#27272a', margin: '0 4px' }} />

            <button
              onClick={undo}
              disabled={!canUndo()}
              title="Undo (Ctrl+Z)"
              style={{
                background: 'none',
                border: 'none',
                color: canUndo() ? '#e4e4e7' : '#52525b',
                cursor: canUndo() ? 'pointer' : 'default',
                padding: '4px',
              }}
            >
              <Undo size={15} />
            </button>
            <button
              onClick={redo}
              disabled={!canRedo()}
              title="Redo (Ctrl+Shift+Z)"
              style={{
                background: 'none',
                border: 'none',
                color: canRedo() ? '#e4e4e7' : '#52525b',
                cursor: canRedo() ? 'pointer' : 'default',
                padding: '4px',
              }}
            >
              <Redo size={15} />
            </button>
          </div>
        </div>

        {/* Center Project Name Badge */}
        <div style={{ fontSize: '12px', fontWeight: 500, color: '#a1a1aa' }}>
          {project.name}
        </div>

        {/* Right Badges */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span
            style={{
              padding: '2px 8px',
              borderRadius: '4px',
              backgroundColor: host.kind === 'electron' ? '#14532d' : '#1e3a8a',
              color: host.kind === 'electron' ? '#86efac' : '#93c5fd',
              fontWeight: 600,
            }}
          >
            {host.kind.toUpperCase()}
          </span>
          <span
            style={{
              padding: '2px 8px',
              borderRadius: '4px',
              backgroundColor: crossOriginIsolated ? '#064e3b' : '#78350f',
              color: crossOriginIsolated ? '#6ee7b7' : '#fde047',
            }}
          >
            {crossOriginIsolated ? 'Isolated (SAB ready)' : 'Standard'}
          </span>
        </div>
      </header>

      {/* Main FlexLayout Dock Workspace */}
      <div style={{ flex: 1, position: 'relative' }}>
        <Layout model={model} factory={factory} />
      </div>

      {/* Status Bar */}
      <footer
        style={{
          height: '24px',
          backgroundColor: '#18181b',
          borderTop: '1px solid #27272a',
          color: '#71717a',
          fontSize: '11px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '0 12px',
        }}
      >
        <span>Tracks: {project.timeline.trackCount} | Revisions: {revision}</span>
        <span>YAVE 2.0 Web Architecture</span>
      </footer>

      {/* Export Modal */}
      <ExportModal isOpen={isExportOpen} onClose={() => setIsExportOpen(false)} />
    </div>
  );
};
