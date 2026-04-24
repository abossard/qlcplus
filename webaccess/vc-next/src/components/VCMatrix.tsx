import { memo } from 'react';
import type { WidgetData } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function VCMatrixImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const colors = widget.matrixColors ?? [];
  const opts = widget.matrixComboOptions ?? [];
  const running = widget.matrixState === '1' || widget.matrixState === 'true';

  return (
    <div
      className={`vc-widget vc-matrix${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
    >
      {widget.caption && <div className="matrix-caption">{widget.caption}</div>}

      <div className="matrix-colors">
        {Array.from({ length: 5 }).map((_, idx) => {
          const c = colors[idx] || '#000000';
          return (
            <label key={idx} className="matrix-color" style={{ background: c }} title={`Color ${idx + 1}`}>
              <input
                type="color"
                value={c.startsWith('#') ? c : '#000000'}
                onChange={(e) => send(widget.id, `MATRIX_COLOR_${idx + 1}`, e.target.value)}
                aria-label={`Color ${idx + 1}`}
              />
            </label>
          );
        })}
      </div>

      <div className="matrix-row">
        <label>Level</label>
        <input
          type="range"
          min={0}
          max={255}
          value={widget.matrixSliderValue ?? 0}
          onChange={(e) => send(widget.id, 'MATRIX_SLIDER', e.target.value)}
        />
      </div>

      {opts.length > 0 && (
        <div className="matrix-row">
          <label>Preset</label>
          <select
            value={widget.matrixComboValue ?? ''}
            onChange={(e) => send(widget.id, 'MATRIX_COMBO', e.target.value)}
          >
            {opts.map((o, idx) => (
              <option key={idx} value={o.value}>{o.label}</option>
            ))}
          </select>
        </div>
      )}

      <div className="matrix-row">
        <button
          type="button"
          className={`audio-toggle${running ? ' on' : ''}`}
          onClick={() => send(widget.id, running ? 'MATRIX_OFF' : 'MATRIX_ON')}
        >{running ? 'Stop' : 'Start'}</button>
      </div>
    </div>
  );
}

export default memo(VCMatrixImpl);
