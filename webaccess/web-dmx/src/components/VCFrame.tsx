import { memo, useState } from 'react';
import type { WidgetData } from '../lib/types';
import { WidgetType } from '../lib/types';
import { WidgetRenderer } from './WidgetRenderer';
import { useVCStore } from '../store/vc-store';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function VCFrameImpl({ widget }: Props) {
  const send = useVCStore(s => s.sendWidgetCommand);
  const [collapsed, setCollapsed] = useState(!!(widget.isCollapsed ?? widget.collapsed));
  const typeId = widget.typeId ?? widget.type;
  const isSolo = typeId === WidgetType.SoloFrame;
  const isMulti = !!(widget.multiPageMode ?? widget.multipage);
  const totalPages = widget.totalPages ?? 1;
  const currentPage = widget.currentPage ?? 0;
  const pageLabel = widget.pageLabels?.[currentPage] ?? `${currentPage + 1} / ${totalPages}`;

  const style: React.CSSProperties = {
    background: widget.bgColor || undefined,
    color: widget.fgColor || undefined,
    ...cssFont(widget.font),
  };

  const showHeader = (widget.showHeader ?? widget.headerVisible) !== false;

  return (
    <div
      className={`vc-widget vc-frame${isSolo ? ' soloframe' : ''}${collapsed ? ' frame-collapsed' : ''}${widget.disabled ? ' disabled' : ''}`}
      style={style}
    >
      {showHeader && (
        <div className="frame-header">
          <button
            type="button"
            className="frame-btn"
            aria-label={collapsed ? 'Expand' : 'Collapse'}
            onClick={() => setCollapsed(c => !c)}
          >
            {collapsed ? '▸' : '▾'}
          </button>
          <span className="frame-caption">{widget.caption || ''}</span>
          {isMulti && (
            <>
              <button
                type="button"
                className="frame-btn"
                onClick={() => send(widget.id, 'PREV_PG')}
                aria-label="Previous page"
              >‹</button>
              <span className="frame-page-label">{pageLabel}</span>
              <button
                type="button"
                className="frame-btn"
                onClick={() => send(widget.id, 'NEXT_PG')}
                aria-label="Next page"
              >›</button>
            </>
          )}
          {(widget.showEnable ?? widget.enableButtonVisible) && (
            <button
              type="button"
              className={`frame-btn enable${!widget.disabled ? ' on' : ''}`}
              onClick={() => send(widget.id, 'FRAME_DISABLE', widget.disabled ? 0 : 1)}
              aria-label="Enable/disable"
              title="Enable/disable"
            >⏻</button>
          )}
        </div>
      )}
      <div className="frame-body">
        {(widget.children ?? [])
          .filter(c => !isMulti || (c.page ?? 0) === currentPage)
          .map(c => <WidgetRenderer key={c.id} widget={c} />)}
      </div>
    </div>
  );
}

export default memo(VCFrameImpl);
