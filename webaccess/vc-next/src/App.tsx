import { useEffect, useLayoutEffect, useRef, useState, useMemo } from 'react';
import { useVCStore } from './store/vc-store';
import { PageView } from './components/PageView';
import type { PageData } from './lib/types';

function computePageSize(page: PageData | undefined): { w: number; h: number } {
  if (!page || !page.children?.length) return { w: 1280, h: 720 };
  let maxX = 0, maxY = 0;
  for (const w of page.children) {
    const g = w.geometry;
    if (!g) continue;
    maxX = Math.max(maxX, g.x + g.w);
    maxY = Math.max(maxY, g.y + g.h);
  }
  return { w: Math.max(maxX, 320), h: Math.max(maxY, 240) };
}

export default function App() {
  const connected = useVCStore(s => s.connected);
  const projectLoaded = useVCStore(s => s.projectLoaded);
  const pages = useVCStore(s => s.pages);
  const selectedPage = useVCStore(s => s.selectedPage);
  const grandMasterValue = useVCStore(s => s.grandMasterValue);
  const init = useVCStore(s => s.init);
  const cleanup = useVCStore(s => s.cleanup);
  const setSelectedPage = useVCStore(s => s.setSelectedPage);

  const viewportRef = useRef<HTMLDivElement>(null);
  const [scale, setScale] = useState(1);
  const [offsetX, setOffsetX] = useState(0);
  const [offsetY, setOffsetY] = useState(0);
  const [compact, setCompact] = useState(false);

  useEffect(() => {
    init();
    return () => cleanup();
  }, [init, cleanup]);

  const currentPage = pages[selectedPage];
  const pageSize = useMemo(() => computePageSize(currentPage), [currentPage]);

  useLayoutEffect(() => {
    const el = viewportRef.current;
    if (!el) return;
    const measure = () => {
      const rect = el.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) return;
      // Reserve a small padding so widgets never kiss the viewport edge.
      const pad = 12;
      const usableW = Math.max(1, rect.width - pad * 2);
      const usableH = Math.max(1, rect.height - pad * 2);
      const sx = usableW / pageSize.w;
      const sy = usableH / pageSize.h;
      const s = Math.min(sx, sy, 1.5);
      setScale(s);
      // Center the scaled content horizontally and vertically.
      setOffsetX(Math.max(pad, (rect.width - pageSize.w * s) / 2));
      setOffsetY(Math.max(pad, (rect.height - pageSize.h * s) / 2));
    };
    measure();
    const ro = new ResizeObserver(measure);
    ro.observe(el);
    return () => ro.disconnect();
  }, [pageSize.w, pageSize.h]);

  function onGMChange(e: React.ChangeEvent<HTMLInputElement>) {
    const v = parseInt(e.target.value, 10);
    useVCStore.setState({ grandMasterValue: v });
    useVCStore.getState().wsClient?.send(`GM_VALUE|${v}`);
  }

  // Status text: Live (WS connected), Demo Mode (data loaded but no WS — static fallback),
  // Waiting… (WS connected, no data yet), or Offline.
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
        <nav className="page-tabs" role="tablist" aria-label="Virtual Console pages">
          {pages.map((p, idx) => (
            <button
              key={p.id ?? idx}
              className={`page-tab${idx === selectedPage ? ' active' : ''}`}
              role="tab"
              aria-selected={idx === selectedPage}
              onClick={() => setSelectedPage(idx)}
            >
              {p.caption || `Page ${idx + 1}`}
            </button>
          ))}
        </nav>
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

      <main className="viewport" ref={viewportRef}>
        {currentPage ? (
          <div
            className="scale-layer"
            style={{
              width: pageSize.w,
              height: pageSize.h,
              transform: `translate(${offsetX}px, ${offsetY}px) scale(${scale})`,
            }}
          >
            <PageView page={currentPage} />
          </div>
        ) : (
          <div className="empty-state">
            {pages.length === 0
              ? (projectLoaded ? 'No pages in project' : 'Loading Virtual Console…')
              : 'Select a page'}
          </div>
        )}
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
