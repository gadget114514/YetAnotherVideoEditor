import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { promises as fs } from 'fs';
import os from 'os';
import path from 'path';
import { resolveAppPath, startRendererServer, type RendererServer } from '../src/main/rendererServer';

describe('resolveAppPath', () => {
  const root = path.resolve('/tmp/renderer');

  it('maps / to index.html and nested assets under root', () => {
    expect(resolveAppPath(root, '/')).toBe(path.join(root, 'index.html'));
    expect(resolveAppPath(root, '/assets/app-1.js')).toBe(path.join(root, 'assets', 'app-1.js'));
  });

  it('rejects path traversal, including encoded forms', () => {
    expect(resolveAppPath(root, '/../secret.txt')).toBeNull();
    expect(resolveAppPath(root, '/%2e%2e/secret.txt')).toBeNull();
    expect(resolveAppPath(root, '/assets/../../etc/passwd')).toBeNull();
  });

  it('rejects malformed escapes and NUL', () => {
    expect(resolveAppPath(root, '/%E0%A4%A')).toBeNull();
    expect(resolveAppPath(root, '/a%00b')).toBeNull();
  });
});

describe('startRendererServer', () => {
  let dir: string;
  let server: RendererServer;

  beforeAll(async () => {
    dir = await fs.mkdtemp(path.join(os.tmpdir(), 'yave-renderer-'));
    await fs.writeFile(path.join(dir, 'index.html'), '<h1>hi</h1>');
    await fs.mkdir(path.join(dir, 'assets'));
    await fs.writeFile(path.join(dir, 'assets', 'a.js'), 'export {}');
    server = await startRendererServer(dir, 0);
  });
  afterAll(async () => {
    await server.close();
    await fs.rm(dir, { recursive: true, force: true });
  });

  it('serves files with isolation headers and correct MIME', async () => {
    const res = await fetch(`${server.origin}/`);
    expect(res.status).toBe(200);
    expect(await res.text()).toBe('<h1>hi</h1>');
    expect(res.headers.get('cross-origin-opener-policy')).toBe('same-origin');
    expect(res.headers.get('cross-origin-embedder-policy')).toBe('require-corp');

    const js = await fetch(`${server.origin}/assets/a.js`);
    expect(js.headers.get('content-type')).toContain('text/javascript');
  });

  it('returns 404 for missing files and rejects traversal', async () => {
    expect((await fetch(`${server.origin}/nope.js`)).status).toBe(404);
    const raw = await fetch(`${server.origin}/%2e%2e/%2e%2e/etc/passwd`);
    expect([403, 404]).toContain(raw.status);
  });

  it('binds to loopback only', () => {
    expect(server.origin.startsWith('http://127.0.0.1:')).toBe(true);
  });
});
