import { en } from './locales/en.js';
import { ja } from './locales/ja.js';

export type SupportedLanguage = 'en' | 'ja';

export class LanguageManager {
  private static instance_: LanguageManager | null = null;
  private currentLanguage_: SupportedLanguage = 'en';
  private dictionaries_: Record<SupportedLanguage, Record<string, string>> = {
    en,
    ja,
  };
  private listeners_: ((lang: SupportedLanguage) => void)[] = [];

  static instance(): LanguageManager {
    if (!this.instance_) {
      this.instance_ = new LanguageManager();
      this.instance_.initialize();
    }
    return this.instance_;
  }

  initialize(): void {
    // Can detect browser language if available
    if (typeof navigator !== 'undefined' && navigator.language?.startsWith('ja')) {
      this.currentLanguage_ = 'ja';
    } else {
      this.currentLanguage_ = 'en';
    }
  }

  currentLanguage(): SupportedLanguage {
    return this.currentLanguage_;
  }

  setLanguage(lang: SupportedLanguage): void {
    if (this.currentLanguage_ !== lang) {
      this.currentLanguage_ = lang;
      for (const cb of this.listeners_) {
        cb(lang);
      }
    }
  }

  t(key: string): string {
    const dict = this.dictionaries_[this.currentLanguage_];
    return dict?.[key] ?? key;
  }

  translate(context: string, text: string): string {
    return this.t(text);
  }

  onLanguageChanged(cb: (lang: SupportedLanguage) => void): () => void {
    this.listeners_.push(cb);
    return () => {
      this.listeners_ = this.listeners_.filter((l) => l !== cb);
    };
  }
}

export function t(key: string): string {
  return LanguageManager.instance().t(key);
}
