import { memo, useCallback, useRef } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import { clamp, haptic } from '../../lib/utils';

interface Axis { coarse: number; fine?: number }

interface Props {
  fixtureId: number;
  pan: Axis;
  tilt: Axis;
  panMax?: number;
  tiltMax?: number;
}

function composeValue(coarse: number, fine: number | undefined, has16: boolean): number {
  if (has16) return (coarse << 8) | (fine ?? 0);
  return coarse;
}

function splitValue(value: number, has16: boolean): [number, number | undefined] {
  if (has16) {
    const v = clamp(value, 0, 65535) | 0;
    return [(v >> 8) & 0xff, v & 0xff];
  }
  return [clamp(value, 0, 255) | 0, undefined];
}

function PositionControlImpl({ fixtureId, pan, tilt, panMax = 540, tiltMax = 270 }: Props) {
  const setChannels = useDmxStore(s => s.setChannels);
  const setActive = useDmxStore(s => s.setActiveChannel);
  const clearActive = useDmxStore(s => s.clearActiveChannel);
  const live = useDmxStore(s => s.liveValues.get(fixtureId));

  const panHas16 = pan.fine !== undefined;
  const tiltHas16 = tilt.fine !== undefined;
  const panRange = panHas16 ? 65535 : 255;
  const tiltRange = tiltHas16 ? 65535 : 255;

  const panRaw = composeValue(live?.[pan.coarse] ?? 0, pan.fine !== undefined ? live?.[pan.fine] ?? 0 : undefined, panHas16);
  const tiltRaw = composeValue(live?.[tilt.coarse] ?? 0, tilt.fine !== undefined ? live?.[tilt.fine] ?? 0 : undefined, tiltHas16);
  const panPct = panRaw / panRange;
  const tiltPct = tiltRaw / tiltRange;

  const padRef = useRef<HTMLDivElement>(null);
  const draggingPad = useRef(false);

  const writePan = useCallback((raw: number) => {
    const [c, f] = splitValue(raw, panHas16);
    const writes: [number, number][] = [[pan.coarse, c]];
    if (panHas16 && pan.fine !== undefined) writes.push([pan.fine, f!]);
    setChannels(fixtureId, writes);
  }, [fixtureId, pan.coarse, pan.fine, panHas16, setChannels]);

  const writeTilt = useCallback((raw: number) => {
    const [c, f] = splitValue(raw, tiltHas16);
    const writes: [number, number][] = [[tilt.coarse, c]];
    if (tiltHas16 && tilt.fine !== undefined) writes.push([tilt.fine, f!]);
    setChannels(fixtureId, writes);
  }, [fixtureId, tilt.coarse, tilt.fine, tiltHas16, setChannels]);

  const writeBoth = useCallback((panVal: number, tiltVal: number) => {
    const [pc, pf] = splitValue(panVal, panHas16);
    const [tc, tf] = splitValue(tiltVal, tiltHas16);
    const writes: [number, number][] = [[pan.coarse, pc], [tilt.coarse, tc]];
    if (panHas16 && pan.fine !== undefined) writes.push([pan.fine, pf!]);
    if (tiltHas16 && tilt.fine !== undefined) writes.push([tilt.fine, tf!]);
    setChannels(fixtureId, writes);
  }, [fixtureId, pan.coarse, pan.fine, tilt.coarse, tilt.fine, panHas16, tiltHas16, setChannels]);

  const padUpdate = useCallback((e: React.PointerEvent) => {
    const el = padRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const nx = clamp((e.clientX - r.left) / r.width, 0, 1);
    const ny = clamp((e.clientY - r.top) / r.height, 0, 1);
    writeBoth(Math.round(nx * panRange), Math.round(ny * tiltRange));
  }, [writeBoth, panRange, tiltRange]);

  const onPadDown = (e: React.PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    draggingPad.current = true;
    setActive(fixtureId, pan.coarse);
    haptic();
    padUpdate(e);
  };
  const onPadMove = (e: React.PointerEvent) => {
    if (!draggingPad.current) return;
    padUpdate(e);
  };
  const onPadUp = (e: React.PointerEvent) => {
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    draggingPad.current = false;
    clearActive();
  };

  const panDeg = panPct * panMax;
  const tiltDeg = tiltPct * tiltMax;

  return (
    <div className="position-control">
      <div
        ref={padRef}
        className="pc-pad"
        onPointerDown={onPadDown}
        onPointerMove={onPadMove}
        onPointerUp={onPadUp}
        onPointerCancel={onPadUp}
        role="slider"
        aria-label="pan tilt"
      >
        <div className="pc-grid-v" style={{ left: '25%' }} />
        <div className="pc-grid-v" style={{ left: '50%' }} />
        <div className="pc-grid-v" style={{ left: '75%' }} />
        <div className="pc-grid-h" style={{ top: '25%' }} />
        <div className="pc-grid-h" style={{ top: '50%' }} />
        <div className="pc-grid-h" style={{ top: '75%' }} />
        <div
          className="pc-cursor"
          style={{ left: `${panPct * 100}%`, top: `${tiltPct * 100}%` }}
        />
      </div>
      <div className="pc-display">
        Pan {panDeg.toFixed(0)}° · Tilt {tiltDeg.toFixed(0)}°
      </div>
      <HorizontalDegSlider
        label="Pan"
        maxDeg={panMax}
        valueRaw={panRaw}
        rangeMax={panRange}
        onChange={writePan}
        onStart={() => setActive(fixtureId, pan.coarse)}
        onEnd={clearActive}
      />
      <HorizontalDegSlider
        label="Tilt"
        maxDeg={tiltMax}
        valueRaw={tiltRaw}
        rangeMax={tiltRange}
        onChange={writeTilt}
        onStart={() => setActive(fixtureId, tilt.coarse)}
        onEnd={clearActive}
      />
    </div>
  );
}

interface HDSProps {
  label: string;
  maxDeg: number;
  valueRaw: number;
  rangeMax: number;
  onChange: (raw: number) => void;
  onStart: () => void;
  onEnd: () => void;
}

function HorizontalDegSlider({ label, maxDeg, valueRaw, rangeMax, onChange, onStart, onEnd }: HDSProps) {
  const ref = useRef<HTMLDivElement>(null);
  const draggingRef = useRef(false);
  const pct = valueRaw / rangeMax;

  const update = (e: React.PointerEvent) => {
    const el = ref.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const p = clamp((e.clientX - r.left) / r.width, 0, 1);
    onChange(Math.round(p * rangeMax));
  };

  return (
    <div className="pc-hslider">
      <span className="pc-hlabel">{label}</span>
      <div
        ref={ref}
        className="pc-htrack"
        onPointerDown={(e) => {
          (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
          draggingRef.current = true;
          onStart();
          update(e);
        }}
        onPointerMove={(e) => { if (draggingRef.current) update(e); }}
        onPointerUp={(e) => {
          try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
          draggingRef.current = false;
          onEnd();
        }}
        onPointerCancel={() => { draggingRef.current = false; onEnd(); }}
        role="slider"
        aria-label={label}
        aria-valuemin={0}
        aria-valuemax={Math.round(maxDeg)}
        aria-valuenow={Math.round(pct * maxDeg)}
      >
        <div className="pc-hfill" style={{ width: `${pct * 100}%` }} />
        <div className="pc-hthumb" style={{ left: `calc(${pct * 100}% - 8px)` }} />
      </div>
      <span className="pc-hvalue">{(pct * maxDeg).toFixed(0)}°</span>
    </div>
  );
}

export default memo(PositionControlImpl);
