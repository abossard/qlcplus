import { lazy, Suspense, useEffect, useState } from 'react';
import { useVCStore } from './store/vc-store';

const DmxView = lazy(() => import('./views/DmxView'));

export default function App() {
  const connected = useVCStore(s => s.connected);
  const projectLoaded = useVCStore(s => s.projectLoaded);
  const grandMasterValue = useVCStore(s => s.grandMasterValue);
  const init = useVCStore(s => s.init);
  const cleanup = useVCStore(s => s.cleanup);

  const [compact, setCompact] = useState(false);

  useEffect(() => {
    init();
    return () => cleanup();
  }, [init, cleanup]);

  function onGMChange(e: React.ChangeEvent<HTMLInputElement>) {
    const v = parseInt(e.target.value, 10);
    useVCStore.setState({ grandMasterValue: v });
    useVCStore.getState().wsClient?.send(`GM_VALUE|${v}`);
  }

  let statusText: string;
  let statusClass: string;
  if (connected) {
    statusText = projectLoaded ? 'Live' : 'Waiting…';
    statusClass = 'connected';
  } else if (projectLoaded) {
    statusText = 'Demo Mode';
    statusClass = 'demo';
  } else {
    statusText = 'Offline';
    statusClass = '';
  }

  return (
    <div className={`app${compact ? ' compact' : ''}`}>
      <header className="topbar">
        <span
          className={`status-dot ${statusClass}`}
          title={statusText}
          aria-label={statusText}
        />
        <span className="status-text">{statusText}</span>

        <button
          type="button"
          className={`compact-toggle${compact ? ' on' : ''}`}
          aria-pressed={compact}
          title="Compact mode"
          onClick={() => setCompact(c => !c)}
        >
          {compact ? '▣' : '▢'}
        </button>
      </header>

      <main className="viewport-shell">
        <Suspense fallback={<div className="empty-state">Loading…</div>}>
          <DmxView />
        </Suspense>
      </main>

      <footer className="grand-master" aria-label="Grand Master">
        <label htmlFor="gm-slider">Grand Master</label>
        <input
          id="gm-slider"
          type="range"
          min={0}
          max={255}
          value={grandMasterValue}
          onChange={onGMChange}
        />
        <span className="gm-value">{Math.round((grandMasterValue / 255) * 100)}%</span>
      </footer>
    </div>
  );
}
