import React, { useState, useRef, useEffect } from 'react';
import { getWhisperBridge, type WhisperStatus } from '@yave/platform';
import type { LyricsAlignmentResult } from '@yave/engine';
import { useProjectStore } from '../store/useProjectStore.js';
import { WhisperSetupWizard } from '../dialogs/WhisperSetupWizard.js';
import { Sparkles, Bot, Upload, Loader2, Settings2 } from 'lucide-react';

const LYRICS_PLACEHOLDER = [
  '正しい歌詞を1行ずつ貼り付けてください。',
  '認識結果はこの歌詞に置き換えられ、',
  '音声に合わせて時刻が自動設定されます。',
].join(String.fromCharCode(10));

const countLines = (text: string): number => text.split(String.fromCharCode(10)).filter((l) => l.trim()).length;

const fmtTime = (sec: number): string => {
  const m = Math.floor(sec / 60);
  const s = sec - m * 60;
  return `${String(m).padStart(2, '0')}:${s.toFixed(1).padStart(4, '0')}`;
};

export const AiPanel: React.FC = () => {
  const { project, importAudioWithSubtitles } = useProjectStore();
  const [prompt, setPrompt] = useState('');
  const [mode, setMode] = useState<'text2video' | 'stt' | 'tts'>('stt');
  const [status, setStatus] = useState<string>('Ready');
  const [progress, setProgress] = useState<number>(0);
  const [isGenerating, setIsGenerating] = useState<boolean>(false);
  const [language, setLanguage] = useState<string>('ja');
  const [selectedAssetId, setSelectedAssetId] = useState<string>('');
  const [selectedFile, setSelectedFile] = useState<File | null>(null);
  const [lyrics, setLyrics] = useState<string>('');
  const [alignment, setAlignment] = useState<LyricsAlignmentResult | null>(null);

  const fileInputRef = useRef<HTMLInputElement>(null);

  const [showWizard, setShowWizard] = useState(false);
  const [whisper, setWhisper] = useState<WhisperStatus | null>(null);
  const hasWhisperHost = getWhisperBridge() !== null;

  useEffect(() => {
    getWhisperBridge()?.getStatus().then(setWhisper).catch(() => setWhisper(null));
  }, []);

  const audioAssets = project.assets
    .allIds()
    .map((id) => project.assets.asset(id)!)
    .filter((a) => a && a.kind === 'audio');

  const handleGenerate = async () => {
    if (mode === 'stt') {
      setIsGenerating(true);
      setProgress(0);
      setAlignment(null);
      setStatus('Starting AI Speech-to-Text transcription...');

      try {
        let audioInput: File | { name: string; durationFrames?: number } | null = null;
        if (selectedFile) {
          audioInput = selectedFile;
        } else if (selectedAssetId) {
          const asset = project.assets.asset(selectedAssetId as any);
          if (asset) {
            audioInput = { name: asset.relativePath, durationFrames: asset.durationFrames };
          }
        } else if (audioAssets.length > 0) {
          audioInput = { name: audioAssets[0]!.relativePath, durationFrames: audioAssets[0]!.durationFrames };
        }

        if (!audioInput) {
          alert('Please select or upload an audio file first.');
          setIsGenerating(false);
          setStatus('Ready');
          return;
        }

        setStatus(`Transcribing audio: ${audioInput.name}...`);
        const result = await importAudioWithSubtitles(audioInput, {
          prompt: prompt.trim() || undefined,
          lyrics: lyrics.trim() || undefined,
          language,
          onProgress: (p) => {
            setProgress(Math.round(p * 100));
            setStatus(`AI transcription in progress... ${Math.round(p * 100)}%`);
          },
        });

        setProgress(100);
        setAlignment(result.lyricsAlignment ?? null);
        setStatus(
          `Success: Created audio track and imported ${result.subtitleClips.length} subtitle cues into timeline!`
        );
      } catch (err: any) {
        setStatus(`Error: ${err?.message || 'Transcription failed'}`);
      } finally {
        setIsGenerating(false);
      }
    } else {
      if (!prompt.trim()) return;
      setIsGenerating(true);
      setStatus('Generating preview...');
      setTimeout(() => {
        setIsGenerating(false);
        setStatus(`Generation simulated for "${prompt.substring(0, 20)}..."`);
      }, 1200);
    }
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#18181b', color: '#e4e4e7', fontSize: '12px' }}>
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #27272a', display: 'flex', alignItems: 'center', gap: '6px', fontWeight: 600 }}>
        <Sparkles size={16} className="text-purple-400" />
        <span>字幕アシスタント</span>
      </div>

      <div style={{ flex: 1, padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px', overflowY: 'auto' }}>
        <div>
          <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>Task Mode</label>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px' }}>
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
                fontWeight: mode === 'stt' ? 600 : 400,
              }}
            >
              Auto Subtitle
            </button>
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
                fontWeight: mode === 'text2video' ? 600 : 400,
              }}
            >
              Video
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
                fontWeight: mode === 'tts' ? 600 : 400,
              }}
            >
              Narration
            </button>
          </div>
        </div>

        {mode === 'stt' ? (
          <>
            <div style={{ padding: '8px', backgroundColor: '#202024', borderRadius: '4px', border: '1px solid #2e2e34', display: 'flex', alignItems: 'center', gap: '8px' }}>
              <div style={{ flex: 1, fontSize: '11px', color: whisper?.ready ? '#4ade80' : '#fbbf24' }}>
                {!hasWhisperHost
                  ? 'Whisper: Web 版ではデモ用モックを使用します (ローカル Whisper はデスクトップ版のみ)'
                  : whisper?.ready
                    ? `Whisper: ローカル利用可 (${whisper.activeModel})`
                    : 'Whisper: 未セットアップ (現在はデモ用モックで動作します)'}
              </div>
              {hasWhisperHost && (
                <button
                  onClick={() => setShowWizard(true)}
                  style={{ display: 'flex', alignItems: 'center', gap: '4px', padding: '4px 8px', backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: '4px', color: '#e4e4e7', cursor: 'pointer', fontSize: '11px' }}
                >
                  <Settings2 size={12} />
                  <span>{whisper?.ready ? '設定' : 'セットアップ'}</span>
                </button>
              )}
            </div>

            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>
                Audio Source (音声ソース)
              </label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                <div style={{ display: 'flex', gap: '6px' }}>
                  <button
                    onClick={() => fileInputRef.current?.click()}
                    style={{
                      flex: 1,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '6px',
                      padding: '6px 10px',
                      backgroundColor: '#27272a',
                      border: '1px solid #3f3f46',
                      borderRadius: '4px',
                      color: '#e4e4e7',
                      cursor: 'pointer',
                      fontSize: '11px',
                    }}
                  >
                    <Upload size={13} />
                    <span>{selectedFile ? selectedFile.name : 'Upload Audio File...'}</span>
                  </button>
                  <input
                    ref={fileInputRef}
                    type="file"
                    accept="audio/*"
                    style={{ display: 'none' }}
                    onChange={(e) => {
                      if (e.target.files?.[0]) {
                        setSelectedFile(e.target.files[0]);
                        setSelectedAssetId('');
                      }
                    }}
                  />
                </div>

                {audioAssets.length > 0 && (
                  <div>
                    <span style={{ fontSize: '10px', color: '#71717a' }}>Or select from library:</span>
                    <select
                      value={selectedAssetId}
                      onChange={(e) => {
                        setSelectedAssetId(e.target.value);
                        setSelectedFile(null);
                      }}
                      style={{
                        width: '100%',
                        marginTop: '2px',
                        padding: '4px 6px',
                        backgroundColor: '#27272a',
                        border: '1px solid #3f3f46',
                        borderRadius: '4px',
                        color: '#fff',
                        fontSize: '11px',
                      }}
                    >
                      <option value="">-- Choose loaded audio asset --</option>
                      {audioAssets.map((a) => (
                        <option key={a.id} value={a.id}>
                          {a.relativePath}
                        </option>
                      ))}
                    </select>
                  </div>
                )}
              </div>
            </div>

            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>
                Language (言語)
              </label>
              <select
                value={language}
                onChange={(e) => setLanguage(e.target.value)}
                style={{
                  width: '100%',
                  padding: '5px 8px',
                  backgroundColor: '#27272a',
                  border: '1px solid #3f3f46',
                  borderRadius: '4px',
                  color: '#fff',
                  fontSize: '11px',
                }}
              >
                <option value="ja">Japanese (日本語)</option>
                <option value="en">English (英語)</option>
                <option value="auto">Auto-detect (自動判別)</option>
              </select>
            </div>

            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>
                正しい歌詞・台本 (任意 / 1行 = 1字幕)
              </label>
              <textarea
                rows={6}
                value={lyrics}
                onChange={(e) => setLyrics(e.target.value)}
                placeholder={LYRICS_PLACEHOLDER}
                style={{
                  width: '100%',
                  padding: '6px',
                  backgroundColor: '#27272a',
                  border: lyrics.trim() ? '1px solid #a855f7' : '1px solid #3f3f46',
                  borderRadius: '4px',
                  color: '#fff',
                  boxSizing: 'border-box',
                  resize: 'vertical',
                }}
              />
              {lyrics.trim() && (
                <div style={{ fontSize: '10px', color: '#71717a', marginTop: '2px' }}>
                  {countLines(lyrics)} 行 — 認識した音声と照合して字幕を置き換えます
                </div>
              )}
            </div>

            <div>
              <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>
                Vocabulary / Prompt Hints (プロンプト・用語指定 - 任意)
              </label>
              <textarea
                rows={2}
                value={prompt}
                onChange={(e) => setPrompt(e.target.value)}
                placeholder="Optional terminology, speaker hints, or custom script..."
                style={{
                  width: '100%',
                  padding: '6px',
                  backgroundColor: '#27272a',
                  border: '1px solid #3f3f46',
                  borderRadius: '4px',
                  color: '#fff',
                  boxSizing: 'border-box',
                  resize: 'vertical',
                }}
              />
            </div>
          </>
        ) : (
          <div>
            <label style={{ display: 'block', color: '#a1a1aa', fontSize: '11px', marginBottom: '4px' }}>
              Prompt / Script
            </label>
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
        )}

        <button
          onClick={handleGenerate}
          disabled={isGenerating}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            padding: '8px',
            backgroundColor: isGenerating ? '#6b21a8' : '#9333ea',
            border: 'none',
            borderRadius: '4px',
            color: '#fff',
            fontWeight: 600,
            cursor: isGenerating ? 'wait' : 'pointer',
          }}
        >
          {isGenerating ? <Loader2 size={15} className="animate-spin" /> : <Bot size={15} />}
          <span>
            {mode === 'stt'
              ? isGenerating
                ? `Transcribing (${progress}%)...`
                : 'Transcribe Audio & Import Subtitles'
              : 'Generate Asset'}
          </span>
        </button>

        {isGenerating && (
          <div style={{ width: '100%', backgroundColor: '#27272a', borderRadius: '3px', height: '6px', overflow: 'hidden' }}>
            <div
              style={{
                width: `${progress}%`,
                height: '100%',
                backgroundColor: '#a855f7',
                transition: 'width 0.2s',
              }}
            />
          </div>
        )}

        <div style={{ padding: '8px', backgroundColor: '#202024', borderRadius: '4px', border: '1px solid #2e2e34', color: '#a1a1aa', fontSize: '11px' }}>
          <strong>Status:</strong> {status}
        </div>

        {alignment && (
          <div style={{ padding: '8px', backgroundColor: '#202024', borderRadius: '4px', border: '1px solid #2e2e34', fontSize: '11px' }}>
            <div style={{ color: '#e4e4e7', marginBottom: '4px' }}>
              <strong>歌詞アラインメント結果</strong>
              <span style={{ color: '#a1a1aa' }}>
                {' '}平均一致率 {Math.round(alignment.averageConfidence * 100)}%
              </span>
            </div>
            <div style={{ maxHeight: '180px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '2px' }}>
              {alignment.lines.map((l) => {
                const pct = Math.round(l.confidence * 100);
                const color = !l.anchored ? '#f87171' : pct < 50 ? '#fbbf24' : '#4ade80';
                return (
                  <div key={l.index} style={{ display: 'flex', gap: '6px', alignItems: 'baseline' }}>
                    <span style={{ width: '88px', flexShrink: 0, color: '#71717a', fontFamily: 'monospace' }}>
                      {fmtTime(l.startSeconds)}–{fmtTime(l.endSeconds)}
                    </span>
                    <span style={{ flex: 1, color: '#e4e4e7' }}>{l.text}</span>
                    <span title={l.anchored ? '音声と一致した割合' : '認識できず時刻は補間'} style={{ color, flexShrink: 0 }}>
                      {l.anchored ? `${pct}%` : '補間'}
                    </span>
                  </div>
                );
              })}
            </div>
            <div style={{ color: '#71717a', marginTop: '4px' }}>
              「補間」や低い一致率の行は時刻がずれている可能性があります。字幕エディタで調整してください。
            </div>
          </div>
        )}
      </div>
      {showWizard && (
        <WhisperSetupWizard onClose={() => setShowWizard(false)} onStatusChange={setWhisper} />
      )}
    </div>
  );
};

