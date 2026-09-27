import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('yaveHost', {
  platform: 'electron',
  setTitle: (title: string) => ipcRenderer.send('host:setTitle', title),
});
