import { memo, useCallback, useRef } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import type { CapabilityInfo } from '../../lib/dmx-types';
import { clamp, haptic } from '../../lib/utils';

interface Props {
  fixtureId: number;
  channelIndex: number;
  name: string;
  min?: number;
  max?: number;
  capabilities?: CapabilityInfo[];
  height?: number;
  variant?: 'default' | 'dimmer';
  displayValue?: (raw: number) => string;
  accent?: string;
}

function ChannelFaderImpl({
  fixtureId,
  channelIndex,
  name,
  min = 0,
  max = 255,
  capabilities,
  height = 200,
  variant = 'default',
  displayValue,
  accent,
}: Props) {
  const setChannel = useDmxStore(s => s.setChannel);
  const setActive = useDmxStore(s => s.setActiveChannel);
  const clearActive = useDmxStore(s => s.clearActiveChannel);
  const value = useDmxStore(s => s.liveValues.get(fixtureId)?.[channelIndex] ?? 0);

  const trackRef = useRef<HTMLDivElement>(null);
  const draggingRef = useRef(false);

  const updateFromEvent = useCallback((e: React.PointerEvent) => {
    const el = trackRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const pct = clamp(1 - (e.clientY - r.top) / r.height, 0, 1);
    const v = Math.round(min + pct * (max - min));
    setChannel(fixtureId, channelIndex, v);
  }, [fixtureId, channelIndex, min, max, setChannel]);

  const onDown = (e: React.PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    draggingRef.current = true;
    setActive(fixtureId, channelIndex);
    haptic();
    updateFromEvent(e);
  };
  const onMove = (e: React.PointerEvent) => {
    if (!draggingRef.current) return;
    updateFromEvent(e);
  };
  const onUp = (e: React.PointerEvent) => {
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    draggingRef.current = false;
    clearActive();
  };

  const pct = max > min ? (value - min) / (max - min) : 0;
  const readout = displayValue ? displayValue(value) : String(value);
  const style: React.CSSProperties = accent ? ({ ['--fader-accent' as never]: accent } as React.CSSProperties) : {};

  return (
    <div className={`channel-fader${variant === 'dimmer' ? ' dimmer-fader' : ''}`} style={style}>
      <div className="cf-value" aria-live="off">{readout}</div>
      <div
        ref={trackRef}
        className="cf-track"
        style={{ height }}
        onPointerDown={onDown}
        onPointerMove={onMove}
        onPointerUp={onUp}
        onPointerCancel={onUp}
        role="slider"
        aria-label={name}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={value}
      >
        <div className="cf-fill" style={{ height: `${pct * 100}%` }} />
        {capabilities?.length ? (
          <div className="cf-ticks" aria-hidden="true">
            {capabilities.map((c, i) => {
              const p = (c.min + c.max) / 2 / 255;
              return (
                <div
                  key={i}
                  className="cf-tick"
                  style={{ bottom: `${p * 100}%` }}
                  title={`${c.name} (${c.min}-${c.max})`}
                >
                  <span className="cf-tick-label">{c.name}</span>
                </div>
              );
            })}
          </div>
        ) : null}
        <div className="cf-thumb" style={{ bottom: `calc(${pct * 100}% - 8px)` }} />
      </div>
      <div className="cf-name" title={name}>{name}</div>
    </div>
  );
}

export default memo(ChannelFaderImpl);
