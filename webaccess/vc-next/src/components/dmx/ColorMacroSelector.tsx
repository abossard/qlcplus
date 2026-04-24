import { memo } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import type { CapabilityInfo } from '../../lib/dmx-types';

interface Props {
  fixtureId: number;
  channelIndex: number;
  capabilities: CapabilityInfo[];
}

function ColorMacroSelectorImpl({ fixtureId, channelIndex, capabilities }: Props) {
  const setChannel = useDmxStore(s => s.setChannel);
  const value = useDmxStore(s => s.liveValues.get(fixtureId)?.[channelIndex] ?? 0);
  const activeIdx = capabilities.findIndex(c => value >= c.min && value <= c.max);

  return (
    <div className="color-macro-grid" role="radiogroup" aria-label="color macros">
      {capabilities.map((cap, i) => {
        const mid = Math.floor((cap.min + cap.max) / 2);
        const active = i === activeIdx;
        const bg = cap.color1 || '#333';
        const grad = cap.color2 ? `linear-gradient(135deg, ${bg} 0 50%, ${cap.color2} 50% 100%)` : bg;
        return (
          <button
            key={i}
            type="button"
            className={`cm-cell${active ? ' active' : ''}`}
            onClick={() => setChannel(fixtureId, channelIndex, mid)}
            title={`${cap.name} (${cap.min}-${cap.max})`}
            aria-pressed={active}
          >
            <span className="cm-swatch" style={{ background: grad }} />
            <span className="cm-label">{cap.name}</span>
          </button>
        );
      })}
    </div>
  );
}

export default memo(ColorMacroSelectorImpl);
