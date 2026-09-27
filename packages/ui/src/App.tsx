import React, { useEffect, useState } from 'react';
import type { PlatformHost } from '@yave/platform';

export interface AppProps {
  readonly host: PlatformHost;
}

export const App: React.FC<AppProps> = ({ host }) => {
  const [crossOriginIsolated, setCrossOriginIsolated] = useState<boolean>(false);

  useEffect(() => {
    setCrossOriginIsolated(typeof window !== 'undefined' && window.crossOriginIsolated);
  }, []);

  return (
    <div style={{
      width: '100vw',
      height: '100vh',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: '#1e1e1e',
      color: '#e0e0e0',
      fontFamily: 'Inter, system-ui, -apple-system, sans-serif',
      userSelect: 'none',
      overflow: 'hidden'
    }}>
      {/* Top Navigation Bar */}
      <header style={{
        height: '40px',
        backgroundColor: '#252526',
        borderBottom: '1px solid #333333',
        display: 'flex',
        alignItems: 'center',
        padding: '0 16px',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ fontWeight: 700, fontSize: '14px', letterSpacing: '0.5px', color: '#61afef' }}>
            YAVE
          </span>
          <span style={{ fontSize: '12px', color: '#888' }}>Yet Another Video Editor</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{
            padding: '2px 8px',
            borderRadius: '4px',
            backgroundColor: host.kind === 'electron' ? '#2d4f2d' : '#2b3b55',
            color: host.kind === 'electron' ? '#8ae28a' : '#8ab4f8'
          }}>
            {host.kind.toUpperCase()}
          </span>
          <span style={{
            padding: '2px 8px',
            borderRadius: '4px',
            backgroundColor: crossOriginIsolated ? '#1d4832' : '#523419',
            color: crossOriginIsolated ? '#6ee7b7' : '#fcd34d'
          }}>
            {crossOriginIsolated ? 'Isolated (SAB ready)' : 'Standard (Non-isolated)'}
          </span>
        </div>
      </header>

      {/* Main Workspace Area */}
      <main style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '24px',
        textAlign: 'center'
      }}>
        <div style={{
          maxWidth: '600px',
          padding: '32px',
          backgroundColor: '#282c34',
          borderRadius: '8px',
          boxShadow: '0 8px 24px rgba(0,0,0,0.4)',
          border: '1px solid #3e4451'
        }}>
          <h1 style={{ fontSize: '20px', fontWeight: 600, margin: '0 0 12px 0', color: '#ffffff' }}>
            YAVE 2.0 Web Architecture (M0)
          </h1>
          <p style={{ fontSize: '13px', lineHeight: '1.6', color: '#abb2bf', margin: '0 0 20px 0' }}>
            TypeScript + React + WebCodecs + WebGL2 + Web Audio API モノレポ環境が正常に初期化されました。
          </p>
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(2, 1fr)',
            gap: '12px',
            fontSize: '12px',
            textAlign: 'left'
          }}>
            <div style={{ padding: '8px 12px', backgroundColor: '#21252b', borderRadius: '4px' }}>
              <strong>Platform Kind:</strong> {host.kind}
            </div>
            <div style={{ padding: '8px 12px', backgroundColor: '#21252b', borderRadius: '4px' }}>
              <strong>Native FS:</strong> {host.capabilities.nativeFileSystem ? 'Yes' : 'No'}
            </div>
            <div style={{ padding: '8px 12px', backgroundColor: '#21252b', borderRadius: '4px' }}>
              <strong>Native FFmpeg:</strong> {host.capabilities.nativeFfmpeg ? 'Yes' : 'No'}
            </div>
            <div style={{ padding: '8px 12px', backgroundColor: '#21252b', borderRadius: '4px' }}>
              <strong>CORS-Free Fetch:</strong> {host.capabilities.corsFreeFetch ? 'Yes' : 'No'}
            </div>
          </div>
        </div>
      </main>

      {/* Status Bar */}
      <footer style={{
        height: '24px',
        backgroundColor: '#007acc',
        color: '#ffffff',
        fontSize: '11px',
        display: 'flex',
        alignItems: 'center',
        padding: '0 12px',
        justifyContent: 'space-between'
      }}>
        <span>Ready</span>
        <span>Milestone M0: Repository Scaffolding</span>
      </footer>
    </div>
  );
};
