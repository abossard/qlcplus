import { memo } from 'react';
import type { WidgetData } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont, formatTime } from '../lib/utils';

interface Props { widget: WidgetData }

const FACTORS = [
  { label: '1/16', idx: 0 },
  { label: '1/8', idx: 1 },
  { label: '1/4', idx: 2 },
  { label: '1/2', idx: 3 },
  { label: '1x', idx: 4 },
  { label: '2x', idx: 5 },
  { label: '4x', idx: 6 },
  { label: '8x', idx: 7 },
];

// Visibility mask bits — tolerate missing bits, default show all
const VIS_TAP = 1 << 0;
const VIS_APPLY = 1 << 1;
const VIS_HOURS = 1 << 2;
const VIS_MIN = 1 << 3;
const VIS_SEC = 1 << 4;
const VIS_MS = 1 << 5;
const VIS_FACTOR = 1 << 6;

function VCSpeedDialImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const vis = widget.visibilityMask ?? 0xffff;
  const show = (flag: number) => (vis & flag) !== 0;
  const ms = widget.speedValue ?? 0;
  const currentFactor = widget.speedFactor ?? 4;

  return (
    <div
      className={`vc-widget vc-speed${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
    >
      {(show(VIS_HOURS) || show(VIS_MIN) || show(VIS_SEC) || show(VIS_MS)) && (
        <div className="speed-row">
          <button type="button" className="speed-btn" onClick={() => send(widget.id, 'SPEED_DOWN')} aria-label="Decrease">−</button>
          <div className="speed-time">{formatTime(ms)}</div>
          <button type="button" className="speed-btn" onClick={() => send(widget.id, 'SPEED_UP')} aria-label="Increase">+</button>
        </div>
      )}
      <div className="speed-row">
        {show(VIS_TAP) && (
          <button type="button" className="speed-btn" onClick={() => send(widget.id, 'SPEED_TAP')}>Tap</button>
        )}
        {show(VIS_APPLY) && (
          <button type="button" className="speed-btn" onClick={() => send(widget.id, 'SPEED_APPLY')}>Apply</button>
        )}
      </div>
      {show(VIS_FACTOR) && (
        <div className="speed-factors">
          {FACTORS.map(f => (
            <button
              key={f.idx}
              type="button"
              className={currentFactor === f.idx ? 'active' : ''}
              onClick={() => send(widget.id, 'SPEED_FACTOR', f.idx)}
            >{f.label}</button>
          ))}
        </div>
      )}
      {widget.caption && <div className="speed-caption">{widget.caption}</div>}
    </div>
  );
}

export default memo(VCSpeedDialImpl);
