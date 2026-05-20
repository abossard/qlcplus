import { memo } from 'react';
import type { WidgetData } from '../lib/types';
import { cssFont } from '../lib/utils';

interface Props { widget: WidgetData }

function VCLabelImpl({ widget }: Props) {
  return (
    <div
      className={`vc-widget vc-label${widget.disabled ? ' disabled' : ''}`}
      style={{
        background: widget.bgColor || undefined,
        color: widget.fgColor || undefined,
        ...cssFont(widget.font),
      }}
      role="note"
    >
      {widget.caption || ''}
    </div>
  );
}

export default memo(VCLabelImpl);
