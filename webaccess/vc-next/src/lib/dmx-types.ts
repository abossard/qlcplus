// Type definitions for the DMX Control Panel.
// These mirror the JSON shape returned by /api/fixtures and /api/channels.

export interface FixtureInfo {
  id: number;
  name: string;
  universe: number;
  address: number;
  channels: number;
  manufacturer?: string;
  model?: string;
  mode?: string;
  type?: string;
  capabilities?: string[];
  headMap?: HeadInfo[];
  physical?: PhysicalInfo;
  channelsDetail?: ChannelInfo[];
}

export interface HeadInfo {
  index: number;
  channels: number[];
  rgbChannels?: number[];
  cmyChannels?: number[];
}

export interface PhysicalInfo {
  focusPanMax?: number;
  focusTiltMax?: number;
  lensDegreesMin?: number;
  lensDegreesMax?: number;
}

export interface ChannelInfo {
  fixtureID: number;
  index: number;
  name: string;
  group: string;
  colour: string;
  preset?: string;
  controlByte: 'coarse' | 'fine';
  defaultValue: number;
  headIndex?: number;
  capabilities: CapabilityInfo[];
}

export interface CapabilityInfo {
  min: number;
  max: number;
  name: string;
  preset?: string;
  color1?: string;
  color2?: string;
  image?: string;
}

// Control plan — declarative description of which UI controls a fixture needs.
export type ControlPlan =
  | { kind: 'dimmer'; channel: number; fine?: number; headIndex?: number }
  | {
      kind: 'color';
      rgbChannels: number[];
      extra: { channel: number; colour: string; fine?: number }[];
      headIndex?: number;
    }
  | {
      kind: 'position';
      pan: { coarse: number; fine?: number };
      tilt: { coarse: number; fine?: number };
      headIndex?: number;
      panMax: number;
      tiltMax: number;
    }
  | { kind: 'gobo'; channel: number; capabilities: CapabilityInfo[]; headIndex?: number }
  | { kind: 'shutter'; channel: number; capabilities: CapabilityInfo[]; headIndex?: number }
  | { kind: 'colorMacro'; channel: number; capabilities: CapabilityInfo[]; headIndex?: number }
  | {
      kind: 'generic';
      channel: number;
      fine?: number;
      name: string;
      group: string;
      capabilities: CapabilityInfo[];
      headIndex?: number;
    };

// Live DMX update messages over WS.
export interface DmxStateMsg {
  cmd: 'DMX_STATE';
  universe: number;
  fixtureID: number;
  address: number;
  values: number[];
}

export interface DmxDeltaMsg {
  cmd: 'DMX_DELTA';
  universe: number;
  changes: [number, number][];
}
