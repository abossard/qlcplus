import { memo, useMemo, useRef } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import type { CapabilityInfo } from '../../lib/dmx-types';
import { clamp } from '../../lib/utils';

interface Props {
  fixtureId: number;
  channelIndex: number;
  capabilities: CapabilityInfo[];
}

type Bucket = 'open' | 'closed' | 'strobe' | 'other';

function classify(cap: CapabilityInfo): Bucket {
  const n = cap.name.toLowerCase();
  if (n.includes('strob') || n.includes('pulse') || n.includes('flash') || n.includes('random')) return 'strobe';
  if (n.includes('close') || n.includes('blackout') || n.includes('off')) return 'closed';
  if (n.includes('open') || n === 'on') return 'open';
  return 'other';
}

function ShutterControlImpl({ fixtureId, channelIndex, capabilities }: Props) {
  const setChannel = useDmxStore(s => s.setChannel);
  const setActive = useDmxStore(s => s.setActiveChannel);
  const clearActive = useDmxStore(s => s.clearActiveChannel);
  const value = useDmxStore(s => s.liveValues.get(fixtureId)?.[channelIndex] ?? 0);

  const { opens, closeds, strobes, others } = useMemo(() => {
    const o: CapabilityInfo[] = [], c: CapabilityInfo[] = [], s: CapabilityInfo[] = [], x: CapabilityInfo[] = [];
    for (const cap of capabilities) {
      const b = classify(cap);
      if (b === 'open') o.push(cap);
      else if (b === 'closed') c.push(cap);
      else if (b === 'strobe') s.push(cap);
      else x.push(cap);
    }
    return { opens: o, closeds: c, strobes: s, others: x };
  }, [capabilities]);

  const activeCap = capabilities.find(c => value >= c.min && value <= c.max);
  const activeStrobe = activeCap && classify(activeCap) === 'strobe' ? activeCap : null;

  const strobeTrackRef = useRef<HTMLDivElement>(null);
  const draggingStrobe = useRef(false);

  const setMid = (cap: CapabilityInfo) => setChannel(fixtureId, channelIndex, Math.floor((cap.min + cap.max) / 2));

  const updateStrobe = (e: React.PointerEvent, cap: CapabilityInfo) => {
    const el = strobeTrackRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const p = clamp((e.clientX - r.left) / r.width, 0, 1);
    const v = Math.round(cap.min + p * (cap.max - cap.min));
    setChannel(fixtureId, channelIndex, v);
  };

  const renderRadio = (cap: CapabilityInfo) => {
    const active = activeCap === cap;
    return (
      <button
        key={`${cap.min}-${cap.max}`}
        type="button"
        className={`sc-radio${active ? ' active' : ''}`}
        onClick={() => setMid(cap)}
        aria-pressed={active}
      >
        {cap.name}
      </button>
    );
  };

  // Use the first strobe capability as the speed-sweep range.
  const strobeCap = strobes[0];
  const strobePct = strobeCap && activeStrobe === strobeCap
    ? (value - strobeCap.min) / Math.max(1, strobeCap.max - strobeCap.min)
    : 0;

  return (
    <div className="shutter-control">
      <div className="sc-group">
        <span className="sc-glabel">State</span>
        <div className="sc-row">
          {opens.map(renderRadio)}
          {closeds.map(renderRadio)}
          {others.map(renderRadio)}
        </div>
      </div>
      {strobes.length > 0 && (
        <div className="sc-group">
          <span className="sc-glabel">Strobe</span>
          <div className="sc-row">
            {strobes.map(renderRadio)}
          </div>
          {strobeCap && (
            <div
              ref={strobeTrackRef}
              className="sc-speed"
              onPointerDown={(e) => {
                (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
                draggingStrobe.current = true;
                setActive(fixtureId, channelIndex);
                updateStrobe(e, strobeCap);
              }}
              onPointerMove={(e) => { if (draggingStrobe.current) updateStrobe(e, strobeCap); }}
              onPointerUp={(e) => {
                try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
                draggingStrobe.current = false;
                clearActive();
              }}
              onPointerCancel={() => { draggingStrobe.current = false; clearActive(); }}
              role="slider"
              aria-label="strobe speed"
              aria-valuemin={strobeCap.min}
              aria-valuemax={strobeCap.max}
              aria-valuenow={value}
            >
              <div className="sc-speed-fill" style={{ width: `${strobePct * 100}%` }} />
              <span className="sc-speed-label">Speed</span>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default memo(ShutterControlImpl);
