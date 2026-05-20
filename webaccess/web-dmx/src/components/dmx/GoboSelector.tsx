import { memo } from 'react';
import { useDmxStore } from '../../store/dmx-store';
import type { CapabilityInfo } from '../../lib/dmx-types';

interface Props {
  fixtureId: number;
  channelIndex: number;
  capabilities: CapabilityInfo[];
}

function resolveImage(raw: string): string {
  if (!raw) return '';
  if (raw.startsWith('http') || raw.startsWith('/') || raw.startsWith('data:')) return raw;
  return `/qrc/${raw}`;
}

function GoboSelectorImpl({ fixtureId, channelIndex, capabilities }: Props) {
  const setChannel = useDmxStore(s => s.setChannel);
  const value = useDmxStore(s => s.liveValues.get(fixtureId)?.[channelIndex] ?? 0);

  const activeIdx = capabilities.findIndex(c => value >= c.min && value <= c.max);

  return (
    <div className="gobo-grid" role="radiogroup" aria-label="gobo">
      {capabilities.map((cap, i) => {
        const mid = Math.floor((cap.min + cap.max) / 2);
        const active = i === activeIdx;
        const img = cap.image ? resolveImage(cap.image) : '';
        return (
          <button
            key={i}
            type="button"
            className={`gobo-cell${active ? ' active' : ''}`}
            onClick={() => setChannel(fixtureId, channelIndex, mid)}
            title={`${cap.name} (${cap.min}-${cap.max})`}
            aria-pressed={active}
          >
            {img ? (
              <img src={img} alt={cap.name} className="gobo-img" draggable={false} />
            ) : (
              <span className="gobo-label">{cap.name}</span>
            )}
            <span className="gobo-caption">{cap.name}</span>
          </button>
        );
      })}
    </div>
  );
}

export default memo(GoboSelectorImpl);
