// DMX Control Panel view — Phase B foundation.
// Loads fixtures, attaches DMX WS subscriptions, and renders a grid of
// fixture cards (controls themselves arrive in Phase C/D).

import { useEffect, useMemo, useState } from 'react';
import { useDmxStore } from '../store/dmx-store';
import FixturePanel from '../components/dmx/FixturePanel';
import type { FixtureInfo } from '../lib/dmx-types';

function uniqueTypes(fixtures: Map<number, FixtureInfo>): string[] {
  const s = new Set<string>();
  for (const fx of fixtures.values()) if (fx.type) s.add(fx.type);
  return Array.from(s).sort();
}

function uniqueUniverses(fixtures: Map<number, FixtureInfo>): number[] {
  const s = new Set<number>();
  for (const fx of fixtures.values()) s.add(fx.universe);
  return Array.from(s).sort((a, b) => a - b);
}

export default function DmxView() {
  const fixtures = useDmxStore(s => s.fixtures);
  const loading = useDmxStore(s => s.loading);
  const error = useDmxStore(s => s.error);
  const loadFixtures = useDmxStore(s => s.loadFixtures);
  const attachWS = useDmxStore(s => s.attachWS);
  const subscribeFixtures = useDmxStore(s => s.subscribeFixtures);
  const unsubscribeFixtures = useDmxStore(s => s.unsubscribeFixtures);
  const [filter, setFilter] = useState('');
  const [typeFilter, setTypeFilter] = useState<string | null>(null);
  const [groupBy, setGroupBy] = useState<'none' | 'universe'>('none');

  useEffect(() => {
    void loadFixtures();
  }, [loadFixtures]);

  useEffect(() => {
    const detach = attachWS();
    return () => detach();
  }, [attachWS]);

  useEffect(() => {
    const ids = Array.from(fixtures.keys());
    if (!ids.length) return;
    subscribeFixtures(ids);
    return () => unsubscribeFixtures(ids);
  }, [fixtures, subscribeFixtures, unsubscribeFixtures]);

  const types = useMemo(() => uniqueTypes(fixtures), [fixtures]);
  const universes = useMemo(() => uniqueUniverses(fixtures), [fixtures]);

  const sorted = useMemo(() => {
    const needle = filter.trim().toLowerCase();
    return Array.from(fixtures.values())
      .filter(fx => {
        if (typeFilter && fx.type !== typeFilter) return false;
        if (!needle) return true;
        return fx.name.toLowerCase().includes(needle)
          || (fx.model?.toLowerCase().includes(needle) ?? false)
          || (fx.manufacturer?.toLowerCase().includes(needle) ?? false);
      })
      .sort((a, b) => {
        if (a.universe !== b.universe) return a.universe - b.universe;
        return a.address - b.address;
      });
  }, [fixtures, filter, typeFilter]);

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

  const grouped = groupBy === 'universe'
    ? universes.map(u => ({
        label: `Universe ${u + 1}`,
        items: sorted.filter(fx => fx.universe === u),
      })).filter(g => g.items.length > 0)
    : [{ label: '', items: sorted }];

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
        {types.length > 1 && (
          <div className="dmx-type-badges">
            <button
              type="button"
              className={`dmx-badge${typeFilter === null ? ' active' : ''}`}
              onClick={() => setTypeFilter(null)}
            >All</button>
            {types.map(t => (
              <button
                key={t}
                type="button"
                className={`dmx-badge${typeFilter === t ? ' active' : ''}`}
                onClick={() => setTypeFilter(typeFilter === t ? null : t)}
              >{t}</button>
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
      {grouped.map(({ label, items }) => (
        <div key={label || 'all'} className="dmx-group">
          {label && <h2 className="dmx-group-label">{label}</h2>}
          <div className="dmx-grid">
            {items.map(fx => (
              <FixturePanel key={fx.id} fixture={fx} />
            ))}
          </div>
        </div>
      ))}
    </div>
  );
}
