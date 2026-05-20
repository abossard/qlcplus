import { memo, useCallback, useEffect, useRef, useState } from 'react';
import type { WidgetData } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { clamp, cssFont, sliderDisplayValue } from '../lib/utils';

interface Props { widget: WidgetData }

function VCSliderImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetValue);
  const min = widget.min ?? 0;
  const max = widget.max ?? 255;
  const [localValue, setLocalValue] = useState(widget.value ?? 0);
  const draggingRef = useRef(false);
  const rafRef = useRef<number | null>(null);
  const pendingRef = useRef<number | null>(null);

  // Sync from server when not actively dragging
  useEffect(() => {
    if (!draggingRef.current) setLocalValue(widget.value ?? 0);
  }, [widget.value]);

  // Throttled send (~60fps via rAF)
  const scheduleSend = useCallback((v: number) => {
    pendingRef.current = v;
    if (rafRef.current != null) return;
    rafRef.current = requestAnimationFrame(() => {
      rafRef.current = null;
      if (pendingRef.current != null) {
        send(widget.id, pendingRef.current);
        pendingRef.current = null;
      }
    });
  }, [send, widget.id]);

  useEffect(() => () => {
    if (rafRef.current != null) cancelAnimationFrame(rafRef.current);
  }, []);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    let v = parseInt(e.target.value, 10);
    if (Number.isNaN(v)) return;
    v = clamp(v, min, max);
    setLocalValue(v);
    scheduleSend(v);
  };

  const handlePointerDown = () => { draggingRef.current = true; };
  const handlePointerUp = () => {
    draggingRef.current = false;
    if (pendingRef.current != null) {
      send(widget.id, pendingRef.current);
      pendingRef.current = null;
    }
  };

  const isKnob = widget.widgetStyle === 'Knob';
  const inverted = widget.inverted === true;
  const displayVal = inverted ? max - (localValue - min) : localValue;
  const pct = max > min ? (localValue - min) / (max - min) : 0;
  const displayPct = inverted ? 1 - pct : pct;
  const knobAngle = -135 + displayPct * 270;

  const style: React.CSSProperties = {
    background: widget.bgColor || undefined,
    color: widget.fgColor || undefined,
    ...cssFont(widget.font),
  };

  return (
    <div
      className={`vc-widget vc-slider${widget.disabled ? ' disabled' : ''}`}
      style={style}
      aria-label={widget.caption || `Slider ${widget.id}`}
    >
      <div className="s-value">{sliderDisplayValue(displayVal, widget.valueDisplay, max)}</div>
      <div className="s-track">
        {widget.clickAndGoType && widget.clickAndGoType !== 'None' && widget.cngPrimaryColor && (
          <span className="s-cng" style={{ background: widget.cngPrimaryColor }} />
        )}
        {isKnob ? (
          <div
            className="vc-knob"
            role="slider"
            tabIndex={0}
            aria-valuenow={displayVal}
            aria-valuemin={min}
            aria-valuemax={max}
            style={{ ['--knob-angle' as never]: `${knobAngle}deg` } as React.CSSProperties}
            onPointerDown={(e) => {
              (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
              draggingRef.current = true;
            }}
            onPointerMove={(e) => {
              if (!draggingRef.current) return;
              const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
              const cx = rect.left + rect.width / 2;
              const cy = rect.top + rect.height / 2;
              const dx = e.clientX - cx;
              const dy = e.clientY - cy;
              // Map angle (-135..135) to value
              let ang = Math.atan2(dy, dx) * 180 / Math.PI + 90;
              if (ang > 180) ang -= 360;
              ang = clamp(ang, -135, 135);
              const p = (ang + 135) / 270;
              const effective = inverted ? 1 - p : p;
              const v = Math.round(min + effective * (max - min));
              setLocalValue(v);
              scheduleSend(v);
            }}
            onPointerUp={(e) => {
              try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
              handlePointerUp();
            }}
            onPointerCancel={handlePointerUp}
          />
        ) : (
          <input
            type="range"
            className="vslider"
            min={min}
            max={max}
            value={inverted ? max - (localValue - min) + min : localValue}
            onChange={(e) => {
              let raw = parseInt(e.target.value, 10);
              if (Number.isNaN(raw)) return;
              if (inverted) raw = max - (raw - min) + min;
              const v = clamp(raw, min, max);
              setLocalValue(v);
              scheduleSend(v);
            }}
            onPointerDown={handlePointerDown}
            onPointerUp={handlePointerUp}
            onPointerCancel={handlePointerUp}
            onMouseUp={handlePointerUp}
            onTouchEnd={handlePointerUp}
            onKeyUp={handlePointerUp}
            onBlur={handlePointerUp}
            aria-valuenow={displayVal}
            aria-valuemin={min}
            aria-valuemax={max}
            aria-label={widget.caption || `Slider ${widget.id}`}
          />
        )}
        {widget.monitorEnabled && (
          <div className="s-monitor" style={{ height: `${displayPct * 100}%` }} />
        )}
      </div>
      {widget.caption && <div className="s-caption">{widget.caption}</div>}
      {widget.isOverriding && (
        <button
          type="button"
          className="s-override"
          onClick={() => useVCStore.getState().sendWidgetCommand(widget.id, 'SLIDER_RESET')}
        >
          Reset
        </button>
      )}
    </div>
  );
}

export default memo(VCSliderImpl);
