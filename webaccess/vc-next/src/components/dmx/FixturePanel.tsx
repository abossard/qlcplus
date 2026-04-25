import { memo, useCallback, useEffect, useState } from 'react';
import type { FixtureInfo, ControlPlan, ChannelInfo } from '../../lib/dmx-types';
import { useDmxStore } from '../../store/dmx-store';
import {
  deletePreset,
  getPresets,
  recallPreset,
  savePreset,
  type PresetEntry,
} from '../../lib/presets';
import DimmerFader from './DimmerFader';
import ColorPicker from './ColorPicker';
import PositionControl from './PositionControl';
import GoboSelector from './GoboSelector';
import ShutterControl from './ShutterControl';
import ColorMacroSelector from './ColorMacroSelector';
import ChannelFader from './ChannelFader';

interface Props {
  fixture: FixtureInfo;
}

function findChannel(fixture: FixtureInfo, idx: number): ChannelInfo | undefined {
  return fixture.channelsDetail?.find(c => c.index === idx);
}

function PlanRenderer({ fixture, plan }: { fixture: FixtureInfo; plan: ControlPlan }) {
  switch (plan.kind) {
    case 'dimmer': {
      const ch = findChannel(fixture, plan.channel);
      return <DimmerFader fixtureId={fixture.id} channelIndex={plan.channel} name={ch?.name ?? 'Dimmer'} />;
    }
    case 'color':
      return (
        <ColorPicker
          fixtureId={fixture.id}
          rgbChannels={plan.rgbChannels}
          extraChannels={plan.extra}
        />
      );
    case 'position':
      return (
        <PositionControl
          fixtureId={fixture.id}
          pan={plan.pan}
          tilt={plan.tilt}
          panMax={plan.panMax}
          tiltMax={plan.tiltMax}
        />
      );
    case 'gobo':
      return (
        <GoboSelector
          fixtureId={fixture.id}
          channelIndex={plan.channel}
          capabilities={plan.capabilities}
        />
      );
    case 'shutter':
      return (
        <ShutterControl
          fixtureId={fixture.id}
          channelIndex={plan.channel}
          capabilities={plan.capabilities}
        />
      );
    case 'colorMacro':
      return (
        <ColorMacroSelector
          fixtureId={fixture.id}
          channelIndex={plan.channel}
          capabilities={plan.capabilities}
        />
      );
    case 'generic':
      return (
        <ChannelFader
          fixtureId={fixture.id}
          channelIndex={plan.channel}
          name={plan.name}
          capabilities={plan.capabilities}
        />
      );
  }
}

function groupPlans(plans: ControlPlan[]) {
  const primary: ControlPlan[] = [];
  const generic: Extract<ControlPlan, { kind: 'generic' }>[] = [];
  for (const p of plans) {
    if (p.kind === 'generic') generic.push(p);
    else primary.push(p);
  }
  return { primary, generic };
}

function planChannelIndex(plan: ControlPlan): number {
  switch (plan.kind) {
    case 'dimmer':
      return plan.channel;
    case 'color':
      return Math.min(...plan.rgbChannels);
    case 'position':
      return Math.min(plan.pan.coarse, plan.tilt.coarse);
    case 'gobo':
    case 'shutter':
    case 'colorMacro':
    case 'generic':
      return plan.channel;
  }
}

function coveredChannelIndices(plans: ControlPlan[]): Set<number> {
  const set = new Set<number>();
  for (const p of plans) {
    switch (p.kind) {
      case 'dimmer':
        set.add(p.channel);
        if (p.fine !== undefined) set.add(p.fine);
        break;
      case 'color':
        for (const idx of p.rgbChannels) set.add(idx);
        for (const e of p.extra) {
          set.add(e.channel);
          if (e.fine !== undefined) set.add(e.fine);
        }
        break;
      case 'position':
        set.add(p.pan.coarse);
        if (p.pan.fine !== undefined) set.add(p.pan.fine);
        set.add(p.tilt.coarse);
        if (p.tilt.fine !== undefined) set.add(p.tilt.fine);
        break;
      case 'gobo':
      case 'shutter':
      case 'colorMacro':
        set.add(p.channel);
        break;
      case 'generic':
        set.add(p.channel);
        if (p.fine !== undefined) set.add(p.fine);
        break;
    }
  }
  return set;
}

function FixturePanelImpl({ fixture }: Props) {
  const plans = useDmxStore(s => s.plans.get(fixture.id) ?? []);
  const resetFixture = useDmxStore(s => s.resetFixture);
  const liveValues = useDmxStore(s => s.liveValues.get(fixture.id));

  const [presets, setPresets] = useState<PresetEntry[]>([]);

  const sorted = [...plans].sort((a, b) => planChannelIndex(a) - planChannelIndex(b));
  const { primary, generic } = groupPlans(sorted);
  const hasUncoveredChannels = generic.length > 0;

  const [rawOpen, setRawOpen] = useState(primary.length === 0);

  useEffect(() => {
    setPresets(getPresets(fixture));
  }, [fixture]);

  const onSavePreset = useCallback(() => {
    const name = window.prompt('Preset name?');
    if (!name || !liveValues) return;
    savePreset(fixture, name, liveValues);
    setPresets(getPresets(fixture));
  }, [fixture, liveValues]);

  const onRecall = useCallback((id: string) => {
    const values = recallPreset(id);
    if (!values) return;
    const writes: [number, number][] = values.map(v => [v.channel, v.value]);
    useDmxStore.getState().setChannels(fixture.id, writes);
  }, [fixture.id]);

  const onDelete = useCallback((id: string) => {
    deletePreset(id);
    setPresets(getPresets(fixture));
  }, [fixture]);

  const subtitle = [fixture.manufacturer, fixture.model, fixture.mode].filter(Boolean).join(' · ');

  const coveredIdx = coveredChannelIndices(primary);
  const allChannels = (fixture.channelsDetail ?? [])
    .slice()
    .sort((a, b) => a.index - b.index);

  return (
    <article className="fixture-panel" aria-label={fixture.name}>
      <header className="fp-header">
        <div className="fp-title-wrap">
          <h3 className="fp-title">{fixture.name}</h3>
          {subtitle && <div className="fp-subtitle">{subtitle}</div>}
        </div>
        <div className="fp-meta">
          {fixture.type && (
            <span className="fp-badge">
              {fixture.type}
              {hasUncoveredChannels && <span className="fp-badge-star" title="Has uncovered channels"> ★</span>}
            </span>
          )}
          <span className="fp-addr">U{fixture.universe + 1}·{fixture.address + 1}</span>
          <button
            type="button"
            className="fp-reset"
            onClick={() => resetFixture(fixture.id)}
            title="Reset all channels"
            aria-label="Reset fixture"
          >✕</button>
        </div>
      </header>

      <div className="fp-presets">
        <button type="button" className="fp-preset-save" onClick={onSavePreset}>＋ Save</button>
        {presets.map(p => (
          <span key={p.id} className="fp-preset">
            <button
              type="button"
              className="fp-preset-recall"
              onClick={() => onRecall(p.id)}
              title={`Recall ${p.name}`}
            >{p.name}</button>
            <button
              type="button"
              className="fp-preset-del"
              onClick={() => onDelete(p.id)}
              aria-label={`Delete ${p.name}`}
            >×</button>
          </span>
        ))}
      </div>

      <div className="fp-body">
        {primary.map((plan, i) => (
          <section key={`${plan.kind}-${i}`} className={`fp-section fp-section-${plan.kind}`}>
            <PlanRenderer fixture={fixture} plan={plan} />
          </section>
        ))}

        {allChannels.length > 0 && (
          <section className={`raw-channels${rawOpen ? ' open' : ''}`}>
            <button
              type="button"
              className="rc-toggle"
              onClick={() => setRawOpen(o => !o)}
              aria-expanded={rawOpen}
            >
              {rawOpen ? '▾' : '▸'} Raw Channels ({allChannels.length})
            </button>
            {rawOpen && (
              <div className="rc-grid">
                {allChannels.map(ch => (
                  <ChannelFader
                    key={`raw-${ch.index}`}
                    fixtureId={fixture.id}
                    channelIndex={ch.index}
                    name={ch.name}
                    capabilities={ch.capabilities}
                    readOnly={coveredIdx.has(ch.index)}
                  />
                ))}
              </div>
            )}
          </section>
        )}
      </div>
    </article>
  );
}

export default memo(FixturePanelImpl);
