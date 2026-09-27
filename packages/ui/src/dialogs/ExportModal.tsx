import React, { useState } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { ExportJob, type ExportResult } from '@yave/engine';
import { Download, X, Film, CheckCircle2, AlertCircle } from 'lucide-react';

export interface ExportModalProps {
  isOpen: boolean;
  onClose: () => void;
}

export const ExportModal: React.FC<ExportModalProps> = ({ isOpen, onClose }) => {
  const { project } = useProjectStore();

  const [resolution, setResolution] = useState<'1080p' | '720p' | '4k'>('1080p');
  const [fps, setFps] = useState<number>(60);
  const [bitrateMbps, setBitrateMbps] = useState<number>(8);
  const [isExporting, setIsExporting] = useState<boolean>(false);
  const [progress, setProgress] = useState<number>(0);
  const [statusMessage, setStatusMessage] = useState<string>('');
  const [exportResult, setExportResult] = useState<ExportResult | null>(null);
  const [abortController, setAbortController] = useState<AbortController | null>(null);

  if (!isOpen) return null;

  const handleStartExport = async () => {
    setIsExporting(true);
    setProgress(0);
    setExportResult(null);
    setStatusMessage('Rendering video...');

    const controller = new AbortController();
    setAbortController(controller);

    let width = 1920;
    let height = 1080;
    if (resolution === '720p') {
      width = 1280;
      height = 720;
    } else if (resolution === '4k') {
      width = 3840;
      height = 2160;
    }

    const job = new ExportJob(project, {
      width,
      height,
      fps,
      videoBitrate: bitrateMbps * 1_000_000,
      signal: controller.signal,
      onProgress: (p, frame, total) => {
        setProgress(p);
        setStatusMessage(`Rendering frame ${frame} / ${total} (${Math.round(p * 100)}%)`);
      },
    });

    try {
      const res = await job.run();
      setExportResult(res);
      setIsExporting(false);

      if (res.ok && res.blob) {
        setStatusMessage('Export completed!');
        // Trigger browser download
        const url = URL.createObjectURL(res.blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${project.name || 'yave-export'}.mp4`;
        a.click();
        URL.revokeObjectURL(url);
      } else if (!res.ok) {
        setStatusMessage(res.error || 'Export failed');
      }
    } catch (err: any) {
      setIsExporting(false);
      setStatusMessage(err?.message || 'Export error');
    }
  };

  const handleCancel = () => {
    if (isExporting && abortController) {
      abortController.abort();
    }
    onClose();
  };

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.75)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
        backdropFilter: 'blur(4px)',
      }}
    >
      <div
        style={{
          width: '480px',
          backgroundColor: '#18181b',
          borderRadius: '12px',
          border: '1px solid #27272a',
          boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.5)',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        {/* Modal Header */}
        <div
          style={{
            padding: '16px 20px',
            borderBottom: '1px solid #27272a',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontWeight: 600, fontSize: '15px' }}>
            <Film size={18} className="text-sky-400" />
            <span>Export Timeline to MP4</span>
          </div>
          <button
            onClick={handleCancel}
            style={{
              background: 'none',
              border: 'none',
              color: '#71717a',
              cursor: 'pointer',
              padding: '4px',
            }}
          >
            <X size={18} />
          </button>
        </div>

        {/* Modal Body */}
        <div style={{ padding: '20px', display: 'flex', flexDirection: 'column', gap: '16px', fontSize: '13px' }}>
          {/* Resolution Selector */}
          <div>
            <label style={{ display: 'block', color: '#a1a1aa', marginBottom: '6px', fontWeight: 500 }}>
              Resolution
            </label>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
              {(['720p', '1080p', '4k'] as const).map((res) => (
                <button
                  key={res}
                  disabled={isExporting}
                  onClick={() => setResolution(res)}
                  style={{
                    padding: '8px',
                    borderRadius: '6px',
                    border: resolution === res ? '1px solid #38bdf8' : '1px solid #27272a',
                    backgroundColor: resolution === res ? '#0c4a6e' : '#27272a',
                    color: resolution === res ? '#f0f9ff' : '#d4d4d8',
                    cursor: isExporting ? 'default' : 'pointer',
                    fontWeight: resolution === res ? 600 : 400,
                  }}
                >
                  {res.toUpperCase()}
                </button>
              ))}
            </div>
          </div>

          {/* Framerate */}
          <div>
            <label style={{ display: 'block', color: '#a1a1aa', marginBottom: '6px', fontWeight: 500 }}>
              Framerate (FPS)
            </label>
            <select
              disabled={isExporting}
              value={fps}
              onChange={(e) => setFps(Number(e.target.value))}
              style={{
                width: '100%',
                backgroundColor: '#27272a',
                border: '1px solid #3f3f46',
                color: '#fff',
                padding: '8px',
                borderRadius: '6px',
              }}
            >
              <option value={24}>24 fps (Cinema)</option>
              <option value={30}>30 fps (Web / Video)</option>
              <option value={60}>60 fps (Smooth / Gaming)</option>
            </select>
          </div>

          {/* Video Bitrate */}
          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', color: '#a1a1aa', marginBottom: '6px' }}>
              <span>Video Bitrate</span>
              <span style={{ color: '#fff' }}>{bitrateMbps} Mbps</span>
            </div>
            <input
              type="range"
              disabled={isExporting}
              min={2}
              max={25}
              value={bitrateMbps}
              onChange={(e) => setBitrateMbps(Number(e.target.value))}
              style={{ width: '100%' }}
            />
          </div>

          {/* Progress / Status Section */}
          {isExporting && (
            <div style={{ marginTop: '8px' }}>
              <div
                style={{
                  height: '8px',
                  backgroundColor: '#27272a',
                  borderRadius: '4px',
                  overflow: 'hidden',
                  marginBottom: '8px',
                }}
              >
                <div
                  style={{
                    height: '100%',
                    width: `${Math.round(progress * 100)}%`,
                    backgroundColor: '#38bdf8',
                    transition: 'width 0.2s',
                  }}
                />
              </div>
              <div style={{ color: '#38bdf8', fontSize: '12px', textAlign: 'center' }}>
                {statusMessage}
              </div>
            </div>
          )}

          {exportResult && (
            <div
              style={{
                padding: '10px 12px',
                borderRadius: '6px',
                backgroundColor: exportResult.ok ? '#064e3b' : '#7f1d1d',
                color: exportResult.ok ? '#6ee7b7' : '#fca5a5',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                fontSize: '12px',
              }}
            >
              {exportResult.ok ? <CheckCircle2 size={16} /> : <AlertCircle size={16} />}
              <span>{statusMessage}</span>
            </div>
          )}
        </div>

        {/* Modal Footer */}
        <div
          style={{
            padding: '12px 20px',
            borderTop: '1px solid #27272a',
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '8px',
            backgroundColor: '#121214',
          }}
        >
          <button
            onClick={handleCancel}
            style={{
              padding: '8px 16px',
              backgroundColor: '#27272a',
              border: 'none',
              borderRadius: '6px',
              color: '#d4d4d8',
              cursor: 'pointer',
            }}
          >
            {isExporting ? 'Cancel' : 'Close'}
          </button>
          {!isExporting && (
            <button
              onClick={handleStartExport}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                padding: '8px 18px',
                backgroundColor: '#0284c7',
                border: 'none',
                borderRadius: '6px',
                color: '#fff',
                fontWeight: 600,
                cursor: 'pointer',
              }}
            >
              <Download size={15} />
              <span>Start Export</span>
            </button>
          )}
        </div>
      </div>
    </div>
  );
};
