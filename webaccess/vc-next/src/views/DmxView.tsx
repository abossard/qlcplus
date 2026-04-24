// DMX Control Panel view — Phase B foundation.
// Loads fixtures, attaches DMX WS subscriptions, and renders a grid of
// fixture cards (controls themselves arrive in Phase C/D).

import { memo, useEffect, useMemo } from 'react';
import { useDmxStore } from '../store/dmx-store';
import type { FixtureInfo, ControlPlan } from '../lib/dmx-types';

function planSummary(plans: ControlPlan[]): string {
  const counts = new Map<string, number>();
  for (const p of plans) counts.set(p.kind, (counts.get(p.kind) ?? 0) + 1);
  return Array.from(counts.entries())
    .map(([k, v]) => `${v}× ${k}`)
    .join(', ');
}

interface CardProps {
  fixture: FixtureInfo;
  plans: ControlPlan[];
}

const FixtureCard = memo(function FixtureCard({ fixture, plans }: CardProps) {
  const summary = plans.length ? planSummary(plans) : 'no controls';
  const subtitle = [fixture.manufacturer, fixture.model, fixture.mode]
    .filter(Boolean)
    .join(' · ');
  return (
    <article className="fixture-card" aria-label={fixture.name}>
      <header className="fixture-card-header">
        <h3 className="fixture-card-title">{fixture.name}</h3>
        <span className="fixture-card-addr">U{fixture.universe + 1} · {fixture.address + 1}</span>
      </header>
      {subtitle && <div className="fixture-card-sub">{subtitle}</div>}
      <div className="fixture-card-meta">
        <span>{fixture.channels} ch</span>
        {fixture.type && <span>{fixture.type}</span>}
        {fixture.headMap?.length ? (
          <span>
            {fixture.headMap.length} head{fixture.headMap.length > 1 ? 's' : ''}
          </span>
        ) : null}
      </div>
      <div className="fixture-card-plans">{summary}</div>
    </article>
  );
});

export default function DmxView() {
  const fixtures = useDmxStore(s => s.fixtures);
  const plans = useDmxStore(s => s.plans);
  const loading = useDmxStore(s => s.loading);
  const error = useDmxStore(s => s.error);
  const loadFixtures = useDmxStore(s => s.loadFixtures);
  const attachWS = useDmxStore(s => s.attachWS);
  const subscribeFixtures = useDmxStore(s => s.subscribeFixtures);
  const unsubscribeFixtures = useDmxStore(s => s.unsubscribeFixtures);

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
    return Array.from(fixtures.values()).sort((a, b) => {
      if (a.universe !== b.universe) return a.universe - b.universe;
      return a.address - b.address;
    });
  }, [fixtures]);

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
  if (sorted.length === 0) {
    return <div className="dmx-view dmx-view-empty">No fixtures patched.</div>;
  }

  return (
    <div className="dmx-view" role="region" aria-label="DMX Control Panel">
      <div className="dmx-grid">
        {sorted.map(fx => (
          <FixtureCard key={fx.id} fixture={fx} plans={plans.get(fx.id) ?? []} />
        ))}
      </div>
    </div>
  );
}
