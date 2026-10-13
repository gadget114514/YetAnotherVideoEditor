import { app, BrowserWindow } from 'electron';
import path from 'path';
import { createMainWindow } from './window.js';
import { registerWhisperIpc } from './whisper/whisperIpc.js';
import { startRendererServer, type RendererServer } from './rendererServer.js';

let rendererServer: RendererServer | null = null;

app.whenReady().then(async () => {
  registerWhisperIpc();
  // YAVE_DEV_URL points the window at a running Vite dev server instead of the bundled UI.
  if (!process.env.YAVE_DEV_URL) {
    rendererServer = await startRendererServer(path.join(__dirname, '../renderer'));
  }
  const origin = rendererServer?.origin;
  createMainWindow(origin);

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createMainWindow(origin);
    }
  });
});

app.on('will-quit', () => {
  void rendererServer?.close();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
