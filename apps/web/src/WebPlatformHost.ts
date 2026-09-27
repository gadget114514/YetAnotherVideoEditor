import type { PlatformHost, ProjectHandle, AssetSource, KeyValueStore, SecretStore } from '@yave/platform';

class WebKeyValueStore implements KeyValueStore {
  async get<T>(key: string, defaultValue: T): Promise<T> {
    try {
      const val = localStorage.getItem(`yave.settings.${key}`);
      return val ? JSON.parse(val) : defaultValue;
    } catch {
      return defaultValue;
    }
  }

  async set<T>(key: string, value: T): Promise<void> {
    localStorage.setItem(`yave.settings.${key}`, JSON.stringify(value));
  }

  async delete(key: string): Promise<void> {
    localStorage.removeItem(`yave.settings.${key}`);
  }
}

class WebSecretStore implements SecretStore {
  async getSecret(key: string): Promise<string | null> {
    return sessionStorage.getItem(`yave.secret.${key}`);
  }

  async setSecret(key: string, secret: string): Promise<void> {
    sessionStorage.setItem(`yave.secret.${key}`, secret);
  }

  async deleteSecret(key: string): Promise<void> {
    sessionStorage.removeItem(`yave.secret.${key}`);
  }
}

export class WebPlatformHost implements PlatformHost {
  readonly kind = 'web' as const;

  readonly capabilities = {
    nativeFileSystem: typeof window !== 'undefined' && 'showOpenFilePicker' in window,
    crossOriginIsolated: typeof window !== 'undefined' && Boolean(window.crossOriginIsolated),
    nativeFfmpeg: false,
    corsFreeFetch: false,
  };

  readonly settings: KeyValueStore = new WebKeyValueStore();
  readonly secrets: SecretStore = new WebSecretStore();

  async openProject(): Promise<ProjectHandle | null> {
    return null;
  }

  async saveProject(_h: ProjectHandle, _json: string): Promise<void> {
    // Web File System Access or download
  }

  async saveProjectAs(_json: string, suggestedName: string): Promise<ProjectHandle | null> {
    return { id: crypto.randomUUID(), name: suggestedName };
  }

  async pickMediaFiles(): Promise<AssetSource[]> {
    return [];
  }

  async readAsset(_h: ProjectHandle, _relativePath: string): Promise<Blob> {
    return new Blob([]);
  }

  resolveRelative(_h: ProjectHandle, _absoluteOrHandle: AssetSource): string | null {
    return null;
  }

  async createExportSink(_suggestedName: string): Promise<WritableStream<Uint8Array>> {
    return new WritableStream<Uint8Array>();
  }

  async fetch(input: RequestInfo, init?: RequestInit): Promise<Response> {
    return window.fetch(input, init);
  }

  setTitle(title: string): void {
    if (typeof document !== 'undefined') {
      document.title = `${title} - YAVE`;
    }
  }

  onBeforeClose(handler: () => Promise<boolean>): void {
    if (typeof window !== 'undefined') {
      window.addEventListener('beforeunload', (e) => {
        handler().then((allow) => {
          if (!allow) {
            e.preventDefault();
            e.returnValue = '';
          }
        });
      });
    }
  }
}
