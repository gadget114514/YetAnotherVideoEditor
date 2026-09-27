export interface ProjectHandle {
  readonly id: string;
  readonly name: string;
}

export type AssetSource = File | string;

export interface KeyValueStore {
  get<T>(key: string, defaultValue: T): Promise<T>;
  set<T>(key: string, value: T): Promise<void>;
  delete(key: string): Promise<void>;
}

export interface SecretStore {
  getSecret(key: string): Promise<string | null>;
  setSecret(key: string, secret: string): Promise<void>;
  deleteSecret(key: string): Promise<void>;
}

export interface PlatformHost {
  readonly kind: 'web' | 'electron';
  readonly capabilities: {
    readonly nativeFileSystem: boolean;
    readonly crossOriginIsolated: boolean;
    readonly nativeFfmpeg: boolean;
    readonly corsFreeFetch: boolean;
  };
  openProject(): Promise<ProjectHandle | null>;
  saveProject(h: ProjectHandle, json: string): Promise<void>;
  saveProjectAs(json: string, suggestedName: string): Promise<ProjectHandle | null>;
  pickMediaFiles(): Promise<AssetSource[]>;
  readAsset(h: ProjectHandle, relativePath: string): Promise<Blob>;
  resolveRelative(h: ProjectHandle, absoluteOrHandle: AssetSource): string | null;
  createExportSink(suggestedName: string): Promise<WritableStream<Uint8Array>>;
  readonly settings: KeyValueStore;
  readonly secrets: SecretStore;
  fetch(input: RequestInfo, init?: RequestInit): Promise<Response>;
  setTitle(title: string): void;
  onBeforeClose(handler: () => Promise<boolean>): void;
}
