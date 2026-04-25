// Smart mapping from raw fixture channels to a renderable ControlPlan list.
// The algorithm walks each head (or the whole fixture when there are no heads),
// pairs coarse/fine channels, and groups channels into semantic controls.

import type {
  CapabilityInfo,
  ChannelInfo,
  ControlPlan,
  FixtureInfo,
  HeadInfo,
} from './dmx-types';

const PLAN_ORDER: ControlPlan['kind'][] = [
  'dimmer',
  'color',
  'position',
  'gobo',
  'shutter',
  'colorMacro',
  'generic',
];

// Strip common fine-channel name suffixes so coarse/fine pairs can be matched.
function normalizeChannelName(name: string): string {
  return name
    .toLowerCase()
    .replace(/\s+(fine|lsb|2|low|coarse|msb)\s*$/i, '')
    .trim();
}

function hasImageCapability(caps: CapabilityInfo[] | undefined): boolean {
  return !!caps?.some(c => !!c.image);
}

function hasColorSwatch(caps: CapabilityInfo[] | undefined): boolean {
  return !!caps?.some(c => !!c.color1);
}

// Build coarse→fine index pairs by walking a sorted channel list.
function pairCoarseFine(channels: ChannelInfo[]): Map<number, number> {
  const fineFor = new Map<number, number>();
  for (let i = 0; i < channels.length - 1; i++) {
    const a = channels[i];
    const b = channels[i + 1];
    if (a.controlByte !== 'coarse' || b.controlByte !== 'fine') continue;
    if (a.group !== b.group) continue;
    if (normalizeChannelName(a.name) !== normalizeChannelName(b.name)) continue;
    fineFor.set(a.index, b.index);
  }
  return fineFor;
}

interface HeadGroup {
  headIndex?: number;
  channels: ChannelInfo[];
}

// Split channels into head-buckets. If the fixture has no headMap, the whole
// fixture is treated as a single anonymous head.
function groupByHead(fixture: FixtureInfo): HeadGroup[] {
  const channels = fixture.channelsDetail ?? [];
  if (!fixture.headMap?.length) {
    return [{ headIndex: undefined, channels: [...channels].sort((a, b) => a.index - b.index) }];
  }
  const groups: HeadGroup[] = [];
  for (const head of fixture.headMap) {
    const idxSet = new Set(head.channels);
    const bucket = channels
      .filter(c => idxSet.has(c.index))
      .sort((a, b) => a.index - b.index);
    if (bucket.length) groups.push({ headIndex: head.index, channels: bucket });
  }
  // Channels not assigned to any head become an extra group (useful for
  // global dimmer/master strobe channels on multi-head movers).
  const allHeadChans = new Set(fixture.headMap.flatMap(h => h.channels));
  const orphans = channels.filter(c => !allHeadChans.has(c.index));
  if (orphans.length) {
    groups.push({
      headIndex: undefined,
      channels: orphans.sort((a, b) => a.index - b.index),
    });
  }
  return groups;
}

function findChannelByIndex(channels: ChannelInfo[], index: number): ChannelInfo | undefined {
  return channels.find(c => c.index === index);
}

function isPanCoarse(c: ChannelInfo): boolean {
  return c.group === 'Pan' && c.controlByte === 'coarse';
}

function isTiltCoarse(c: ChannelInfo): boolean {
  return c.group === 'Tilt' && c.controlByte === 'coarse';
}

function isShutter(c: ChannelInfo): boolean {
  return c.group === 'Shutter' || (c.preset?.startsWith('Shutter') ?? false);
}

function isGobo(c: ChannelInfo): boolean {
  return c.group === 'Gobo' && hasImageCapability(c.capabilities);
}

function isColorMacro(c: ChannelInfo): boolean {
  if (c.preset === 'ColorMacro') return true;
  if (c.group === 'Colour' && hasColorSwatch(c.capabilities)) return true;
  return false;
}

function isDimmer(c: ChannelInfo, headChannelCount: number): boolean {
  if (c.preset === 'IntensityDimmer' || c.preset === 'IntensityMasterDimmer') return true;
  if (c.group === 'Intensity' && headChannelCount === 1) return true;
  return false;
}

// Build plans for one head group.
function planForHead(
  group: HeadGroup,
  headInfo: HeadInfo | undefined,
  fixture: FixtureInfo,
): ControlPlan[] {
  const channels = group.channels;
  const fineFor = pairCoarseFine(channels);
  const consumed = new Set<number>();
  const plans: ControlPlan[] = [];

  const physical = fixture.physical ?? {};
  const panMax = physical.focusPanMax ?? 540;
  const tiltMax = physical.focusTiltMax ?? 270;

  // 1. Color (RGB + extras like W/A/UV/Lime/Indigo).
  if (headInfo?.rgbChannels?.length === 3) {
    const rgb = headInfo.rgbChannels;
    rgb.forEach(idx => consumed.add(idx));
    const extras: { channel: number; colour: string; fine?: number }[] = [];
    for (const c of channels) {
      if (consumed.has(c.index)) continue;
      // Accept both Colour-group and Intensity-group channels with known extra colours
      const isColourGroup = c.group === 'Colour';
      const isExtraIntensity = c.group === 'Intensity'
        && ['White', 'Amber', 'UV', 'Lime', 'Indigo'].includes(c.colour);
      if (!isColourGroup && !isExtraIntensity) continue;
      if (c.controlByte === 'fine') continue;
      // Skip macros/swatches — they get their own colorMacro plan.
      if (isColorMacro(c)) continue;
      if (['White', 'Amber', 'UV', 'Lime', 'Indigo'].includes(c.colour)) {
        extras.push({ channel: c.index, colour: c.colour, fine: fineFor.get(c.index) });
        consumed.add(c.index);
        const fine = fineFor.get(c.index);
        if (fine !== undefined) consumed.add(fine);
      }
    }
    plans.push({
      kind: 'color',
      rgbChannels: rgb,
      extra: extras,
      headIndex: group.headIndex,
    });
  }

  // 2. Position (Pan + Tilt with fine pairs).
  const panCh = channels.find(isPanCoarse);
  const tiltCh = channels.find(isTiltCoarse);
  if (panCh && tiltCh && !consumed.has(panCh.index) && !consumed.has(tiltCh.index)) {
    const panFine = fineFor.get(panCh.index);
    const tiltFine = fineFor.get(tiltCh.index);
    consumed.add(panCh.index);
    consumed.add(tiltCh.index);
    if (panFine !== undefined) consumed.add(panFine);
    if (tiltFine !== undefined) consumed.add(tiltFine);
    plans.push({
      kind: 'position',
      pan: { coarse: panCh.index, fine: panFine },
      tilt: { coarse: tiltCh.index, fine: tiltFine },
      headIndex: group.headIndex,
      panMax,
      tiltMax,
    });
  }

  // 3. Walk remaining coarse channels and classify each.
  for (const c of channels) {
    if (consumed.has(c.index)) continue;
    if (c.controlByte === 'fine') continue; // unpaired fines fall through to generic later

    const fine = fineFor.get(c.index);

    if (isDimmer(c, channels.length)) {
      plans.push({ kind: 'dimmer', channel: c.index, fine, headIndex: group.headIndex });
      consumed.add(c.index);
      if (fine !== undefined) consumed.add(fine);
      continue;
    }
    if (isGobo(c)) {
      plans.push({
        kind: 'gobo',
        channel: c.index,
        capabilities: c.capabilities ?? [],
        headIndex: group.headIndex,
      });
      consumed.add(c.index);
      if (fine !== undefined) consumed.add(fine);
      continue;
    }
    if (isShutter(c)) {
      plans.push({
        kind: 'shutter',
        channel: c.index,
        capabilities: c.capabilities ?? [],
        headIndex: group.headIndex,
      });
      consumed.add(c.index);
      if (fine !== undefined) consumed.add(fine);
      continue;
    }
    if (isColorMacro(c)) {
      plans.push({
        kind: 'colorMacro',
        channel: c.index,
        capabilities: c.capabilities ?? [],
        headIndex: group.headIndex,
      });
      consumed.add(c.index);
      if (fine !== undefined) consumed.add(fine);
      continue;
    }
  }

  // 4. Everything not yet consumed becomes a generic control.
  for (const c of channels) {
    if (consumed.has(c.index)) continue;
    if (c.controlByte === 'fine') continue;
    const fine = fineFor.get(c.index);
    plans.push({
      kind: 'generic',
      channel: c.index,
      fine,
      name: c.name,
      group: c.group,
      capabilities: c.capabilities ?? [],
      headIndex: group.headIndex,
    });
    consumed.add(c.index);
    if (fine !== undefined) consumed.add(fine);
  }

  // Surface any orphan fine channels as generic controls so they remain reachable.
  for (const c of channels) {
    if (consumed.has(c.index)) continue;
    plans.push({
      kind: 'generic',
      channel: c.index,
      name: c.name,
      group: c.group,
      capabilities: c.capabilities ?? [],
      headIndex: group.headIndex,
    });
    consumed.add(c.index);
  }

  return plans;
}

export function planFixture(fixture: FixtureInfo): ControlPlan[] {
  if (!fixture.channelsDetail?.length) return [];
  const groups = groupByHead(fixture);
  const all: ControlPlan[] = [];
  for (const group of groups) {
    const headInfo = fixture.headMap?.find(h => h.index === group.headIndex);
    all.push(...planForHead(group, headInfo, fixture));
  }
  // Stable ordering: dimmer → color → position → gobo → shutter → colorMacro → generic.
  // Within same kind, preserve original order (heads first, channel order within head).
  return all
    .map((plan, i) => ({ plan, i }))
    .sort((a, b) => {
      const ka = PLAN_ORDER.indexOf(a.plan.kind);
      const kb = PLAN_ORDER.indexOf(b.plan.kind);
      if (ka !== kb) return ka - kb;
      return a.i - b.i;
    })
    .map(x => x.plan);
}

// Re-export internal helpers in case other modules want to peek at the raw
// channel pairing logic for diagnostics.
export const __internal = { pairCoarseFine, normalizeChannelName, findChannelByIndex };
