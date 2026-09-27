import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  base: '/YetAnotherVideoEditor/',
  plugins: [react()],
  resolve: {
    alias: {
      '@yave/core': path.resolve(__dirname, '../../packages/core/src'),
      '@yave/io': path.resolve(__dirname, '../../packages/io/src'),
      '@yave/engine': path.resolve(__dirname, '../../packages/engine/src'),
      '@yave/platform': path.resolve(__dirname, '../../packages/platform/src'),
      '@yave/plugin-sdk': path.resolve(__dirname, '../../packages/plugin-sdk/src'),
      '@yave/ui': path.resolve(__dirname, '../../packages/ui/src'),
    },
  },
  worker: {
    format: 'es',
  },
  server: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
});
