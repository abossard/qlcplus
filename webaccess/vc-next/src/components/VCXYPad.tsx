import { memo, useCallback, useEffect, useRef, useState } from 'react';
import type { WidgetData } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { clamp, cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function VCXYPadImpl({ widget }: Props) {
  const wsClient = useVCStore(s => s.wsClient);
  const send = useVCStore(s => s.sendWidgetCommand);
  const canvasRef = useRef<HTMLDivElement>(null);
  const rafRef = useRef<number | null>(null);
  const pendingRef = useRef<{ x: number; y: number } | null>(null);
  const draggingRef = useRef(false);
  const [pos, setPos] = useState({ x: widget.xPos ?? 0.5, y: widget.yPos ?? 0.5 });

  useEffect(() => {
    if (!draggingRef.current) {
      setPos({ x: widget.xPos ?? 0.5, y: widget.yPos ?? 0.5 });
    }
  }, [widget.xPos, widget.yPos]);

  const flush = useCallback(() => {
    rafRef.current = null;
    const p = pendingRef.current;
    if (!p || !wsClient) return;
    wsClient.send(`${widget.id}|${p.x.toFixed(3)}|${p.y.toFixed(3)}`);
    pendingRef.current = null;
  }, [wsClient, widget.id]);

  const schedule = useCallback((x: number, y: number) => {
    pendingRef.current = { x, y };
    if (rafRef.current != null) return;
    rafRef.current = requestAnimationFrame(flush);
  }, [flush]);

  useEffect(() => () => {
    if (rafRef.current != null) cancelAnimationFrame(rafRef.current);
  }, []);

  const updateFromEvent = useCallback((e: React.PointerEvent) => {
    const el = canvasRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const x = clamp((e.clientX - r.left) / r.width, 0, 1);
    const y = clamp((e.clientY - r.top) / r.height, 0, 1);
    setPos({ x, y });
    schedule(x, y);
  }, [schedule]);

  const onDown = (e: React.PointerEvent) => {
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    draggingRef.current = true;
    updateFromEvent(e);
  };
  const onMove = (e: React.PointerEvent) => {
    if (!draggingRef.current) return;
    updateFromEvent(e);
  };
  const onUp = (e: React.PointerEvent) => {
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    draggingRef.current = false;
    if (rafRef.current != null) { cancelAnimationFrame(rafRef.current); rafRef.current = null; }
    flush();
  };

  const displayMode = widget.displayMode || 'percentage';
  const dx = displayMode === 'dmx'
    ? Math.round(pos.x * 255)
    : displayMode === 'degrees'
      ? `${(pos.x * 360).toFixed(0)}°`
      : `${(pos.x * 100).toFixed(1)}%`;
  const dy = displayMode === 'dmx'
    ? Math.round(pos.y * 255)
    : displayMode === 'degrees'
      ? `${(pos.y * 360).toFixed(0)}°`
      : `${(pos.y * 100).toFixed(1)}%`;

  return (
    <div
      className={`vc-widget vc-xypad${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
    >
      {widget.caption && <div className="xypad-caption">{widget.caption}</div>}
      <div
        ref={canvasRef}
        className="xypad-canvas"
        onPointerDown={onDown}
        onPointerMove={onMove}
        onPointerUp={onUp}
        onPointerCancel={onUp}
        role="slider"
        aria-label={widget.caption || `XY Pad ${widget.id}`}
        aria-valuenow={Math.round(pos.x * 100)}
      >
        <div className="grid-v" style={{ left: '25%' }} />
        <div className="grid-v" style={{ left: '50%' }} />
        <div className="grid-v" style={{ left: '75%' }} />
        <div className="grid-h" style={{ top: '25%' }} />
        <div className="grid-h" style={{ top: '50%' }} />
        <div className="grid-h" style={{ top: '75%' }} />
        <div
          className="xypad-cursor"
          style={{ left: `${pos.x * 100}%`, top: `${pos.y * 100}%` }}
        />
      </div>
      <div className="xypad-display">X: {dx} &nbsp; Y: {dy}</div>
      {widget.presets && widget.presets.length > 0 && (
        <div className="xypad-presets">
          {widget.presets.map((p, idx) => (
            <button
              key={idx}
              type="button"
              onClick={() => send(widget.id, 'XYPAD_PRESET', idx)}
            >{p.name || `Preset ${idx + 1}`}</button>
          ))}
        </div>
      )}
    </div>
  );
}

export default memo(VCXYPadImpl);
