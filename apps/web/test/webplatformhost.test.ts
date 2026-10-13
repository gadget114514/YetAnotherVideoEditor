import { describe, it, expect, beforeEach, vi } from 'vitest';
import { WebPlatformHost } from '../src/WebPlatformHost.js';

describe('WebPlatformHost', () => {
  let host: WebPlatformHost;
  let localStorageMap: Map<string, string>;
  let sessionStorageMap: Map<string, string>;

  beforeEach(() => {
    localStorageMap = new Map();
    sessionStorageMap = new Map();

    const mockLocalStorage = {
      getItem: (key: string) => localStorageMap.get(key) ?? null,
      setItem: (key: string, value: string) => localStorageMap.set(key, value),
      removeItem: (key: string) => localStorageMap.delete(key),
      clear: () => localStorageMap.clear(),
    };

    const mockSessionStorage = {
      getItem: (key: string) => sessionStorageMap.get(key) ?? null,
      setItem: (key: string, value: string) => sessionStorageMap.set(key, value),
      removeItem: (key: string) => sessionStorageMap.delete(key),
      clear: () => sessionStorageMap.clear(),
    };

    vi.stubGlobal('localStorage', mockLocalStorage);
    vi.stubGlobal('sessionStorage', mockSessionStorage);

    host = new WebPlatformHost();
  });

  it('settings store (WebKeyValueStore) set, get, and delete', async () => {
    expect(await host.settings.get('theme', 'dark')).toBe('dark');

    await host.settings.set('theme', 'light');
    expect(await host.settings.get('theme', 'dark')).toBe('light');

    await host.settings.delete('theme');
    expect(await host.settings.get('theme', 'dark')).toBe('dark');
  });

  it('secrets store (WebSecretStore) setSecret, getSecret, and deleteSecret', async () => {
    expect(await host.secrets.getSecret('apiKey')).toBeNull();

    await host.secrets.setSecret('apiKey', 'secret-12345');
    expect(await host.secrets.getSecret('apiKey')).toBe('secret-12345');

    await host.secrets.deleteSecret('apiKey');
    expect(await host.secrets.getSecret('apiKey')).toBeNull();
  });

  it('platform properties and saveProjectAs', async () => {
    expect(host.kind).toBe('web');
    expect(host.capabilities.nativeFfmpeg).toBe(false);

    const handle = await host.saveProjectAs('{}', 'test-proj');
    expect(handle).not.toBeNull();
    expect(handle?.name).toBe('test-proj');
    expect(handle?.id).toBeDefined();
  });

  it('setTitle updates document title if document is present', () => {
    const mockDocument = { title: '' };
    vi.stubGlobal('document', mockDocument);

    host.setTitle('My Movie');
    expect(mockDocument.title).toBe('My Movie - YAVE');
  });
});
