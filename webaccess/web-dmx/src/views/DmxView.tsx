// DMX Control Panel view — Phase B foundation.
// Loads fixtures, attaches DMX WS subscriptions, and renders a grid of
// fixture cards (controls themselves arrive in Phase C/D).

import { useCallback, useEffect, useMemo, useState } from 'react';
import { useDmxStore } from '../store/dmx-store';
import { useVCStore } from '../store/vc-store';
import FixturePanel from '../components/dmx/FixturePanel';
import type { FixtureInfo } from '../lib/dmx-types';

const COLLAPSED_KEY = 'qlcplus.dmx.collapsed-universes';

function fixtureModelLabel(fx: FixtureInfo): string {
  return fx.model && fx.model.trim() ? fx.model : fx.name;
}

function uniqueModels(fixtures: Map<number, FixtureInfo>): string[] {
  const s = new Set<string>();
  for (const fx of fixtures.values()) s.add(fixtureModelLabel(fx));
  return Array.from(s).sort();
}

function uniqueUniverses(fixtures: Map<number, FixtureInfo>): number[] {
  const s = new Set<number>();
  for (const fx of fixtures.values()) s.add(fx.universe);
  return Array.from(s).sort((a, b) => a - b);
}

function loadCollapsed(): Set<number> {
  try {
    const raw = localStorage.getItem(COLLAPSED_KEY);
    if (!raw) return new Set();
    const arr = JSON.parse(raw);
    if (Array.isArray(arr)) return new Set(arr.filter((n: unknown) => typeof n === 'number'));
  } catch { /* noop */ }
  return new Set();
}

function persistCollapsed(set: Set<number>) {
  try {
    localStorage.setItem(COLLAPSED_KEY, JSON.stringify(Array.from(set)));
  } catch { /* noop */ }
}

export default function DmxView() {
  const connected = useVCStore(s => s.connected);
  const fixtures = useDmxStore(s => s.fixtures);
  const loading = useDmxStore(s => s.loading);
  const error = useDmxStore(s => s.error);
  const loadFixtures = useDmxStore(s => s.loadFixtures);
  const attachWS = useDmxStore(s => s.attachWS);
  const subscribeFixtures = useDmxStore(s => s.subscribeFixtures);
  const unsubscribeFixtures = useDmxStore(s => s.unsubscribeFixtures);
  const [filter, setFilter] = useState('');
  const [modelFilter, setModelFilter] = useState<string | null>(null);
  const [groupBy, setGroupBy] = useState<'none' | 'universe'>('none');
  const [collapsedUniverses, setCollapsedUniverses] = useState<Set<number>>(() => loadCollapsed());

  useEffect(() => {
    void loadFixtures();
  }, [loadFixtures]);

  useEffect(() => {
    if (!connected) return;
    const detach = attachWS();
    return () => detach();
  }, [attachWS, connected]);

  useEffect(() => {
    const ids = Array.from(fixtures.keys());
    if (!connected || !ids.length) return;
    subscribeFixtures(ids);
    // Send periodic heartbeats to prevent 30s timeout cleanup.
    const hbInterval = setInterval(() => {
      subscribeFixtures([]); // empty subscribe acts as heartbeat
    }, 15_000);
    // Resubscribe when tab regains focus (browsers throttle setInterval
    // in background tabs, which can miss the 30s server TTL).
    const onVisible = () => {
      if (document.visibilityState === 'visible') subscribeFixtures(ids);
    };
    document.addEventListener('visibilitychange', onVisible);
    return () => {
      clearInterval(hbInterval);
      document.removeEventListener('visibilitychange', onVisible);
      unsubscribeFixtures(ids);
    };
  }, [connected, fixtures, subscribeFixtures, unsubscribeFixtures]);

  const toggleUniverse = useCallback((u: number) => {
    setCollapsedUniverses(prev => {
      const next = new Set(prev);
      if (next.has(u)) next.delete(u); else next.add(u);
      persistCollapsed(next);
      return next;
    });
  }, []);

  const models = useMemo(() => uniqueModels(fixtures), [fixtures]);
  const universes = useMemo(() => uniqueUniverses(fixtures), [fixtures]);

  const sorted = useMemo(() => {
    const needle = filter.trim().toLowerCase();
    return Array.from(fixtures.values())
      .filter(fx => {
        if (modelFilter && fixtureModelLabel(fx) !== modelFilter) return false;
        if (!needle) return true;
        return fx.name.toLowerCase().includes(needle)
          || (fx.model?.toLowerCase().includes(needle) ?? false)
          || (fx.manufacturer?.toLowerCase().includes(needle) ?? false);
      })
      .sort((a, b) => {
        if (a.universe !== b.universe) return a.universe - b.universe;
        return a.address - b.address;
      });
  }, [fixtures, filter, modelFilter]);

  if (loading && sorted.length === 0) {
    return <div className="dmx-view dmx-view-empty">Loading fixtures…</div>;
  }
  if (error) {
    return (
      <div className="dmx-view dmx-view-empty">
        <div className="dmx-error">Failed to load fixtures: {error}</div>
        <button type="button" className="dmx-retry" onClick={() => void loadFixtures()}>
          Retry
        </button>
      </div>
    );
  }
  const totalFixtures = fixtures.size;
  if (totalFixtures === 0) {
    return <div className="dmx-view dmx-view-empty">No fixtures patched.</div>;
  }

  const grouped: { label: string; universe: number | null; items: FixtureInfo[] }[] =
    groupBy === 'universe'
      ? universes
          .map(u => ({
            label: `Universe ${u + 1}`,
            universe: u,
            items: sorted.filter(fx => fx.universe === u),
          }))
          .filter(g => g.items.length > 0)
      : [{ label: '', universe: null, items: sorted }];

  return (
    <div className="dmx-view" role="region" aria-label="DMX Control Panel">
      <div className="dmx-toolbar">
        <input
          type="search"
          className="dmx-search"
          placeholder="Filter fixtures…"
          value={filter}
          onChange={e => setFilter(e.target.value)}
          aria-label="Filter fixtures"
        />
        {models.length > 1 && (
          <div className="dmx-type-badges">
            <button
              type="button"
              className={`dmx-badge${modelFilter === null ? ' active' : ''}`}
              onClick={() => setModelFilter(null)}
            >All</button>
            {models.map(m => (
              <button
                key={m}
                type="button"
                className={`dmx-badge${modelFilter === m ? ' active' : ''}`}
                onClick={() => setModelFilter(modelFilter === m ? null : m)}
                title={m}
              >{m}</button>
            ))}
          </div>
        )}
        <div className="dmx-toolbar-right">
          {universes.length > 1 && (
            <button
              type="button"
              className={`dmx-group-btn${groupBy === 'universe' ? ' active' : ''}`}
              onClick={() => setGroupBy(g => g === 'universe' ? 'none' : 'universe')}
              title="Group by universe"
            >⊞</button>
          )}
          <span className="dmx-count">{sorted.length} / {totalFixtures}</span>
        </div>
      </div>
      {grouped.map(({ label, universe, items }) => {
        const collapsible = universe !== null;
        const collapsed = collapsible && collapsedUniverses.has(universe);
        return (
          <div key={label || 'all'} className="dmx-group">
            {label && (
              collapsible ? (
                <h2
                  className={`dmx-group-label dmx-group-label-toggle${collapsed ? ' collapsed' : ''}`}
                  role="button"
                  tabIndex={0}
                  aria-expanded={!collapsed}
                  onClick={() => toggleUniverse(universe!)}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter' || e.key === ' ') {
                      e.preventDefault();
                      toggleUniverse(universe!);
                    }
                  }}
                >
                  <span className="dmx-group-chevron" aria-hidden="true">{collapsed ? '▸' : '▾'}</span>
                  {' '}{label}{' '}
                  <span className="dmx-group-count">({items.length} fixture{items.length === 1 ? '' : 's'})</span>
                </h2>
              ) : (
                <h2 className="dmx-group-label">{label}</h2>
              )
            )}
            {!collapsed && (
              <div className="dmx-grid">
                {items.map(fx => (
                  <FixturePanel key={fx.id} fixture={fx} />
                ))}
              </div>
            )}
          </div>
        );
      })}
    </div>
  );
}
