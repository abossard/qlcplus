// Zustand store for the DMX Control Panel.
// - Owns fixture metadata + per-fixture ControlPlans.
// - Tracks live DMX values pushed by the server.
// - Batches outbound channel writes via requestAnimationFrame.

import { create } from 'zustand';
import { fetchFixturesWithChannels } from '../lib/dmx-api';
import { planFixture } from '../lib/control-mapper';
import type { ControlPlan, FixtureInfo } from '../lib/dmx-types';
import {
  sendDmxHeartbeat,
  sendDmxSubscribe,
  sendDmxUnsubscribe,
  setupDmxWS,
} from '../lib/dmx-ws';
import { useVCStore } from './vc-store';

interface ActiveChannel {
  fxId: number;
  ch: number;
}

interface DmxStore {
  fixtures: Map<number, FixtureInfo>;
  plans: Map<number, ControlPlan[]>;
  loading: boolean;
  error: string | null;

  liveValues: Map<number, Uint8Array>;

  pendingWrites: Map<string, number>; // "uni:addr" → 0..255
  activeChannel: ActiveChannel | null;
  heldChannels: Set<string>; // "uni:addr"

  loadFixtures: () => Promise<void>;
  setChannel: (fxId: number, chIndex: number, value: number) => void;
  setChannels: (fxId: number, values: [number, number][]) => void;
  resetChannel: (fxId: number, chIndex: number) => void;
  resetFixture: (fxId: number) => void;
  setActiveChannel: (fxId: number, ch: number) => void;
  clearActiveChannel: () => void;
  applyDmxState: (
    universe: number,
    fixtureID: number,
    address: number,
    values: number[],
  ) => void;
  applyDmxDelta: (universe: number, changes: [number, number][]) => void;
  flush: () => void;

  attachWS: () => () => void;
  subscribeFixtures: (ids: number[]) => void;
  unsubscribeFixtures: (ids: number[]) => void;
}

function clamp255(v: number): number {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v | 0;
}

function addrKey(universe: number, address: number): string {
  return `${universe}:${address}`;
}

// Apply a value into a fixture's local Uint8Array buffer (immutably swapping
// the buffer so React subscribers see a new reference).
function setLiveByte(
  liveValues: Map<number, Uint8Array>,
  fxId: number,
  chIndex: number,
  value: number,
  channelCount?: number,
): Map<number, Uint8Array> {
  const existing = liveValues.get(fxId);
  const len = existing?.length ?? channelCount ?? chIndex + 1;
  const next = new Uint8Array(len);
  if (existing) next.set(existing);
  if (chIndex < next.length) next[chIndex] = clamp255(value);
  const out = new Map(liveValues);
  out.set(fxId, next);
  return out;
}

let rafHandle: number | null = null;

function scheduleFlush(get: () => DmxStore) {
  if (rafHandle != null) return;
  rafHandle = requestAnimationFrame(() => {
    rafHandle = null;
    get().flush();
  });
}

export const useDmxStore = create<DmxStore>((set, get) => ({
  fixtures: new Map(),
  plans: new Map(),
  loading: false,
  error: null,

  liveValues: new Map(),

  pendingWrites: new Map(),
  activeChannel: null,
  heldChannels: new Set(),

  async loadFixtures() {
    set({ loading: true, error: null });
    try {
      const fixtures = await fetchFixturesWithChannels();
      const fxMap = new Map<number, FixtureInfo>();
      const planMap = new Map<number, ControlPlan[]>();
      const liveMap = new Map<number, Uint8Array>();
      for (const fx of fixtures) {
        fxMap.set(fx.id, fx);
        planMap.set(fx.id, planFixture(fx));
        liveMap.set(fx.id, new Uint8Array(fx.channels));
      }
      set({
        fixtures: fxMap,
        plans: planMap,
        liveValues: liveMap,
        loading: false,
        error: null,
      });
    } catch (err) {
      set({
        loading: false,
        error: err instanceof Error ? err.message : String(err),
      });
    }
  },

  setChannel(fxId, chIndex, value) {
    const state = get();
    const fx = state.fixtures.get(fxId);
    if (!fx) return;
    const v = clamp255(value);
    const absAddr = fx.address + chIndex;
    const key = addrKey(fx.universe, absAddr);

    const liveValues = setLiveByte(state.liveValues, fxId, chIndex, v, fx.channels);
    const pendingWrites = new Map(state.pendingWrites);
    pendingWrites.set(key, v);
    const heldChannels = new Set(state.heldChannels);
    heldChannels.add(key);

    set({ liveValues, pendingWrites, heldChannels });
    scheduleFlush(get);
  },

  setChannels(fxId, values) {
    const state = get();
    const fx = state.fixtures.get(fxId);
    if (!fx) return;
    let liveValues = state.liveValues;
    const pendingWrites = new Map(state.pendingWrites);
    const heldChannels = new Set(state.heldChannels);
    for (const [chIndex, raw] of values) {
      const v = clamp255(raw);
      const absAddr = fx.address + chIndex;
      const key = addrKey(fx.universe, absAddr);
      liveValues = setLiveByte(liveValues, fxId, chIndex, v, fx.channels);
      pendingWrites.set(key, v);
      heldChannels.add(key);
    }
    set({ liveValues, pendingWrites, heldChannels });
    scheduleFlush(get);
  },

  resetChannel(fxId, chIndex) {
    const state = get();
    const fx = state.fixtures.get(fxId);
    if (!fx) return;
    const absAddr = fx.address + chIndex;
    const key = addrKey(fx.universe, absAddr);
    const ws = useVCStore.getState().wsClient;
    // QLC+ sdResetChannel uses 1-based addresses.
    ws?.send(`QLC+API|sdResetChannel|${fx.universe * 512 + absAddr + 1}`);
    const heldChannels = new Set(state.heldChannels);
    heldChannels.delete(key);
    const pendingWrites = new Map(state.pendingWrites);
    pendingWrites.delete(key);
    set({ heldChannels, pendingWrites });
  },

  resetFixture(fxId) {
    const state = get();
    const fx = state.fixtures.get(fxId);
    if (!fx) return;
    for (let i = 0; i < fx.channels; i++) {
      get().resetChannel(fxId, i);
    }
  },

  setActiveChannel(fxId, ch) {
    set({ activeChannel: { fxId, ch } });
  },

  clearActiveChannel() {
    set({ activeChannel: null });
  },

  applyDmxState(_universe, fixtureID, _address, values) {
    const state = get();
    const fx = state.fixtures.get(fixtureID);
    if (!fx) return;
    // Replace the entire buffer for the fixture with the snapshot.
    const buf = new Uint8Array(fx.channels);
    for (let i = 0; i < values.length && i < buf.length; i++) {
      buf[i] = clamp255(values[i]);
    }
    // Preserve any active drag value to avoid ping-pong.
    const active = state.activeChannel;
    if (active && active.fxId === fixtureID) {
      const prev = state.liveValues.get(fixtureID);
      if (prev && active.ch < buf.length && active.ch < prev.length) {
        buf[active.ch] = prev[active.ch];
      }
    }
    const next = new Map(state.liveValues);
    next.set(fixtureID, buf);
    set({ liveValues: next });
  },

  applyDmxDelta(universe, changes) {
    const state = get();
    if (!changes.length) return;

    // Build address → fixture/channel index lookup once.
    type Hit = { fxId: number; chIndex: number };
    const hitsByAddr = new Map<number, Hit[]>();
    for (const fx of state.fixtures.values()) {
      if (fx.universe !== universe) continue;
      for (let i = 0; i < fx.channels; i++) {
        const addr = fx.address + i;
        const list = hitsByAddr.get(addr) ?? [];
        list.push({ fxId: fx.id, chIndex: i });
        hitsByAddr.set(addr, list);
      }
    }

    let liveValues = state.liveValues;
    const active = state.activeChannel;
    let dirty = false;

    for (const [address, value] of changes) {
      const hits = hitsByAddr.get(address);
      if (!hits) continue;
      for (const hit of hits) {
        // Skip while the user is actively dragging this channel.
        if (active && active.fxId === hit.fxId && active.ch === hit.chIndex) continue;
        const fx = state.fixtures.get(hit.fxId);
        if (!fx) continue;
        liveValues = setLiveByte(liveValues, hit.fxId, hit.chIndex, value, fx.channels);
        dirty = true;
      }
    }

    if (dirty) set({ liveValues });
  },

  flush() {
    const state = get();
    if (state.pendingWrites.size === 0) return;
    const ws = useVCStore.getState().wsClient;
    for (const [key, value] of state.pendingWrites) {
      const colon = key.indexOf(':');
      if (colon < 0) continue;
      const universe = parseInt(key.slice(0, colon), 10);
      const addr = parseInt(key.slice(colon + 1), 10);
      if (!Number.isFinite(universe) || !Number.isFinite(addr)) continue;
      // QLC+ Simple Desk CH| protocol uses 1-based addresses.
      ws?.send(`CH|${universe * 512 + addr + 1}|${value}`);
    }
    set({ pendingWrites: new Map() });
  },

  attachWS() {
    const ws = useVCStore.getState().wsClient;
    if (!ws) return () => {};
    return setupDmxWS(ws, {
      onDmxState: (universe, fixtureID, address, values) =>
        get().applyDmxState(universe, fixtureID, address, values),
      onDmxDelta: (universe, changes) => get().applyDmxDelta(universe, changes),
    });
  },

  subscribeFixtures(ids) {
    const ws = useVCStore.getState().wsClient;
    if (!ws) return;
    if (ids.length) sendDmxSubscribe(ws, ids);
    sendDmxHeartbeat(ws);
  },

  unsubscribeFixtures(ids) {
    const ws = useVCStore.getState().wsClient;
    if (ws && ids.length) sendDmxUnsubscribe(ws, ids);
  },
}));
