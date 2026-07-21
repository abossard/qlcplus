// E2E tests for the web-based DMX Control Panel.
// Requires QLC+ running with: ./qlcplus-qml -w -o GARAGE.qxw

import { test, expect, type Page } from '@playwright/test';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
const APP_URL = '/vc/';

async function loadApp(page: Page) {
  await page.goto(APP_URL);
  // DMX is the only view — wait for it to render.
  await page.waitForSelector('.dmx-view', { timeout: 10_000 });
  await page.locator('.fixture-panel').first().waitFor({ timeout: 15_000 });
}

async function setFunctionStatus(page: Page, functionId: number, running: boolean) {
  await page.evaluate(
    ([id, status]) => new Promise<void>((resolve, reject) => {
      const socket = new WebSocket(`ws://${window.location.host}/qlcplusWS`);
      const expected = status ? 'Running' : 'Stopped';
      let poll = 0;
      const timeout = window.setTimeout(() => {
        window.clearInterval(poll);
        socket.close();
        reject(new Error(`Function ${id} did not become ${expected}`));
      }, 5_000);
      socket.onopen = () => {
        socket.send(`QLC+API|setFunctionStatus|${id}|${status ? 1 : 0}`);
        const requestStatus = () => socket.send(`QLC+API|getFunctionStatus|${id}`);
        requestStatus();
        poll = window.setInterval(requestStatus, 50);
      };
      socket.onmessage = event => {
        if (String(event.data) !== `QLC+API|getFunctionStatus|${expected}`) return;
        window.clearTimeout(timeout);
        window.clearInterval(poll);
        socket.close();
        resolve();
      };
      socket.onerror = () => {
        window.clearTimeout(timeout);
        window.clearInterval(poll);
        reject(new Error('Unable to connect to the QLC+ WebSocket'));
      };
    }),
    [functionId, running] as const,
  );
}

async function sendWebAccessCommand(page: Page, command: string) {
  await page.evaluate(
    cmd => new Promise<void>((resolve, reject) => {
      const socket = new WebSocket(`ws://${window.location.host}/qlcplusWS`);
      socket.onopen = () => {
        socket.send(cmd);
        socket.close();
        resolve();
      };
      socket.onerror = () => reject(new Error('Unable to connect to the QLC+ WebSocket'));
    }),
    command,
  );
}

async function resetChannelAndWait(page: Page, absoluteChannel: number) {
  await page.evaluate(
    channel => new Promise<void>((resolve, reject) => {
      const socket = new WebSocket(`ws://${window.location.host}/qlcplusWS`);
      const timeout = window.setTimeout(() => {
        socket.close();
        reject(new Error(`Reset ${channel} was not acknowledged`));
      }, 5_000);
      socket.onopen = () =>
        socket.send(`QLC+API|sdResetChannel|${channel}`);
      socket.onmessage = event => {
        if (!String(event.data).startsWith('QLC+API|getChannelsValues|')) return;
        window.clearTimeout(timeout);
        socket.close();
        resolve();
      };
      socket.onerror = () => {
        window.clearTimeout(timeout);
        reject(new Error('Unable to connect to the QLC+ WebSocket'));
      };
    }),
    absoluteChannel,
  );
}

// Dispatch synthetic pointer events on a fader track. Stubs setPointerCapture
// because synthetic PointerEvents have no active pointer state, which would
// otherwise cause React's onPointerDown handler to throw before updating state.
async function pointerClickAt(track: ReturnType<Page['locator']>, verticalPct: number) {
  await track.scrollIntoViewIfNeeded();
  await track.evaluate((el: HTMLElement, pct: number) => {
    const orig = (Element.prototype as any).setPointerCapture;
    (Element.prototype as any).setPointerCapture = function () { /* no-op */ };
    try {
      const r = el.getBoundingClientRect();
      const opts: PointerEventInit = {
        bubbles: true, cancelable: true, pointerId: 1, pointerType: 'mouse',
        clientX: r.left + r.width / 2,
        clientY: r.top + r.height * pct,
        button: 0, buttons: 1, isPrimary: true,
      };
      el.dispatchEvent(new PointerEvent('pointerdown', opts));
      el.dispatchEvent(new PointerEvent('pointerup', { ...opts, buttons: 0 }));
    } finally {
      (Element.prototype as any).setPointerCapture = orig;
    }
  }, verticalPct);
}

async function pointerDragRange(
  track: ReturnType<Page['locator']>,
  startPct: number,
  endPct: number,
  steps = 14,
) {
  await track.scrollIntoViewIfNeeded();
  await track.evaluate((el: HTMLElement, [s, e, n]: [number, number, number]) => {
    const orig = (Element.prototype as any).setPointerCapture;
    (Element.prototype as any).setPointerCapture = function () { /* no-op */ };
    try {
      const r = el.getBoundingClientRect();
      const x = r.left + r.width / 2;
      const yOf = (p: number) => r.top + r.height * p;
      const base: PointerEventInit = {
        bubbles: true, cancelable: true, pointerId: 1, pointerType: 'mouse',
        clientX: x, clientY: yOf(s), button: 0, buttons: 1, isPrimary: true,
      };
      el.dispatchEvent(new PointerEvent('pointerdown', base));
      for (let i = 1; i <= n; i++) {
        const p = s + ((e - s) * i) / n;
        el.dispatchEvent(new PointerEvent('pointermove', { ...base, clientY: yOf(p) }));
      }
      el.dispatchEvent(new PointerEvent('pointerup', { ...base, clientY: yOf(e), buttons: 0 }));
    } finally {
      (Element.prototype as any).setPointerCapture = orig;
    }
  }, [startPct, endPct, steps] as [number, number, number]);
}

// ---------------------------------------------------------------------------
// 1. Shell & Navigation
// ---------------------------------------------------------------------------
test.describe('App Shell', () => {
  test('T1: no tab bar — DMX view loads directly', async ({ page }) => {
    await page.goto(APP_URL);
    // Wait for app to render.
    await page.waitForSelector('.topbar', { timeout: 10_000 });
    // No legacy view-tabs element.
    await expect(page.locator('.view-tabs')).toHaveCount(0);
    await expect(page.locator('.view-tab')).toHaveCount(0);
    // DMX view loaded directly (no tab click required).
    await expect(page.locator('.dmx-view')).toBeVisible({ timeout: 10_000 });
  });

  test('shows connection status indicator', async ({ page }) => {
    await loadApp(page);
    await expect(page.locator('.status-dot')).toBeVisible();
  });

  test('topbar and grand master are visible', async ({ page }) => {
    await loadApp(page);
    await expect(page.locator('.topbar')).toBeVisible();
    await expect(page.locator('.grand-master')).toBeVisible();
  });
});

// ---------------------------------------------------------------------------
// 2. DMX Control Panel — Fixture Loading
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Fixtures', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('loads and displays fixture panels from GARAGE workspace', async ({ page }) => {
    const panels = page.locator('.fixture-panel');
    await expect(panels.first()).toBeVisible({ timeout: 15_000 });
    expect(await panels.count()).toBe(8);
  });

  test('fixture panels show name and metadata', async ({ page }) => {
    const panel = page.locator('.fixture-panel').first();
    await expect(panel.locator('.fp-title')).toBeVisible();
    const name = await panel.locator('.fp-title').textContent();
    expect(name?.trim().length).toBeGreaterThan(0);

    const addr = panel.locator('.fp-addr');
    await expect(addr).toBeVisible();
    expect(await addr.textContent()).toMatch(/U\d+·\d+/);
  });

  test('fixture panels have a reset button', async ({ page }) => {
    const resetBtn = page.locator('.fixture-panel').first().locator('.fp-reset');
    await expect(resetBtn).toBeVisible();
    await expect(resetBtn).toHaveAttribute('title', 'Reset all channels');
  });

  test('fixture count is displayed in toolbar', async ({ page }) => {
    const text = await page.locator('.dmx-count').textContent();
    expect(text).toMatch(/\d+\s*\/\s*\d+/);
  });

  test('T5: fixtures sorted by universe·address', async ({ page }) => {
    const addrs = await page.locator('.fixture-panel .fp-addr').allTextContents();
    expect(addrs.length).toBeGreaterThan(0);

    // Parse "U<u>·<a>" → tuple, verify ascending sort.
    const tuples = addrs.map(t => {
      const m = t.match(/U(\d+)·(\d+)/);
      if (!m) throw new Error(`Bad address text: ${t}`);
      return [Number(m[1]), Number(m[2])] as [number, number];
    });
    for (let i = 1; i < tuples.length; i++) {
      const [pU, pA] = tuples[i - 1];
      const [u, a] = tuples[i];
      expect(u > pU || (u === pU && a >= pA)).toBeTruthy();
    }
  });

  test('T4: controls are sorted by channel within a fixture', async ({ page, request }) => {
    // Use HERO (id=0, 23 channels) — open raw to expose all channels in order.
    const heroPanel = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await expect(heroPanel).toBeVisible({ timeout: 15_000 });

    // Ensure raw section is open.
    const toggle = heroPanel.locator('.rc-toggle');
    const expanded = await toggle.getAttribute('aria-expanded');
    if (expanded !== 'true') {
      await toggle.click();
      await page.waitForTimeout(200);
    }

    const rawNames = await heroPanel.locator('.rc-grid .channel-fader .cf-name').allTextContents();
    expect(rawNames.length).toBe(23);

    // Compare with the expected channel order from the API.
    const resp = await request.get('/api/channels');
    const allCh = await resp.json() as Array<{ fixtureID: number; index: number; name: string }>;
    const heroCh = allCh.filter(c => c.fixtureID === 0).sort((a, b) => a.index - b.index);
    expect(heroCh.length).toBe(23);
    const expected = heroCh.map(c => c.name);
    expect(rawNames.map(s => s.trim())).toEqual(expected.map(s => s.trim()));
  });
});

// ---------------------------------------------------------------------------
// 3. DMX Panel — Search & Model Filter (T9)
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Search & Filter', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('search input filters fixtures by name', async ({ page }) => {
    const totalBefore = await page.locator('.fixture-panel').count();
    await page.locator('.dmx-search').fill('HERO');
    await page.waitForTimeout(300);
    const after = await page.locator('.fixture-panel').count();
    expect(after).toBeLessThan(totalBefore);
    expect(after).toBeGreaterThanOrEqual(1);
    const title = await page.locator('.fixture-panel .fp-title').first().textContent();
    expect(title?.toUpperCase()).toContain('HERO');
  });

  test('clearing search restores all fixtures', async ({ page }) => {
    const totalBefore = await page.locator('.fixture-panel').count();
    await page.locator('.dmx-search').fill('HERO');
    await page.waitForTimeout(300);
    await page.locator('.dmx-search').fill('');
    await page.waitForTimeout(300);
    expect(await page.locator('.fixture-panel').count()).toBe(totalBefore);
  });

  test('T9: badges show model names (not types)', async ({ page }) => {
    const badges = page.locator('.dmx-badge');
    const count = await badges.count();
    expect(count).toBeGreaterThan(1);

    const texts = await badges.allTextContents();
    // First badge is "All".
    expect(texts[0]).toMatch(/^All$/);

    // Model names from GARAGE — these should appear.
    // (Types like "Moving Head", "Hazer", "Strobe", "LED Bar (Pixels)" should NOT be the badge label.)
    const labels = texts.slice(1).map(s => s.trim());
    expect(labels).toContain('Hero Spot Wash 140 2in1 RGBW+W');
    expect(labels).toContain('Hz-200 DMX');
    expect(labels).toContain('RGBPanel');
    expect(labels).toContain('Beam Ball 100 Quad LED');

    // Verify no badge uses bare type labels.
    for (const l of labels) {
      expect(l).not.toBe('Moving Head');
      expect(l).not.toBe('Hazer');
      expect(l).not.toBe('Strobe');
    }
  });

  test('T9: clicking a model badge filters to that model', async ({ page }) => {
    const totalBefore = await page.locator('.fixture-panel').count();

    // Click the "RGBPanel" badge — should leave 4 WLED fixtures.
    await page.locator('.dmx-badge', { hasText: /^RGBPanel$/ }).click();
    await page.waitForTimeout(300);
    const filtered = await page.locator('.fixture-panel').count();
    expect(filtered).toBe(4);
    expect(filtered).toBeLessThan(totalBefore);

    // Each remaining panel should have name starting with "WLED".
    const titles = await page.locator('.fixture-panel .fp-title').allTextContents();
    for (const t of titles) {
      expect(t).toMatch(/^WLED/);
    }

    // Click "All" — restores everything.
    await page.locator('.dmx-badge', { hasText: /^All$/ }).click();
    await page.waitForTimeout(300);
    expect(await page.locator('.fixture-panel').count()).toBe(totalBefore);
  });
});

// ---------------------------------------------------------------------------
// 4. T6: Collapsible universe groups
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Universe Grouping', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('T6: universe headers collapse and expand', async ({ page }) => {
    // Activate "group by universe" (multiple universes exist in GARAGE).
    const groupBtn = page.locator('.dmx-group-btn');
    await expect(groupBtn).toBeVisible();
    await groupBtn.click();
    await page.waitForTimeout(200);

    // Universe headers should appear.
    const headers = page.locator('.dmx-group-label-toggle');
    const headerCount = await headers.count();
    expect(headerCount).toBeGreaterThan(1);

    const firstGroup = page.locator('.dmx-group').nth(0);
    const firstHeader = firstGroup.locator('.dmx-group-label-toggle');
    await expect(firstHeader).toHaveAttribute('aria-expanded', 'true');

    const panelsInGroup = firstGroup.locator('.fixture-panel');
    const before = await panelsInGroup.count();
    expect(before).toBeGreaterThan(0);

    // Collapse: panels should disappear (the .dmx-grid is no longer rendered).
    await firstHeader.click();
    await page.waitForTimeout(200);
    await expect(firstHeader).toHaveAttribute('aria-expanded', 'false');
    expect(await panelsInGroup.count()).toBe(0);

    // Expand again.
    await firstHeader.click();
    await page.waitForTimeout(200);
    await expect(firstHeader).toHaveAttribute('aria-expanded', 'true');
    expect(await panelsInGroup.count()).toBe(before);
  });
});

// ---------------------------------------------------------------------------
// 5. DMX Panel — Control Components
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Controls', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('channel faders are rendered for fixtures', async ({ page }) => {
    const faders = page.locator('.channel-fader, .dimmer-fader');
    await expect(faders.first()).toBeVisible({ timeout: 5_000 });
    expect(await faders.count()).toBeGreaterThanOrEqual(1);
  });

  test('fader shows value readout and name label', async ({ page }) => {
    const fader = page.locator('.channel-fader, .dimmer-fader').first();
    await expect(fader.locator('.cf-value')).toBeVisible();
    const label = await fader.locator('.cf-name').textContent();
    expect(label?.trim().length).toBeGreaterThan(0);
  });

  test('fader track has ARIA slider role', async ({ page }) => {
    const track = page.locator('.cf-track').first();
    await expect(track).toBeVisible({ timeout: 5_000 });
    await expect(track).toHaveAttribute('role', 'slider');
  });

  test('clicking a fader track updates the value', async ({ page }) => {
    // Use HERO's dimmer fader — known writable, not inside another widget.
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const fader = hero.locator('.dimmer-fader:not(.readonly) .cf-track').first();
    await expect(fader).toBeVisible({ timeout: 5_000 });
    await fader.scrollIntoViewIfNeeded();

    // Use helper that stubs setPointerCapture (synthetic events have no
    // active pointer state, which would otherwise make React's handler throw).
    await pointerClickAt(fader, 0.025);

    // Poll because WS DMX_DELTA echoes can briefly override optimistic state.
    await expect.poll(
      async () => Number(await fader.getAttribute('aria-valuenow') ?? '0'),
      { timeout: 3_000, intervals: [50, 100, 200] }
    ).toBeGreaterThan(150);
  });

  test('color picker is rendered for RGB fixtures', async ({ page }) => {
    const picker = page.locator('.color-picker').first();
    await expect(picker).toBeVisible();
    await expect(picker.locator('.cp-swatch')).toBeVisible();
    await expect(picker.locator('.cp-square')).toBeVisible();
    await expect(picker.locator('.cp-hue')).toBeVisible();
  });

  test('color picker square responds to pointer interaction', async ({ page }) => {
    const square = page.locator('.cp-square').first();
    await square.scrollIntoViewIfNeeded();
    const hueBar = page.locator('.cp-hue').first();
    await hueBar.scrollIntoViewIfNeeded();
    const hueBox = await hueBar.boundingBox();
    if (hueBox) {
      await page.mouse.click(hueBox.x + hueBox.width * 0.3, hueBox.y + hueBox.height / 2);
      await page.waitForTimeout(200);
    }
    const rgbText = page.locator('.cp-rgb-text').first();
    await square.scrollIntoViewIfNeeded();
    const box = await square.boundingBox();
    if (box) {
      await page.mouse.click(box.x + box.width * 0.9, box.y + box.height * 0.1);
      await page.waitForTimeout(300);
    }
    const after = await rgbText.textContent();
    expect(after).toMatch(/[1-9]\d*/);
  });

  test('position control is rendered for moving heads', async ({ page }) => {
    const posCtrl = page.locator('.position-control').first();
    await expect(posCtrl).toBeVisible();
    await expect(posCtrl.locator('.pc-pad')).toBeVisible();
    await expect(posCtrl.locator('.pc-display')).toBeVisible();
  });

  test('position XY pad responds to click', async ({ page }) => {
    const pad = page.locator('.pc-pad').first();
    await pad.scrollIntoViewIfNeeded();
    const box = await pad.boundingBox();
    if (box) {
      await page.mouse.click(box.x + box.width * 0.75, box.y + box.height * 0.25);
      await page.waitForTimeout(400);
    }
    const text = await page.locator('.pc-display').first().textContent();
    const degrees = text?.match(/\d+/g)?.map(Number) ?? [];
    if (box) {
      expect(degrees.some(d => d > 0)).toBeTruthy();
    }
  });
});

// ---------------------------------------------------------------------------
// 6. T7: Auto-open raw + star indicator
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Raw Auto-Open & Star', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('T7: HZ (raw-only fixture) auto-opens raw channels', async ({ page }) => {
    const hz = page.locator('.fixture-panel', { hasText: 'HZ ' }).first();
    await expect(hz).toBeVisible({ timeout: 10_000 });

    const toggle = hz.locator('.rc-toggle');
    await expect(toggle).toBeVisible();
    await expect(toggle).toHaveAttribute('aria-expanded', 'true');
    // Grid is rendered.
    await expect(hz.locator('.rc-grid')).toBeVisible();
  });

  test('T7: HERO (has primary controls) starts with raw closed', async ({ page }) => {
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await expect(hero).toBeVisible({ timeout: 10_000 });

    const toggle = hero.locator('.rc-toggle');
    await expect(toggle).toBeVisible();
    await expect(toggle).toHaveAttribute('aria-expanded', 'false');
    await expect(hero.locator('.rc-grid')).toHaveCount(0);
  });

  test('T7: star indicator appears on fixtures with uncovered channels', async ({ page }) => {
    // At least one fixture should have a star (e.g., Beam Ball with 49 channels).
    const stars = page.locator('.fp-badge-star');
    const count = await stars.count();
    expect(count).toBeGreaterThanOrEqual(1);

    // Verify the star sits inside an .fp-badge inside a fixture panel.
    const firstStar = stars.first();
    await expect(firstStar).toBeVisible();
    await expect(firstStar).toHaveAttribute('title', 'Has uncovered channels');
  });
});

// ---------------------------------------------------------------------------
// 7. T8: Raw shows ALL channels, with read-only ones for covered channels
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Raw All-Channels & Read-Only', () => {
  test.beforeEach(async ({ page }) => {
    await loadApp(page);
  });

  test('T8: HERO raw lists all 23 channels', async ({ page }) => {
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await expect(hero).toBeVisible({ timeout: 10_000 });

    const toggle = hero.locator('.rc-toggle');
    if ((await toggle.getAttribute('aria-expanded')) !== 'true') {
      await toggle.click();
      await page.waitForTimeout(200);
    }
    // Toggle label includes the channel count.
    expect(await toggle.textContent()).toContain('(23)');

    const faders = hero.locator('.rc-grid .channel-fader');
    expect(await faders.count()).toBe(23);
  });

  test('T8: HERO raw has some read-only channels (covered by primary controls)', async ({ page }) => {
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const toggle = hero.locator('.rc-toggle');
    if ((await toggle.getAttribute('aria-expanded')) !== 'true') {
      await toggle.click();
      await page.waitForTimeout(200);
    }

    const all = hero.locator('.rc-grid .channel-fader');
    const ro = hero.locator('.rc-grid .channel-fader.readonly');
    const total = await all.count();
    const roCount = await ro.count();
    expect(roCount).toBeGreaterThan(0);
    expect(roCount).toBeLessThan(total);

    // Read-only fader value readout shows the lock emoji.
    const firstRoValue = await ro.first().locator('.cf-value').textContent();
    expect(firstRoValue ?? '').toContain('🔒');
  });

  test('T8: read-only fader does not respond to clicks', async ({ page }) => {
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const toggle = hero.locator('.rc-toggle');
    if ((await toggle.getAttribute('aria-expanded')) !== 'true') {
      await toggle.click();
      await page.waitForTimeout(200);
    }

    const ro = hero.locator('.rc-grid .channel-fader.readonly').first();
    await ro.scrollIntoViewIfNeeded();
    const track = ro.locator('.cf-track');
    const before = Number(await track.getAttribute('aria-valuenow'));

    const box = await track.boundingBox();
    if (box) {
      await page.mouse.click(box.x + box.width / 2, box.y + 5);
      await page.waitForTimeout(200);
    }
    // Clicking a readonly track should not write a new value via this UI;
    // value comes only from live DMX state, so it should not jump to ~max.
    const after = Number(await track.getAttribute('aria-valuenow'));
    expect(after).toBe(before);
  });
});

// ---------------------------------------------------------------------------
// 8. DMX Panel — Presets
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Presets', () => {
  test('preset save button is visible on fixture panels', async ({ page }) => {
    await loadApp(page);
    const saveBtn = page.locator('.fp-preset-save').first();
    await expect(saveBtn).toBeVisible();
    await expect(saveBtn).toContainText(/save/i);
  });
});

// ---------------------------------------------------------------------------
// 9. DMX Panel — Raw Channels collapsibility (existing behavior)
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Raw Channels', () => {
  test('raw channels section is collapsible', async ({ page }) => {
    await loadApp(page);
    // Use HERO — starts closed.
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const toggle = hero.locator('.rc-toggle');
    await expect(toggle).toBeVisible();
    await expect(toggle).toHaveAttribute('aria-expanded', 'false');
    await expect(hero.locator('.rc-grid')).toHaveCount(0);

    await toggle.click();
    await page.waitForTimeout(200);
    await expect(toggle).toHaveAttribute('aria-expanded', 'true');
    await expect(hero.locator('.rc-grid')).toBeVisible();

    await toggle.click();
    await page.waitForTimeout(200);
    await expect(toggle).toHaveAttribute('aria-expanded', 'false');
    await expect(hero.locator('.rc-grid')).toHaveCount(0);
  });
});

// ---------------------------------------------------------------------------
// 10. REST API — Direct endpoint tests
// ---------------------------------------------------------------------------
test.describe('REST API', () => {
  test('GET /api/fixtures returns GARAGE fixtures', async ({ request }) => {
    const resp = await request.get('/api/fixtures');
    expect(resp.ok()).toBeTruthy();
    const data = await resp.json();
    expect(Array.isArray(data)).toBeTruthy();
    expect(data.length).toBe(8);
    const fx = data[0];
    expect(fx).toHaveProperty('id');
    expect(fx).toHaveProperty('name');
    expect(fx).toHaveProperty('universe');
    expect(fx).toHaveProperty('address');
    expect(fx).toHaveProperty('channels');
  });

  test('GET /api/channels returns channel details', async ({ request }) => {
    const resp = await request.get('/api/channels');
    expect(resp.ok()).toBeTruthy();
    const data = await resp.json();
    expect(Array.isArray(data)).toBeTruthy();
    expect(data.length).toBeGreaterThan(0);
    const ch = data[0];
    expect(ch).toHaveProperty('fixtureID');
    expect(ch).toHaveProperty('index');
    expect(ch).toHaveProperty('name');
    expect(ch).toHaveProperty('group');
  });

  test('GET /vc/ serves the SPA index.html', async ({ request }) => {
    const resp = await request.get('/vc/');
    expect(resp.ok()).toBeTruthy();
    const html = await resp.text();
    expect(html).toContain('<div id="root">');
  });
});

// ---------------------------------------------------------------------------
// 11. Fader drag interaction (pointer events)
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Fader Drag', () => {
  test('dragging a fader changes its value continuously', async ({ page }) => {
    await loadApp(page);
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const track = hero.locator('.dimmer-fader:not(.readonly) .cf-track').first();
    await expect(track).toBeVisible({ timeout: 5_000 });
    await track.scrollIntoViewIfNeeded();

    // Drag from the bottom to the top via pointer-event helper.
    await pointerDragRange(track, 0.975, 0.025, 16);

    // Poll because WS DMX_DELTA echoes can briefly override optimistic state.
    await expect.poll(
      async () => Number(await track.getAttribute('aria-valuenow') ?? '0'),
      { timeout: 3_000, intervals: [50, 100, 200] }
    ).toBeGreaterThan(100);
  });
});

// ---------------------------------------------------------------------------
// 12. Reset fixture
// ---------------------------------------------------------------------------
test.describe('DMX Panel — Reset', () => {
  test('reset releases the override and restores the underlying DMX value', async ({ page }) => {
    await page.addInitScript(() => {
      const messages: string[] = [];
      (window as any).__qlcSentMessages = messages;
      const send = WebSocket.prototype.send;
      WebSocket.prototype.send = function (data) {
        messages.push(String(data));
        return send.call(this, data);
      };
    });
    await loadApp(page);
    const fixture = page.locator('.fixture-panel', { hasText: 'WLED - Row 1' }).first();
    const rawToggle = fixture.locator('.rc-toggle');
    if ((await rawToggle.getAttribute('aria-expanded')) !== 'true') {
      await rawToggle.click();
    }
    const track = fixture.locator('.rc-grid .cf-track[aria-label="Red 1"]').first();
    await expect(track).toBeVisible({ timeout: 5_000 });
    await track.scrollIntoViewIfNeeded();

    const underlyingValue = 255;
    const overrideValue = 64;
    try {
      await setFunctionStatus(page, 1, true);
      await expect.poll(
        async () => Number(await track.getAttribute('aria-valuenow') ?? '0'),
        { timeout: 5_000, intervals: [25, 50, 100] },
      ).toBe(underlyingValue);

      // Universe 2, channel 1 => absolute one-based channel 513.
      await sendWebAccessCommand(page, `CH|513|${overrideValue}`);
      await expect.poll(
        async () => Number(await track.getAttribute('aria-valuenow') ?? '0'),
        { timeout: 3_000, intervals: [25, 50, 100] },
      ).toBe(overrideValue);

      await fixture.locator('.fp-reset').click();
      await expect.poll(
        () => page.evaluate(() =>
          (window as any).__qlcSentMessages.includes('QLC+API|sdResetChannel|513')),
        { timeout: 2_000, intervals: [10, 25, 50] },
      ).toBeTruthy();
      await expect.poll(
        async () => Number(await track.getAttribute('aria-valuenow') ?? '0'),
        { timeout: 5_000, intervals: [25, 50, 100] },
      ).toBe(underlyingValue);
    } finally {
      await setFunctionStatus(page, 1, false);
    }
  });

  test('invalid reset address leaves a seeded valid channel unchanged', async ({ page }) => {
    await loadApp(page);
    const fixture = page.locator('.fixture-panel', { hasText: 'WLED - Row 1' }).first();
    const rawToggle = fixture.locator('.rc-toggle');
    if ((await rawToggle.getAttribute('aria-expanded')) !== 'true') {
      await rawToggle.click();
    }
    const track = fixture.locator('.rc-grid .cf-track[aria-label="Red 1"]').first();
    await expect(track).toBeVisible({ timeout: 5_000 });

    const seededValue = 64;
    await setFunctionStatus(page, 1, true);
    try {
      await sendWebAccessCommand(page, `CH|513|${seededValue}`);
      await expect.poll(
        async () => Number(await track.getAttribute('aria-valuenow') ?? '0'),
        { timeout: 3_000, intervals: [25, 50, 100] },
      ).toBe(seededValue);

      await resetChannelAndWait(page, 999_999);
      await expect(track).toHaveAttribute('aria-valuenow', String(seededValue));
      console.log(`Invalid reset 999999 preserved channel 513 at ${seededValue}`);
    } finally {
      await resetChannelAndWait(page, 513);
      await setFunctionStatus(page, 1, false);
    }
  });
});
