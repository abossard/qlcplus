import { memo, useCallback, useEffect, useRef, useState } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import ChannelFader from './ChannelFader';
import { clamp, haptic } from '../../lib/utils';

interface ExtraChannel {
  channel: number;
  colour: string;
  fine?: number;
}

interface Props {
  fixtureId: number;
  rgbChannels: number[];
  extraChannels?: ExtraChannel[];
}

interface HSV { h: number; s: number; v: number }

function rgbToHsv(r: number, g: number, b: number): HSV {
  const rn = r / 255, gn = g / 255, bn = b / 255;
  const max = Math.max(rn, gn, bn), min = Math.min(rn, gn, bn);
  const d = max - min;
  let h = 0;
  if (d !== 0) {
    switch (max) {
      case rn: h = ((gn - bn) / d) % 6; break;
      case gn: h = (bn - rn) / d + 2; break;
      default: h = (rn - gn) / d + 4;
    }
    h *= 60;
    if (h < 0) h += 360;
  }
  const s = max === 0 ? 0 : d / max;
  return { h, s, v: max };
}

function hsvToRgb(h: number, s: number, v: number): [number, number, number] {
  const c = v * s;
  const hp = h / 60;
  const x = c * (1 - Math.abs((hp % 2) - 1));
  let r = 0, g = 0, b = 0;
  if (hp >= 0 && hp < 1) { r = c; g = x; }
  else if (hp < 2) { r = x; g = c; }
  else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; }
  else if (hp < 5) { r = x; b = c; }
  else { r = c; b = x; }
  const m = v - c;
  return [
    Math.round((r + m) * 255),
    Math.round((g + m) * 255),
    Math.round((b + m) * 255),
  ];
}

const EXTRA_ACCENT: Record<string, string> = {
  White: '#f5f5f5',
  Amber: '#f59e0b',
  UV: '#7c3aed',
  Lime: '#a3e635',
  Indigo: '#6366f1',
};

function ColorPickerImpl({ fixtureId, rgbChannels, extraChannels = [] }: Props) {
  const setChannels = useDmxStore(s => s.setChannels);
  const setActive = useDmxStore(s => s.setActiveChannel);
  const clearActive = useDmxStore(s => s.clearActiveChannel);
  const live = useDmxStore(s => s.liveValues.get(fixtureId));

  const r = live?.[rgbChannels[0]] ?? 0;
  const g = live?.[rgbChannels[1]] ?? 0;
  const b = live?.[rgbChannels[2]] ?? 0;

  // Maintain local hue because converting grey/near-grey RGB back to HSV loses it.
  const [hue, setHue] = useState(() => rgbToHsv(r, g, b).h);
  const syncedRef = useRef(false);

  // One-time sync on mount when live values arrive from server.
  useEffect(() => {
    if (syncedRef.current) return;
    if (r === 0 && g === 0 && b === 0) return;
    syncedRef.current = true;
    setHue(rgbToHsv(r, g, b).h);
  }, [r, g, b]);

  const currentSV = rgbToHsv(r, g, b);
  const squareX = currentSV.s;
  const squareY = 1 - currentSV.v;

  const squareRef = useRef<HTMLDivElement>(null);
  const hueRef = useRef<HTMLDivElement>(null);
  const draggingSquare = useRef(false);
  const draggingHue = useRef(false);

  const writeRGB = useCallback((rr: number, gg: number, bb: number) => {
    setChannels(fixtureId, [
      [rgbChannels[0], rr],
      [rgbChannels[1], gg],
      [rgbChannels[2], bb],
    ]);
  }, [fixtureId, rgbChannels, setChannels]);

  const onSquareDown = (e: React.PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    draggingSquare.current = true;
    setActive(fixtureId, rgbChannels[0]);
    haptic();
    onSquareMove(e);
  };
  const onSquareMove = (e: React.PointerEvent) => {
    if (!draggingSquare.current) return;
    const el = squareRef.current;
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const x = clamp((e.clientX - rect.left) / rect.width, 0, 1);
    const y = clamp((e.clientY - rect.top) / rect.height, 0, 1);
    const [rr, gg, bb] = hsvToRgb(hue, x, 1 - y);
    writeRGB(rr, gg, bb);
  };
  const onSquareUp = (e: React.PointerEvent) => {
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    draggingSquare.current = false;
    clearActive();
  };

  const onHueDown = (e: React.PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    draggingHue.current = true;
    setActive(fixtureId, rgbChannels[0]);
    haptic();
    onHueMove(e);
  };
  const onHueMove = (e: React.PointerEvent) => {
    if (!draggingHue.current) return;
    const el = hueRef.current;
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const p = clamp((e.clientX - rect.left) / rect.width, 0, 1);
    const h = p * 360;
    setHue(h);
    const [rr, gg, bb] = hsvToRgb(h, Math.max(currentSV.s, 0.001), Math.max(currentSV.v, 0.001));
    writeRGB(rr, gg, bb);
  };
  const onHueUp = (e: React.PointerEvent) => {
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    draggingHue.current = false;
    clearActive();
  };

  const [hueR, hueG, hueB] = hsvToRgb(hue, 1, 1);
  const hueColor = `rgb(${hueR}, ${hueG}, ${hueB})`;
  const swatchColor = `rgb(${r}, ${g}, ${b})`;
  const huePct = (hue / 360) * 100;

  return (
    <div className="color-picker">
      <div className="cp-head">
        <div className="cp-swatch" style={{ background: swatchColor }} aria-label="current color" />
        <div className="cp-rgb-text">R {r} · G {g} · B {b}</div>
      </div>

      <div
        ref={squareRef}
        className="cp-square"
        style={{ background: `linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, ${hueColor})` }}
        onPointerDown={onSquareDown}
        onPointerMove={onSquareMove}
        onPointerUp={onSquareUp}
        onPointerCancel={onSquareUp}
        role="slider"
        aria-label="saturation and brightness"
      >
        <div
          className="cp-crosshair"
          style={{ left: `${squareX * 100}%`, top: `${squareY * 100}%` }}
        />
      </div>

      <div
        ref={hueRef}
        className="cp-hue"
        onPointerDown={onHueDown}
        onPointerMove={onHueMove}
        onPointerUp={onHueUp}
        onPointerCancel={onHueUp}
        role="slider"
        aria-label="hue"
        aria-valuemin={0}
        aria-valuemax={360}
        aria-valuenow={Math.round(hue)}
      >
        <div className="cp-hue-cursor" style={{ left: `${huePct}%` }} />
      </div>

      <div className="cp-channels">
        <ChannelFader fixtureId={fixtureId} channelIndex={rgbChannels[0]} name="R" height={140} accent="#ef4444" />
        <ChannelFader fixtureId={fixtureId} channelIndex={rgbChannels[1]} name="G" height={140} accent="#22c55e" />
        <ChannelFader fixtureId={fixtureId} channelIndex={rgbChannels[2]} name="B" height={140} accent="#3b82f6" />
        {extraChannels.map(ex => (
          <ChannelFader
            key={ex.channel}
            fixtureId={fixtureId}
            channelIndex={ex.channel}
            name={ex.colour}
            height={140}
            accent={EXTRA_ACCENT[ex.colour] ?? 'var(--accent)'}
          />
        ))}
      </div>
    </div>
  );
}

export default memo(ColorPickerImpl);
