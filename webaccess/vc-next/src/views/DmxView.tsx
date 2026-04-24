// DMX Control Panel view — Phase B foundation.
// Loads fixtures, attaches DMX WS subscriptions, and renders a grid of
// fixture cards (controls themselves arrive in Phase C/D).

import { useEffect, useMemo, useState } from 'react';
import { useDmxStore } from '../store/dmx-store';
import FixturePanel from '../components/dmx/FixturePanel';

export default function DmxView() {
  const fixtures = useDmxStore(s => s.fixtures);
  const loading = useDmxStore(s => s.loading);
  const error = useDmxStore(s => s.error);
  const loadFixtures = useDmxStore(s => s.loadFixtures);
  const attachWS = useDmxStore(s => s.attachWS);
  const subscribeFixtures = useDmxStore(s => s.subscribeFixtures);
  const unsubscribeFixtures = useDmxStore(s => s.unsubscribeFixtures);
  const [filter, setFilter] = useState('');

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

  const sorted = useMemo(() => {
    const needle = filter.trim().toLowerCase();
    return Array.from(fixtures.values())
      .filter(fx => !needle || fx.name.toLowerCase().includes(needle)
        || (fx.model?.toLowerCase().includes(needle) ?? false)
        || (fx.manufacturer?.toLowerCase().includes(needle) ?? false))
      .sort((a, b) => {
        if (a.universe !== b.universe) return a.universe - b.universe;
        return a.address - b.address;
      });
  }, [fixtures, filter]);

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
        <span className="dmx-count">{sorted.length} / {totalFixtures}</span>
      </div>
      <div className="dmx-grid">
        {sorted.map(fx => (
          <FixturePanel key={fx.id} fixture={fx} />
        ))}
      </div>
    </div>
  );
}
