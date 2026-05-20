// localStorage-backed preset save/recall keyed by fixture identity
// ("manufacturer|model|mode") so presets travel across patch shuffles.

import type { FixtureInfo } from './dmx-types';

const STORAGE_KEY = 'qlcplus.dmx.presets.v1';

export interface PresetEntry {
  id: string;
  name: string;
  fixtureKey: string;
  values: { channel: number; value: number }[];
  createdAt: number;
}

export function fixtureKey(fx: FixtureInfo): string {
  return `${fx.manufacturer ?? '?'}|${fx.model ?? '?'}|${fx.mode ?? '?'}`;
}

function readAll(): PresetEntry[] {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function writeAll(presets: PresetEntry[]): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(presets));
  } catch {
    // Storage may be full or disabled; presets are best-effort.
  }
}

function newId(): string {
  return `p_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 8)}`;
}

export function savePreset(fx: FixtureInfo, name: string, values: Uint8Array): PresetEntry {
  const all = readAll();
  const entry: PresetEntry = {
    id: newId(),
    name,
    fixtureKey: fixtureKey(fx),
    values: Array.from(values).map((v, i) => ({ channel: i, value: v })),
    createdAt: Date.now(),
  };
  all.push(entry);
  writeAll(all);
  return entry;
}

export function getPresets(fx: FixtureInfo): PresetEntry[] {
  const key = fixtureKey(fx);
  return readAll()
    .filter(p => p.fixtureKey === key)
    .sort((a, b) => b.createdAt - a.createdAt);
}

export function recallPreset(presetId: string): { channel: number; value: number }[] | null {
  const entry = readAll().find(p => p.id === presetId);
  return entry ? entry.values : null;
}

export function deletePreset(presetId: string): void {
  writeAll(readAll().filter(p => p.id !== presetId));
}

export function exportPresets(): string {
  return JSON.stringify(readAll(), null, 2);
}

export function importPresets(json: string): number {
  let incoming: PresetEntry[];
  try {
    const parsed = JSON.parse(json);
    if (!Array.isArray(parsed)) return 0;
    incoming = parsed as PresetEntry[];
  } catch {
    return 0;
  }
  const existing = readAll();
  const seen = new Set(existing.map(p => p.id));
  let added = 0;
  for (const p of incoming) {
    if (!p || typeof p !== 'object' || !p.id || !p.fixtureKey) continue;
    if (seen.has(p.id)) continue;
    existing.push(p);
    seen.add(p.id);
    added++;
  }
  writeAll(existing);
  return added;
}
