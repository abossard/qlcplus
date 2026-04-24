// HTTP client for the DMX-related REST endpoints exposed by the QLC+ web server.
import type { ChannelInfo, FixtureInfo } from './dmx-types';

export async function fetchFixtures(): Promise<FixtureInfo[]> {
  const res = await fetch('/api/fixtures');
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

export async function fetchChannels(fixtureIDs?: number[]): Promise<ChannelInfo[]> {
  const params = fixtureIDs?.length ? `?fixtureIDs=${fixtureIDs.join(',')}` : '';
  const res = await fetch(`/api/channels${params}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

// Convenience: fetch fixtures and channels in parallel and merge channel detail
// onto each fixture under `channelsDetail`.
export async function fetchFixturesWithChannels(): Promise<FixtureInfo[]> {
  const [fixtures, channels] = await Promise.all([fetchFixtures(), fetchChannels()]);

  const byFixture = new Map<number, ChannelInfo[]>();
  for (const ch of channels) {
    const list = byFixture.get(ch.fixtureID) ?? [];
    list.push(ch);
    byFixture.set(ch.fixtureID, list);
  }
  for (const list of byFixture.values()) {
    list.sort((a, b) => a.index - b.index);
  }

  return fixtures.map(fx => ({
    ...fx,
    channelsDetail: byFixture.get(fx.id) ?? [],
  }));
}
