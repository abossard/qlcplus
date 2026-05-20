import { memo, useEffect, useState } from 'react';
import type { WidgetData } from '../lib/types';
import { ClockType } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function formatClock(d: Date): string {
  const h = String(d.getHours()).padStart(2, '0');
  const m = String(d.getMinutes()).padStart(2, '0');
  const s = String(d.getSeconds()).padStart(2, '0');
  return `${h}:${m}:${s}`;
}

function VCClockImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const type = widget.clockType ?? ClockType.Clock;
  const [now, setNow] = useState(() => new Date());

  useEffect(() => {
    if (type !== ClockType.Clock) return;
    const id = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(id);
  }, [type]);

  let display: string;
  let label: string;
  if (type === ClockType.Stopwatch) { display = widget.timeDisplay || '00:00:00.0'; label = 'Stopwatch'; }
  else if (type === ClockType.Countdown) { display = widget.timeDisplay || '00:00:00.0'; label = 'Countdown'; }
  else { display = formatClock(now); label = widget.caption || 'Clock'; }

  return (
    <div
      className={`vc-widget vc-clock${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
    >
      <div className="clock-caption">{label}</div>
      <div className="clock-time">{display}</div>
      {type !== ClockType.Clock && (
        <div className="clock-controls">
          <button
            type="button"
            className={widget.isRunning ? 'on' : ''}
            onClick={() => send(widget.id, 'CLOCK_PLAY')}
            aria-label="Play"
          >{widget.isRunning ? '❚❚' : '▶'}</button>
          <button type="button" onClick={() => send(widget.id, 'CLOCK_RESET')} aria-label="Reset">↺</button>
        </div>
      )}
    </div>
  );
}

export default memo(VCClockImpl);
