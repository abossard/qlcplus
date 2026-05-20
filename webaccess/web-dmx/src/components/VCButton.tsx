import { memo, useCallback, useRef } from 'react';
import type { WidgetData } from '../lib/types';
import { ButtonAction } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont, contrastTextColor, lightenColor, relativeLuminance } from '../lib/utils';

interface Props { widget: WidgetData }

// Inline SVG icons (no QRC deps)
const IconFlash = (
  <svg className="btn-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <path d="M13 2L3 14h7l-1 8 10-12h-7l1-8z"/>
  </svg>
);
const IconBlackout = (
  <svg className="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" aria-hidden>
    <circle cx="12" cy="12" r="9"/>
    <path d="M5 5l14 14"/>
  </svg>
);
const IconStopAll = (
  <svg className="btn-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <rect x="5" y="5" width="14" height="14" rx="2"/>
  </svg>
);

function VCButtonImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetValue);
  const pressedRef = useRef(false);

  const action = widget.actionType ?? ButtonAction.Toggle;
  const isActive = widget.state === '255' || widget.state === 'true';
  const bg = widget.bgColor || '#24263b';
  // Auto-pick contrasting text on bright bg colors when no explicit fg is set.
  const fg = widget.fgColor || contrastTextColor(bg);
  const bgLum = relativeLuminance(bg) ?? 0;
  const isBrightBg = bgLum > 0.55;

  const style: React.CSSProperties = {
    background: isActive ? lightenColor(bg, 1.35) : bg,
    color: fg,
    ...cssFont(widget.font),
  };

  const handleClick = useCallback(() => {
    if (action === ButtonAction.Toggle) {
      send(widget.id, isActive ? 0 : 255);
    } else if (action === ButtonAction.Blackout || action === ButtonAction.StopAll) {
      send(widget.id, 255);
    }
  }, [action, isActive, send, widget.id]);

  const handlePointerDown = useCallback((e: React.PointerEvent) => {
    if (action !== ButtonAction.Flash) return;
    (e.currentTarget as HTMLElement).setPointerCapture?.(e.pointerId);
    pressedRef.current = true;
    send(widget.id, 255);
  }, [action, send, widget.id]);

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    if (action !== ButtonAction.Flash) return;
    try { (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId); } catch { /* noop */ }
    if (pressedRef.current) {
      pressedRef.current = false;
      send(widget.id, 0);
    }
  }, [action, send, widget.id]);

  let icon: React.ReactNode = null;
  if (action === ButtonAction.Flash) icon = IconFlash;
  else if (action === ButtonAction.Blackout) icon = IconBlackout;
  else if (action === ButtonAction.StopAll) icon = IconStopAll;

  return (
    <button
      type="button"
      className={`vc-widget vc-button${isActive ? ' state-active' : ''}${isBrightBg ? ' bright-bg' : ''}${widget.disabled ? ' disabled' : ''}`}
      style={style}
      onClick={action === ButtonAction.Flash ? undefined : handleClick}
      onPointerDown={handlePointerDown}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
      aria-pressed={isActive}
      aria-label={widget.caption || `Button ${widget.id}`}
      disabled={widget.disabled}
    >
      {icon}
      {widget.caption && <span className="btn-caption">{widget.caption}</span>}
    </button>
  );
}

export default memo(VCButtonImpl);
