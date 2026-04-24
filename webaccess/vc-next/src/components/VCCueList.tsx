import { memo, useCallback, useEffect, useRef } from 'react';
import type { WidgetData } from '../lib/types';
import { SideFaderMode } from '../lib/types';
import { useVCStore } from '../store/vc-store';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

const IconPlay = (
  <svg className="icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <path d="M8 5v14l11-7z"/>
  </svg>
);
const IconStop = (
  <svg className="icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <rect x="6" y="6" width="12" height="12"/>
  </svg>
);
const IconPrev = (
  <svg className="icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <path d="M6 6h2v12H6zm3.5 6l8.5-6v12z"/>
  </svg>
);
const IconNext = (
  <svg className="icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden>
    <path d="M6 18l8.5-6L6 6v12zM16 6h2v12h-2z"/>
  </svg>
);

function VCCueListImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const sendV = useVCStore(s => s.sendWidgetValue);
  const scrollRef = useRef<HTMLDivElement>(null);
  const currentStep = widget.currentStep ?? -1;
  const isPlaying = (widget.playbackStatus ?? 0) === 1;
  const steps = widget.steps ?? [];
  const sideMode = widget.sideFaderMode ?? SideFaderMode.None;

  useEffect(() => {
    if (currentStep < 0) return;
    const el = scrollRef.current?.querySelector(`[data-step="${currentStep}"]`) as HTMLElement | null;
    el?.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }, [currentStep]);

  const onStep = useCallback((idx: number) => send(widget.id, 'STEP', idx), [send, widget.id]);

  return (
    <div
      className={`vc-widget vc-cuelist${widget.disabled ? ' disabled' : ''}`}
      style={{ background: widget.bgColor || undefined, color: widget.fgColor || undefined, ...cssFont(widget.font) }}
      aria-label={widget.caption || `Cue list ${widget.id}`}
    >
      <div className="cuelist-body">
        <div className="cuelist-scroll" ref={scrollRef}>
          <table className="cuelist-table">
            <thead>
              <tr>
                <th style={{ width: 36 }}>#</th>
                <th>Label</th>
                <th style={{ width: 64 }}>In</th>
                <th style={{ width: 64 }}>Out</th>
                <th style={{ width: 72 }}>Dur</th>
                <th>Note</th>
              </tr>
            </thead>
            <tbody>
              {steps.map((s, idx) => (
                <tr
                  key={idx}
                  data-step={idx}
                  className={idx === currentStep ? 'current' : ''}
                  onClick={() => onStep(idx)}
                >
                  <td>{s.index ?? idx + 1}</td>
                  <td>{s.label ?? ''}</td>
                  <td>{s.fadeIn ?? ''}</td>
                  <td>{s.fadeOut ?? ''}</td>
                  <td>{s.duration ?? s.hold ?? ''}</td>
                  <td>{s.note ?? ''}</td>
                </tr>
              ))}
              {steps.length === 0 && (
                <tr><td colSpan={6} style={{ textAlign: 'center', color: '#6b7080', padding: 24 }}>No steps</td></tr>
              )}
            </tbody>
          </table>
        </div>

        {sideMode !== SideFaderMode.None && (
          <div className="cuelist-side">
            <span className={`side-label${widget.primaryTop ? ' active' : ''}`}>{widget.primaryLabel || '—'}</span>
            <input
              type="range"
              className="vslider"
              min={0}
              max={100}
              value={widget.sideFaderLevel ?? 0}
              onChange={(e) => sendV(widget.id, `CUE_SIDECHANGE|${e.target.value}`)}
              style={{ flex: 1, width: 32 }}
              aria-label="Side fader"
            />
            <span className={`side-label${!widget.primaryTop ? ' active' : ''}`}>{widget.secondaryLabel || '—'}</span>
          </div>
        )}
      </div>

      <div className="cuelist-controls">
        <button type="button" onClick={() => send(widget.id, 'PREV')} aria-label="Previous">{IconPrev}</button>
        <button
          type="button"
          className={isPlaying ? 'playing' : ''}
          onClick={() => send(widget.id, 'PLAY')}
          aria-label="Play"
        >{IconPlay}</button>
        <button type="button" onClick={() => send(widget.id, 'STOP')} aria-label="Stop">{IconStop}</button>
        <button type="button" onClick={() => send(widget.id, 'NEXT')} aria-label="Next">{IconNext}</button>
      </div>
    </div>
  );
}

export default memo(VCCueListImpl);
