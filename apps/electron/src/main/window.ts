import { BrowserWindow, session, shell } from 'electron';
import path from 'path';

export function createMainWindow(appOrigin?: string): BrowserWindow {
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Cross-Origin-Opener-Policy': ['same-origin'],
        'Cross-Origin-Embedder-Policy': ['require-corp'],
      },
    });
  });

  const win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 700,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, '../preload/preload.js'),
    },
  });

  // Default: the bundled UI (dist/renderer) served by the loopback renderer server.
  // Set YAVE_DEV_URL (e.g. http://localhost:5173/YetAnotherVideoEditor/) to use a running Vite dev server.
  void win.loadURL(process.env.YAVE_DEV_URL ?? `${appOrigin}/index.html`);

  win.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  win.webContents.on('will-navigate', (event, url) => {
    if (!url.startsWith('http://localhost') && !(appOrigin && url.startsWith(appOrigin))) {
      event.preventDefault();
    }
  });

  return win;
}
