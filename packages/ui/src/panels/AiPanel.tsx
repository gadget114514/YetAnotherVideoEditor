import React, { useState } from 'react';
import { useProjectStore } from '../store/useProjectStore.js';
import { Sparkles, Bot, Play } from 'lucide-react';

export const AiPanel: React.FC = () => {
  const [prompt, setPrompt] = useState('');
  const [mode, setMode] = useState<'text2video' | 'stt' | 'tts'>('text2video');
  const [status, setStatus] = useState<string>('Ready');

  const handleGenerate = () => {
    if (!prompt.trim()) return;
    setStatus('Generating preview...');
    setTimeout(() => {
      setStatus(`Generation simulated for "${prompt.substring(0, 20)}..."`);
    }, 1200);
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
        <Sparkles size={16} className="text-purple-400" />
        <span>AI Orchestrator (M6)</span>
      </div>

      <div style={{ flex: 1, padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div>
          <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Task Mode</label>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
            <button
              onClick={() => setMode('text2video')}
              style={{
                padding: '6px 4px',
                borderRadius: '4px',
                border: mode === 'text2video' ? '1px solid #a855f7' : '1px solid #3f3f46',
                backgroundColor: mode === 'text2video' ? '#581c87' : '#27272a',
                color: '#fff',
                cursor: 'pointer',
                fontSize: '11px',
              }}
            >
              Video
            </button>
            <button
              onClick={() => setMode('stt')}
              style={{
                padding: '6px 4px',
                borderRadius: '4px',
                border: mode === 'stt' ? '1px solid #a855f7' : '1px solid #3f3f46',
                backgroundColor: mode === 'stt' ? '#581c87' : '#27272a',
                color: '#fff',
                cursor: 'pointer',
                fontSize: '11px',
              }}
            >
              Auto Subtitle
            </button>
            <button
              onClick={() => setMode('tts')}
              style={{
                padding: '6px 4px',
                borderRadius: '4px',
                border: mode === 'tts' ? '1px solid #a855f7' : '1px solid #3f3f46',
                backgroundColor: mode === 'tts' ? '#581c87' : '#27272a',
                color: '#fff',
                cursor: 'pointer',
                fontSize: '11px',
              }}
            >
              Narration
            </button>
          </div>
        </div>

        <div>
          <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Prompt / Script</label>
          <textarea
            rows={4}
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
            placeholder="Enter AI generation instructions or narration text..."
            style={{
              width: '100%',
              padding: '8px',
              backgroundColor: '#27272a',
              border: '1px solid #3f3f46',
              borderRadius: '4px',
              color: '#fff',
              boxSizing: 'border-box',
              resize: 'vertical',
            }}
          />
        </div>

        <button
          onClick={handleGenerate}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            padding: '8px',
            backgroundColor: '#9333ea',
            border: 'none',
            borderRadius: '4px',
            color: '#fff',
            fontWeight: 600,
            cursor: 'pointer',
          }}
        >
          <Bot size={15} />
          <span>Generate Asset</span>
        </button>

        <div style={{ padding: '8px', backgroundColor: '#202024', borderRadius: '4px', border: '1px solid #2e2e34', color: '#a1a1aa', fontSize: '11px' }}>
          <strong>Status:</strong> {status}
        </div>
      </div>
    </div>
  );
};
