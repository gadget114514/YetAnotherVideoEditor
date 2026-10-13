import { promises as fs } from 'fs';
import http from 'http';
import type { AddressInfo } from 'net';
import path from 'path';

/**
 * Serves the bundled web UI (apps/web build) from a loopback-only HTTP server.
 * A real http origin is required for COOP/COEP, i.e. `crossOriginIsolated` (SharedArrayBuffer / WebCodecs paths);
 * Chromium ignores those headers on custom schemes.
 */

const PREFERRED_PORT = 51734; // fixed so localStorage/IndexedDB survive restarts; falls back to a random port

const MIME: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.wasm': 'application/wasm',
  '.map': 'application/json',
  '.txt': 'text/plain; charset=utf-8',
};

/** Maps a request pathname onto a file below `root`; returns null when it would escape `root`. */
export function resolveAppPath(root: string, pathname: string): string | null {
  let decoded: string;
  try {
    decoded = decodeURIComponent(pathname);
  } catch {
    return null;
  }
  if (decoded.includes('\0')) return null;
  const rel = decoded === '/' || decoded === '' ? 'index.html' : decoded.replace(/^\/+/, '');
  const rootResolved = path.resolve(root);
  const full = path.resolve(rootResolved, rel);
  if (full !== rootResolved && !full.startsWith(rootResolved + path.sep)) return null;
  return full;
}

export const ISOLATION_HEADERS = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
  'Cross-Origin-Resource-Policy': 'same-origin',
} as const;

export interface RendererServer {
  readonly origin: string;
  close(): Promise<void>;
}

export function startRendererServer(root: string, preferredPort = PREFERRED_PORT): Promise<RendererServer> {
  const server = http.createServer(async (req, res) => {
    const send = (status: number, body: string) => {
      res.writeHead(status, { 'Content-Type': 'text/plain; charset=utf-8', ...ISOLATION_HEADERS });
      res.end(body);
    };
    if (req.method !== 'GET' && req.method !== 'HEAD') return send(405, 'Method Not Allowed');

    const file = resolveAppPath(root, new URL(req.url ?? '/', 'http://localhost').pathname);
    if (!file) return send(403, 'Forbidden');
    try {
      const data = await fs.readFile(file);
      res.writeHead(200, {
        'Content-Type': MIME[path.extname(file).toLowerCase()] ?? 'application/octet-stream',
        'Cache-Control': 'no-cache',
        ...ISOLATION_HEADERS,
      });
      res.end(req.method === 'HEAD' ? undefined : data);
    } catch {
      send(404, 'Not Found');
    }
  });

  const listen = (port: number) =>
    new Promise<void>((resolve, reject) => {
      server.once('error', reject);
      server.listen(port, '127.0.0.1', () => {
        server.off('error', reject);
        resolve();
      });
    });

  return listen(preferredPort)
    .catch(() => listen(0))
    .then(() => {
      const { port } = server.address() as AddressInfo;
      return {
        origin: `http://127.0.0.1:${port}`,
        close: () => new Promise<void>((resolve) => server.close(() => resolve())),
      };
    });
}
