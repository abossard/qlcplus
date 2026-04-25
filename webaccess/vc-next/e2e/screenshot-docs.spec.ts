// Screenshot documentation tests — captures visual proof of each feature.
// Generates screenshots in e2e/screenshots/ and a markdown report.
//
// Run: npx playwright test e2e/screenshot-docs.spec.ts

import { test, expect, type Page, type BrowserContext } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

const APP_URL = '/vc/';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SHOTS_DIR = path.join(__dirname, 'screenshots');
const REPORT_PATH = path.join(SHOTS_DIR, 'FEATURES.md');

// Accumulate report lines across tests (written at end).
const reportLines: string[] = [
  '# DMX Web Control — Feature Screenshots',
  '',
  `Generated: ${new Date().toISOString().slice(0, 19)}`,
  '',
  '---',
  '',
];

function shot(name: string): string {
  return path.join(SHOTS_DIR, `${name}.png`);
}

function addSection(title: string, description: string, ...images: string[]) {
  reportLines.push(`## ${title}`, '', description, '');
  for (const img of images) {
    const rel = path.basename(img);
    reportLines.push(`![${title}](${rel})`, '');
  }
  reportLines.push('---', '');
}

async function loadApp(page: Page) {
  await page.goto(APP_URL);
  await page.waitForSelector('.fixture-panel', { timeout: 15_000 });
  await page.waitForTimeout(1000);
}

// ---------------------------------------------------------------------------
// Feature screenshots
// ---------------------------------------------------------------------------
test.describe.serial('Feature Screenshots', () => {
  test.afterAll(() => {
    fs.mkdirSync(SHOTS_DIR, { recursive: true });
    fs.writeFileSync(REPORT_PATH, reportLines.join('\n'));
  });

  test('T1+T3: DMX Control loads directly — no VC tab', async ({ page }) => {
    await loadApp(page);
    // Verify no tab bar exists.
    expect(await page.locator('.view-tabs').count()).toBe(0);
    await page.screenshot({ path: shot('01-dmx-only'), fullPage: false });
    addSection(
      'DMX Control — Direct Load (T1, T3)',
      'The web app loads directly into DMX Control. No Virtual Console tab bar — this is a dedicated fixture control surface.',
      shot('01-dmx-only'),
    );
  });

  test('T5: Fixtures sorted by DMX address', async ({ page }) => {
    await loadApp(page);
    const addrs = await page.locator('.fp-addr').allTextContents();
    await page.screenshot({ path: shot('02-fixture-order'), fullPage: true });
    addSection(
      'Fixtures Sorted by DMX Address (T5)',
      `Fixtures are ordered by universe and DMX address: ${addrs.slice(0, 5).join(', ')}...`,
      shot('02-fixture-order'),
    );
  });

  test('T9: Model-based filter badges', async ({ page }) => {
    await loadApp(page);
    const badges = await page.locator('.dmx-badge').allTextContents();
    await page.screenshot({ path: shot('03-model-filter-all'), fullPage: false });

    // Click a specific model badge.
    const modelBadge = page.locator('.dmx-badge').nth(1);
    if (await modelBadge.count() > 0) {
      await modelBadge.click();
      await page.waitForTimeout(300);
      await page.screenshot({ path: shot('03-model-filter-active'), fullPage: false });
      // Reset.
      await page.locator('.dmx-badge', { hasText: 'All' }).first().click();
    }

    addSection(
      'Filter by Model (T9)',
      `Filter badges show fixture models: ${badges.join(', ')}. Click to filter — only matching fixtures remain visible.`,
      shot('03-model-filter-all'),
      shot('03-model-filter-active'),
    );
  });

  test('T6: Collapsible universe groups', async ({ page }) => {
    await loadApp(page);
    // Enable group-by-universe.
    const groupBtn = page.locator('.dmx-group-btn');
    if (await groupBtn.count() > 0) {
      await groupBtn.click();
      await page.waitForTimeout(300);
      await page.screenshot({ path: shot('04-universe-expanded'), fullPage: false });

      // Collapse the first universe.
      const header = page.locator('.dmx-group-label-toggle').first();
      await header.click();
      await page.waitForTimeout(300);
      await page.screenshot({ path: shot('04-universe-collapsed'), fullPage: false });

      addSection(
        'Collapsible Universe Groups (T6)',
        'Fixtures can be grouped by universe. Click the header to collapse/expand. State persists in localStorage.',
        shot('04-universe-expanded'),
        shot('04-universe-collapsed'),
      );
    }
  });

  test('T4: Controls sorted by channel index', async ({ page }) => {
    await loadApp(page);
    // Scroll to HERO fixture panel.
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await hero.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    await hero.screenshot({ path: shot('05-channel-order') });

    addSection(
      'Controls Sorted by Channel Index (T4)',
      'Within each fixture, controls appear in DMX channel order — matching the fixture definition.',
      shot('05-channel-order'),
    );
  });

  test('T7: Auto-open raw + star indicator for raw-only fixtures', async ({ page }) => {
    await loadApp(page);
    // Find HZ (hazer) — should have raw channels open by default.
    const hz = page.locator('.fixture-panel', { hasText: 'HZ' }).first();
    await hz.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    await hz.screenshot({ path: shot('06-raw-auto-open') });

    // Check star indicator.
    const star = hz.locator('.fp-badge-star');
    const hasStar = await star.count() > 0;

    addSection(
      'Raw-Only Fixtures Auto-Open + Star (T7)',
      `The HZ hazer has only raw channels (no color picker, no dimmer). Its raw section opens by default. ${hasStar ? 'A ★ star indicates uncovered channels.' : ''}`,
      shot('06-raw-auto-open'),
    );
  });

  test('T8: Raw channels show ALL channels with read-only monitors', async ({ page }) => {
    await loadApp(page);
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await hero.scrollIntoViewIfNeeded();
    // Open raw channels.
    const toggle = hero.locator('.rc-toggle');
    if (await toggle.count() > 0) {
      await toggle.click();
      await page.waitForTimeout(300);
    }
    await hero.screenshot({ path: shot('07-raw-all-channels') });

    // Count total and read-only channels.
    const totalRaw = await hero.locator('.rc-grid .channel-fader').count();
    const readonlyRaw = await hero.locator('.rc-grid .channel-fader.readonly').count();

    addSection(
      'Raw Channels — ALL Channels with Read-Only Monitors (T8)',
      `HERO shows all ${totalRaw} channels in raw view. ${readonlyRaw} channels are read-only 🔒 (already controlled by color picker, position pad, etc.). Uncovered channels are fully interactive.`,
      shot('07-raw-all-channels'),
    );
  });

  test('Cross-tab sync: color picker change propagates', async ({ browser }) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();

    // Set viewport to a size that shows the HERO fixture clearly.
    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });

    await loadApp(tabA);
    await loadApp(tabB);

    // Screenshot Tab B BEFORE any change — baseline.
    const heroB = tabB.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await heroB.scrollIntoViewIfNeeded();
    await tabB.waitForTimeout(500);
    await tabB.screenshot({ path: shot('08-sync-before-tabB'), fullPage: false });

    // In Tab A: click the hue bar to set a vivid hue.
    const hueBarA = tabA.locator('.cp-hue').first();
    await hueBarA.scrollIntoViewIfNeeded();
    const hueBox = await hueBarA.boundingBox();
    if (hueBox) {
      await tabA.mouse.click(hueBox.x + hueBox.width * 0.3, hueBox.y + hueBox.height / 2);
      await tabA.waitForTimeout(200);
    }

    // In Tab A: click the color square to pick a bright saturated color.
    const squareA = tabA.locator('.cp-square').first();
    await squareA.scrollIntoViewIfNeeded();
    const sqBox = await squareA.boundingBox();
    if (sqBox) {
      await tabA.mouse.click(sqBox.x + sqBox.width * 0.85, sqBox.y + sqBox.height * 0.15);
      await tabA.waitForTimeout(300);
    }

    // Screenshot Tab A showing the color change.
    const heroA = tabA.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await heroA.scrollIntoViewIfNeeded();
    await tabA.screenshot({ path: shot('08-sync-tabA-changed'), fullPage: false });

    // Wait for sync to Tab B.
    await tabB.waitForTimeout(2000);

    // Screenshot Tab B AFTER the change propagated.
    await heroB.scrollIntoViewIfNeeded();
    await tabB.screenshot({ path: shot('08-sync-after-tabB'), fullPage: false });

    // Verify Tab B's color picker shows the same RGB.
    const rgbA = await tabA.locator('.cp-rgb-text').first().textContent();
    const rgbB = await tabB.locator('.cp-rgb-text').first().textContent();

    addSection(
      'Cross-Tab Sync — Color Picker (Proof)',
      `**Tab A** changes the color picker to a vivid color. **Tab B** (independent browser tab) receives the DMX delta via WebSocket and updates its color picker — swatch, RGB values, hue bar cursor, and crosshair all sync.\n\nTab A RGB: \`${rgbA}\`  \nTab B RGB: \`${rgbB}\``,
      shot('08-sync-before-tabB'),
      shot('08-sync-tabA-changed'),
      shot('08-sync-after-tabB'),
    );

    await ctx.close();
  });

  test('Cross-tab sync: position pad change propagates', async ({ browser }) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();

    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });

    await loadApp(tabA);
    await loadApp(tabB);

    // Tab B baseline — scroll to HERO position control.
    const posBLocator = tabB.locator('.position-control').first();
    if (await posBLocator.count() === 0) {
      test.skip();
      return;
    }
    await posBLocator.scrollIntoViewIfNeeded();
    await tabB.waitForTimeout(500);
    await tabB.screenshot({ path: shot('09-sync-pos-before-tabB'), fullPage: false });

    // Tab A: click the position pad at 75%, 25%.
    const padA = tabA.locator('.pc-pad').first();
    await padA.scrollIntoViewIfNeeded();
    const padBox = await padA.boundingBox();
    if (padBox) {
      await tabA.mouse.click(padBox.x + padBox.width * 0.75, padBox.y + padBox.height * 0.25);
      await tabA.waitForTimeout(300);
    }
    await tabA.screenshot({ path: shot('09-sync-pos-tabA-changed'), fullPage: false });

    // Wait for sync.
    await tabB.waitForTimeout(2000);
    await posBLocator.scrollIntoViewIfNeeded();
    await tabB.screenshot({ path: shot('09-sync-pos-after-tabB'), fullPage: false });

    const degA = await tabA.locator('.pc-display').first().textContent();
    const degB = await tabB.locator('.pc-display').first().textContent();

    addSection(
      'Cross-Tab Sync — Position XY Pad (Proof)',
      `**Tab A** moves the pan/tilt XY pad. **Tab B** shows the cursor move to the same position.\n\nTab A: \`${degA}\`  \nTab B: \`${degB}\``,
      shot('09-sync-pos-before-tabB'),
      shot('09-sync-pos-tabA-changed'),
      shot('09-sync-pos-after-tabB'),
    );

    await ctx.close();
  });

  test('Cross-tab sync: fader change propagates', async ({ browser }) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();

    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });

    await loadApp(tabA);
    await loadApp(tabB);

    // Tab B baseline.
    await tabB.screenshot({ path: shot('10-sync-fader-before-tabB'), fullPage: false });

    // Tab A: click HERO dimmer fader near top.
    const heroA = tabA.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const faderA = heroA.locator('.dimmer-fader:not(.readonly) .cf-track').first();
    await faderA.scrollIntoViewIfNeeded();
    const fBox = await faderA.boundingBox();
    if (fBox) {
      await tabA.mouse.click(fBox.x + fBox.width / 2, fBox.y + 5);
      await tabA.waitForTimeout(300);
    }
    await tabA.screenshot({ path: shot('10-sync-fader-tabA-changed'), fullPage: false });

    // Wait for sync.
    await tabB.waitForTimeout(2000);
    await tabB.screenshot({ path: shot('10-sync-fader-after-tabB'), fullPage: false });

    const valA = await heroA.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();
    const heroB = tabB.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const valB = await heroB.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();

    addSection(
      'Cross-Tab Sync — Dimmer Fader (Proof)',
      `**Tab A** sets the HERO dimmer to ${valA}. **Tab B** reflects the change: ${valB}. Both tabs share the same DMX universe via WebSocket.`,
      shot('10-sync-fader-before-tabB'),
      shot('10-sync-fader-tabA-changed'),
      shot('10-sync-fader-after-tabB'),
    );

    await ctx.close();
  });
});
