import { memo } from 'react';
import type { WidgetData } from '../lib/types';
import { WidgetType } from '../lib/types';
import VCButton from './VCButton';
import VCSlider from './VCSlider';
import VCCueList from './VCCueList';
import VCFrame from './VCFrame';
import VCXYPad from './VCXYPad';
import VCLabel from './VCLabel';
import VCClock from './VCClock';
import VCSpeedDial from './VCSpeedDial';
import VCMatrix from './VCMatrix';
import VCAudioTriggers from './VCAudioTriggers';

interface Props {
  widget: WidgetData;
}

function WidgetRendererImpl({ widget }: Props) {
  const g = widget.geometry ?? { x: 0, y: 0, w: 120, h: 60 };
  const style: React.CSSProperties = {
    left: g.x,
    top: g.y,
    width: g.w,
    height: g.h,
  };

  let inner: React.ReactNode = null;
  const typeId = widget.typeId ?? widget.type;
  switch (typeId) {
    case WidgetType.Button:        inner = <VCButton widget={widget} />; break;
    case WidgetType.Slider:        inner = <VCSlider widget={widget} />; break;
    case WidgetType.XYPad:         inner = <VCXYPad widget={widget} />; break;
    case WidgetType.Frame:
    case WidgetType.SoloFrame:     inner = <VCFrame widget={widget} />; break;
    case WidgetType.Speed:         inner = <VCSpeedDial widget={widget} />; break;
    case WidgetType.CueList:       inner = <VCCueList widget={widget} />; break;
    case WidgetType.Label:         inner = <VCLabel widget={widget} />; break;
    case WidgetType.AudioTriggers: inner = <VCAudioTriggers widget={widget} />; break;
    case WidgetType.Animation:     inner = <VCMatrix widget={widget} />; break;
    case WidgetType.Clock:         inner = <VCClock widget={widget} />; break;
    default:
      inner = <div style={{ padding: 8, fontSize: 11, color: '#9aa0b4' }}>
        {widget.caption || `Widget #${widget.id}`}
      </div>;
  }

  return <div className="vc-widget-pos" style={style}>{inner}</div>;
}

export const WidgetRenderer = memo(WidgetRendererImpl);
