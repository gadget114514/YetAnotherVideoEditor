import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('yaveHost', {
  platform: 'electron',
  setTitle: (title: string) => ipcRenderer.send('host:setTitle', title),
});

function subscribe<T>(channel: string, cb: (value: T) => void): () => void {
  const listener = (_e: unknown, value: T) => cb(value);
  ipcRenderer.on(channel, listener);
  return () => ipcRenderer.removeListener(channel, listener);
}

contextBridge.exposeInMainWorld('yaveWhisper', {
  getStatus: () => ipcRenderer.invoke('whisper:status'),
  install: (req: unknown) => ipcRenderer.invoke('whisper:install', req),
  cancelInstall: () => ipcRenderer.invoke('whisper:cancelInstall'),
  selfTest: () => ipcRenderer.invoke('whisper:selfTest'),
  transcribe: (req: unknown) => ipcRenderer.invoke('whisper:transcribe', req),
  cancelTranscribe: () => ipcRenderer.invoke('whisper:cancelTranscribe'),
  onInstallProgress: (cb: (p: unknown) => void) => subscribe('whisper:installProgress', cb),
  onTranscribeProgress: (cb: (p: number) => void) => subscribe('whisper:transcribeProgress', cb),
});
