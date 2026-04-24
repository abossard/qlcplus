// Virtual Console view — extracted from App.tsx so the shell can host
// multiple top-level views (VC + DMX). Behaviour is identical to the
// original implementation: page tabs, scaled viewport.

import { useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useVCStore } from '../store/vc-store';
import { PageView } from '../components/PageView';
import type { PageData } from '../lib/types';

function computePageSize(page: PageData | undefined): { w: number; h: number } {
  if (!page || !page.children?.length) return { w: 1280, h: 720 };
  let maxX = 0;
  let maxY = 0;
  for (const w of page.children) {
    const g = w.geometry;
    if (!g) continue;
    maxX = Math.max(maxX, g.x + g.w);
    maxY = Math.max(maxY, g.y + g.h);
  }
  return { w: Math.max(maxX, 320), h: Math.max(maxY, 240) };
}

export default function VCView() {
  const projectLoaded = useVCStore(s => s.projectLoaded);
  const pages = useVCStore(s => s.pages);
  const selectedPage = useVCStore(s => s.selectedPage);
  const setSelectedPage = useVCStore(s => s.setSelectedPage);

  const viewportRef = useRef<HTMLDivElement>(null);
  const [scale, setScale] = useState(1);
  const [offsetX, setOffsetX] = useState(0);
  const [offsetY, setOffsetY] = useState(0);

  const currentPage = pages[selectedPage];
  const pageSize = useMemo(() => computePageSize(currentPage), [currentPage]);

  useLayoutEffect(() => {
    const el = viewportRef.current;
    if (!el) return;
    const measure = () => {
      const rect = el.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) return;
      const pad = 12;
      const usableW = Math.max(1, rect.width - pad * 2);
      const usableH = Math.max(1, rect.height - pad * 2);
      const sx = usableW / pageSize.w;
      const sy = usableH / pageSize.h;
      const s = Math.min(sx, sy, 1.5);
      setScale(s);
      setOffsetX(Math.max(pad, (rect.width - pageSize.w * s) / 2));
      setOffsetY(Math.max(pad, (rect.height - pageSize.h * s) / 2));
    };
    measure();
    const ro = new ResizeObserver(measure);
    ro.observe(el);
    return () => ro.disconnect();
  }, [pageSize.w, pageSize.h]);

  return (
    <div className="vc-view">
      {pages.length > 0 && (
        <nav className="page-tabs vc-page-tabs" role="tablist" aria-label="Virtual Console pages">
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
      )}

      <div className="viewport" ref={viewportRef}>
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
              ? projectLoaded
                ? 'No pages in project'
                : 'Loading Virtual Console…'
              : 'Select a page'}
          </div>
        )}
      </div>
    </div>
  );
}
