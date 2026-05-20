import { memo } from 'react';
import type { WidgetData } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function VCAudioTriggersImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const sendV = useVCStore(s => s.sendWidgetValue);
  const enabled = !!widget.audioEnabled;
  const volume = widget.audioVolume ?? 128;
  const bars = widget.bars ?? [];

  return (
    <div
      className={`vc-widget vc-audio${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
    >
      <div className="audio-header">
        <button
          type="button"
          className={`audio-toggle${enabled ? ' on' : ''}`}
          onClick={() => sendV(widget.id, enabled ? 0 : 255)}
          aria-pressed={enabled}
        >{enabled ? 'On' : 'Off'}</button>
        <input
          type="range"
          className="audio-volume"
          min={0}
          max={255}
          value={volume}
          onChange={(e) => send(widget.id, 'AUDIO_VOLUME', e.target.value)}
          aria-label="Volume"
        />
      </div>
      <div className="audio-bars" aria-hidden>
        {(bars.length ? bars : Array.from({ length: 16 }, () => 0)).map((v, idx) => (
          <div
            key={idx}
            className="audio-bar"
            style={{ height: `${Math.max(2, Math.min(100, v))}%` }}
          />
        ))}
      </div>
      {widget.caption && <div className="matrix-caption">{widget.caption}</div>}
    </div>
  );
}

export default memo(VCAudioTriggersImpl);
