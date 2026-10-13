import React, { useCallback, useEffect, useState } from 'react';
import {
  getWhisperBridge,
  type WhisperBridge,
  type WhisperInstallProgress,
  type WhisperModelId,
  type WhisperStatus,
} from '@yave/platform';
import { CheckCircle2, Download, Loader2, XCircle, X } from 'lucide-react';

type Step = 'check' | 'choose' | 'install' | 'test' | 'done';

export interface WhisperSetupWizardProps {
  onClose: () => void;
  /** Called whenever the local Whisper status changes (after install / test). */
  onStatusChange?: (status: WhisperStatus) => void;
}

const STEP_LABELS: Record<Step, string> = {
  check: '1. 環境確認',
  choose: '2. モデル選択',
  install: '3. インストール',
  test: '4. 動作テスト',
  done: '5. 完了',
};
const STEP_ORDER: Step[] = ['check', 'choose', 'install', 'test', 'done'];

const fmtMB = (bytes: number) =>
  bytes >= 1e9 ? `${(bytes / 1e9).toFixed(1)} GB` : `${Math.round(bytes / 1e6)} MB`;

const btn = (primary = false, disabled = false): React.CSSProperties => ({
  padding: '6px 14px',
  borderRadius: 4,
  border: primary ? 'none' : '1px solid #3f3f46',
  backgroundColor: primary ? (disabled ? '#6b21a8' : '#9333ea') : '#27272a',
  color: '#fff',
  cursor: disabled ? 'not-allowed' : 'pointer',
  opacity: disabled ? 0.6 : 1,
  fontSize: 12,
});

export const WhisperSetupWizard: React.FC<WhisperSetupWizardProps> = ({ onClose, onStatusChange }) => {
  const [bridge] = useState<WhisperBridge | null>(() => getWhisperBridge());
  const [step, setStep] = useState<Step>('check');
  const [status, setStatus] = useState<WhisperStatus | null>(null);
  const [modelId, setModelId] = useState<WhisperModelId>('small');
  const [useExisting, setUseExisting] = useState(false);
  const [customPath, setCustomPath] = useState('');
  const [progress, setProgress] = useState<WhisperInstallProgress | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [testResult, setTestResult] = useState<{ ok: boolean; message: string } | null>(null);

  const refresh = useCallback(async () => {
    if (!bridge) return null;
    const s = await bridge.getStatus();
    setStatus(s);
    onStatusChange?.(s);
    return s;
  }, [bridge, onStatusChange]);

  useEffect(() => {
    if (!bridge) return;
    refresh()
      .then((s) => {
        if (s) {
          setUseExisting(!s.canAutoDownloadBinary);
          if (s.activeModel) setModelId(s.activeModel);
        }
      })
      .catch((e) => setError(String(e?.message ?? e)));
  }, [bridge, refresh]);

  useEffect(() => {
    if (!bridge) return;
    return bridge.onInstallProgress(setProgress);
  }, [bridge]);

  const runInstall = async () => {
    if (!bridge) return;
    setStep('install');
    setError(null);
    setBusy(true);
    setProgress({ phase: 'binary', progress: 0, message: '準備中...' });
    try {
      const s = await bridge.install({
        modelId,
        useExistingBinary: useExisting,
        customBinaryPath: useExisting && customPath.trim() ? customPath.trim() : undefined,
      });
      setStatus(s);
      onStatusChange?.(s);
      setStep('test');
    } catch (e: any) {
      setError(String(e?.message ?? e));
    } finally {
      setBusy(false);
    }
  };

  const runTest = async () => {
    if (!bridge) return;
    setBusy(true);
    setTestResult(null);
    try {
      const r = await bridge.selfTest();
      setTestResult(r);
      if (r.ok) {
        await refresh();
        setStep('done');
      }
    } catch (e: any) {
      setTestResult({ ok: false, message: String(e?.message ?? e) });
    } finally {
      setBusy(false);
    }
  };

  const cancelInstall = async () => {
    await bridge?.cancelInstall();
  };

  const selectedModel = status?.models.find((m) => m.id === modelId);
  const pct = progress ? Math.round(progress.progress * 100) : 0;

  const body = () => {
    if (!bridge) {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          <div style={{ color: '#fbbf24' }}>
            ローカル Whisper のセットアップはデスクトップ版 (Electron) でのみ利用できます。
          </div>
          <div style={{ color: '#a1a1aa' }}>
            Web 版ではブラウザからローカルプロセスを起動・インストールできないため、デスクトップ版をご利用ください。
          </div>
          <div style={btnRow}>
            <button style={btn()} onClick={onClose}>閉じる</button>
          </div>
        </div>
      );
    }

    if (step === 'check') {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          {!status ? (
            <div><Loader2 size={14} className="animate-spin" /> 確認中...</div>
          ) : (
            <>
              <Row label="whisper-cli" ok={Boolean(status.binaryPath)}
                value={status.binaryPath ? `${status.binaryPath} (${status.binarySource})` : '未インストール'} />
              <Row label="モデル" ok={status.installedModels.length > 0}
                value={status.installedModels.length ? status.installedModels.join(', ') : '未ダウンロード'} />
              <Row label="インストール先" ok value={status.installDir} />
              {status.ready && (
                <div style={{ color: '#4ade80' }}>
                  既に利用可能です。モデルの追加・変更や再テストも行えます。
                </div>
              )}
            </>
          )}
          <div style={btnRow}>
            <button style={btn()} onClick={onClose}>閉じる</button>
            <button style={btn(true, !status)} disabled={!status} onClick={() => setStep('choose')}>
              次へ
            </button>
          </div>
        </div>
      );
    }

    if (step === 'choose') {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
          <div>
            <div style={label}>Whisper モデル</div>
            {status?.models.map((m) => (
              <label key={m.id} style={{ display: 'flex', gap: 8, padding: '4px 0', cursor: 'pointer' }}>
                <input type="radio" name="whisper-model" checked={modelId === m.id}
                  onChange={() => setModelId(m.id)} />
                <span>
                  {m.label}
                  <span style={{ color: '#71717a' }}> — {m.note}</span>
                  {status.installedModels.includes(m.id) && (
                    <span style={{ color: '#4ade80' }}> (導入済み)</span>
                  )}
                </span>
              </label>
            ))}
          </div>

          <div>
            <div style={label}>whisper.cpp 実行ファイル</div>
            {status?.canAutoDownloadBinary && (
              <label style={{ display: 'flex', gap: 8, padding: '4px 0', cursor: 'pointer' }}>
                <input type="radio" name="whisper-bin" checked={!useExisting}
                  onChange={() => setUseExisting(false)} />
                <span>公式リリースを自動ダウンロード (約 9 MB, SHA-256 検証付き)</span>
              </label>
            )}
            <label style={{ display: 'flex', gap: 8, padding: '4px 0', cursor: 'pointer' }}>
              <input type="radio" name="whisper-bin" checked={useExisting}
                onChange={() => setUseExisting(true)} />
              <span>インストール済みの whisper-cli を使う (PATH または指定パス)</span>
            </label>
            {useExisting && (
              <input
                value={customPath}
                onChange={(e) => setCustomPath(e.target.value)}
                placeholder={status?.platform === 'win32' ? 'C:\\tools\\whisper\\whisper-cli.exe (空欄で PATH を検索)'
                  : '/opt/homebrew/bin/whisper-cli (空欄で PATH を検索)'}
                style={input}
              />
            )}
            {!status?.canAutoDownloadBinary && (
              <div style={{ color: '#a1a1aa', marginTop: 4 }}>
                macOS: <code>brew install whisper-cpp</code> / Linux: whisper.cpp をビルドして PATH に置いてください。
              </div>
            )}
          </div>

          <div style={{ color: '#a1a1aa' }}>
            ダウンロードするもの: モデル {selectedModel ? fmtMB(selectedModel.sizeBytes) : ''}
            {!useExisting && ' + whisper.cpp 実行ファイル'} (保存先: {status?.installDir})
          </div>

          <div style={btnRow}>
            <button style={btn()} onClick={() => setStep('check')}>戻る</button>
            <button style={btn(true)} onClick={runInstall}>
              <Download size={13} style={{ verticalAlign: -2 }} /> インストール開始
            </button>
          </div>
        </div>
      );
    }

    if (step === 'install') {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          {error ? (
            <div style={{ color: '#f87171', whiteSpace: 'pre-wrap' }}>
              <XCircle size={14} style={{ verticalAlign: -2 }} /> {error}
            </div>
          ) : (
            <>
              <div>{progress?.message ?? '...'}</div>
              <div style={{ backgroundColor: '#27272a', height: 8, borderRadius: 4, overflow: 'hidden' }}>
                <div style={{ width: `${pct}%`, height: '100%', backgroundColor: '#a855f7', transition: 'width .2s' }} />
              </div>
              <div style={{ color: '#a1a1aa' }}>{pct}%</div>
            </>
          )}
          <div style={btnRow}>
            {busy ? (
              <button style={btn()} onClick={cancelInstall}>キャンセル</button>
            ) : (
              <>
                <button style={btn()} onClick={() => setStep('choose')}>戻る</button>
                <button style={btn(true)} onClick={runInstall}>再試行</button>
              </>
            )}
          </div>
        </div>
      );
    }

    if (step === 'test') {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
          <div>インストールが完了しました。無音の音声で Whisper を実行し、動作を確認します。</div>
          {testResult && (
            <div style={{ color: testResult.ok ? '#4ade80' : '#f87171', whiteSpace: 'pre-wrap' }}>
              {testResult.ok ? <CheckCircle2 size={14} style={{ verticalAlign: -2 }} /> : <XCircle size={14} style={{ verticalAlign: -2 }} />}{' '}
              {testResult.message}
            </div>
          )}
          <div style={btnRow}>
            <button style={btn(true, busy)} disabled={busy} onClick={runTest}>
              {busy ? '実行中...' : testResult && !testResult.ok ? '再テスト' : 'テスト実行'}
            </button>
          </div>
        </div>
      );
    }

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
        <div style={{ color: '#4ade80' }}>
          <CheckCircle2 size={14} style={{ verticalAlign: -2 }} /> ローカル Whisper を利用できます。
        </div>
        <div style={{ color: '#a1a1aa' }}>
          「字幕アシスタント」パネルの「Auto Subtitle」やメディアライブラリへの音声インポートで、自動的にローカル Whisper が使われます
          (音声は端末外へ送信されません)。
        </div>
        <div style={btnRow}>
          <button style={btn(true)} onClick={onClose}>閉じる</button>
        </div>
      </div>
    );
  };

  return (
    <div style={overlay}>
      <div style={dialog}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 10 }}>
          <strong>Whisper ローカルセットアップ</strong>
          <button style={{ ...btn(), padding: 4 }} onClick={onClose} aria-label="close">
            <X size={14} />
          </button>
        </div>
        <div style={{ display: 'flex', gap: 6, marginBottom: 14, flexWrap: 'wrap' }}>
          {STEP_ORDER.map((s) => (
            <span key={s} style={{
              padding: '2px 8px', borderRadius: 10, fontSize: 11,
              backgroundColor: s === step ? '#581c87' : '#27272a',
              border: s === step ? '1px solid #a855f7' : '1px solid #3f3f46',
              color: s === step ? '#fff' : '#a1a1aa',
            }}>{STEP_LABELS[s]}</span>
          ))}
        </div>
        {error && step !== 'install' && (
          <div style={{ color: '#f87171', marginBottom: 8 }}>{error}</div>
        )}
        {body()}
      </div>
    </div>
  );
};

const Row: React.FC<{ label: string; ok: boolean; value: string }> = ({ label: l, ok, value }) => (
  <div style={{ display: 'flex', gap: 8, alignItems: 'flex-start' }}>
    {ok ? <CheckCircle2 size={14} color="#4ade80" /> : <XCircle size={14} color="#f87171" />}
    <span style={{ width: 90, color: '#a1a1aa', flexShrink: 0 }}>{l}</span>
    <span style={{ wordBreak: 'break-all' }}>{value}</span>
  </div>
);

const overlay: React.CSSProperties = {
  position: 'fixed', inset: 0, backgroundColor: 'rgba(0,0,0,0.6)', zIndex: 1000,
  display: 'flex', alignItems: 'center', justifyContent: 'center',
};
const dialog: React.CSSProperties = {
  width: 560, maxWidth: '92vw', maxHeight: '90vh', overflowY: 'auto', padding: 16,
  backgroundColor: '#18181b', color: '#e4e4e7', border: '1px solid #3f3f46',
  borderRadius: 8, fontSize: 12,
};
const btnRow: React.CSSProperties = { display: 'flex', justifyContent: 'flex-end', gap: 8, marginTop: 6 };
const label: React.CSSProperties = { color: '#a1a1aa', fontSize: 11, marginBottom: 4 };
const input: React.CSSProperties = {
  width: '100%', boxSizing: 'border-box', marginTop: 4, padding: '5px 8px',
  backgroundColor: '#27272a', border: '1px solid #3f3f46', borderRadius: 4, color: '#fff', fontSize: 11,
};
