// HTTP client for the DMX-related REST endpoints exposed by the QLC+ web server.
import type { ChannelInfo, FixtureInfo } from './dmx-types';

async function fetchPatchPart<T>(url: string): Promise<{ revision: string; data: T }> {
  const res = await fetch(url, { cache: 'no-store' });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const revision = res.headers.get('X-QLC-Patch-Revision');
  if (!revision) throw new Error('Missing fixture patch revision');
  return { revision, data: await res.json() };
}

export async function fetchPresetValues(fixture: FixtureInfo, revision: string): Promise<Uint8Array> {
  const params = new URLSearchParams({ fixtureID: String(fixture.id), revision });
  const res = await fetch(`/api/preset-values?${params}`, { cache: 'no-store' });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const values = await res.json();
  if (!Array.isArray(values) || values.length !== fixture.channels)
    throw new Error('Incomplete fixture capture');
  return Uint8Array.from(values);
}

// Convenience: fetch fixtures and channels in parallel and merge channel detail
// onto each fixture under `channelsDetail`.
export async function fetchFixturesWithChannels() {
  const [fixtures, channels] = await Promise.all([
    fetchPatchPart<FixtureInfo[]>('/api/fixtures'),
    fetchPatchPart<ChannelInfo[]>('/api/channels'),
  ]);
  if (fixtures.revision !== channels.revision) throw new Error('Fixture patch changed during loading');

  const byFixture = new Map<number, ChannelInfo[]>();
  for (const ch of channels.data) {
    const list = byFixture.get(ch.fixtureID) ?? [];
    list.push(ch);
    byFixture.set(ch.fixtureID, list);
  }
  for (const list of byFixture.values()) {
    list.sort((a, b) => a.index - b.index);
  }

  const detailed = fixtures.data.map(fx => ({
    ...fx,
    channelsDetail: byFixture.get(fx.id) ?? [],
  }));
  return { revision: fixtures.revision, fixtures: detailed };
}
