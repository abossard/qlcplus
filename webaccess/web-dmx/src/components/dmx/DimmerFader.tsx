import { memo } from 'react';
import ChannelFader from './ChannelFader';

interface Props {
  fixtureId: number;
  channelIndex: number;
  name?: string;
  height?: number;
}

function DimmerFaderImpl({ fixtureId, channelIndex, name = 'Dimmer', height = 220 }: Props) {
  return (
    <ChannelFader
      fixtureId={fixtureId}
      channelIndex={channelIndex}
      name={name}
      height={height}
      variant="dimmer"
      accent="var(--warn)"
      displayValue={v => `${Math.round((v / 255) * 100)}%`}
    />
  );
}

export default memo(DimmerFaderImpl);
