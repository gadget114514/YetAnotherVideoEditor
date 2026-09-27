import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from '@yave/ui';
import { WebPlatformHost } from './WebPlatformHost.js';

const host = new WebPlatformHost();
host.setTitle('Project');

const rootEl = document.getElementById('root');
if (rootEl) {
  ReactDOM.createRoot(rootEl).render(
    <React.StrictMode>
      <App host={host} />
    </React.StrictMode>
  );
}
