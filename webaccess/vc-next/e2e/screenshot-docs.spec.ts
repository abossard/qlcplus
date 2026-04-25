// Screenshot documentation tests — captures visual proof of each feature.
// Screenshots are attached to test results via testInfo.attach() so the
// custom markdown reporter (markdown-reporter.ts) embeds them automatically.
//
// Run: npx playwright test e2e/screenshot-docs.spec.ts

import { test, expect, type Page } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

const APP_URL = '/vc/';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SHOTS_DIR = path.join(__dirname, 'screenshots');

async function loadApp(page: Page) {
  await page.goto(APP_URL);
  await page.waitForSelector('.fixture-panel', { timeout: 15_000 });
  await page.waitForTimeout(1000);
}

/** Take a screenshot, save to screenshots/ dir, and attach to testInfo. */
async function snap(
  page: Page,
  name: string,
  testInfo: { attach: (name: string, opts: { path?: string; body?: Buffer; contentType: string }) => Promise<void> },
  opts?: { fullPage?: boolean; element?: import('@playwright/test').Locator },
) {
  fs.mkdirSync(SHOTS_DIR, { recursive: true });
  const filePath = path.join(SHOTS_DIR, `${name}.png`);
  if (opts?.element) {
    await opts.element.screenshot({ path: filePath });
  } else {
    await page.screenshot({ path: filePath, fullPage: opts?.fullPage ?? false });
  }
  await testInfo.attach(name, { path: filePath, contentType: 'image/png' });
}

// ---------------------------------------------------------------------------
// Feature screenshots
// ---------------------------------------------------------------------------
test.describe.serial('Feature Screenshots', () => {
  test('T1+T3: DMX Control loads directly — no VC tab', async ({ page }, testInfo) => {
    await loadApp(page);
    expect(await page.locator('.view-tabs').count()).toBe(0);
    await snap(page, '01-dmx-only', testInfo);
  });

  test('T5: Fixtures sorted by DMX address', async ({ page }, testInfo) => {
    await loadApp(page);
    await snap(page, '02-fixture-order', testInfo, { fullPage: true });
  });

  test('T9: Model-based filter badges', async ({ page }, testInfo) => {
    await loadApp(page);
    await snap(page, '03-model-filter-all', testInfo);
    const modelBadge = page.locator('.dmx-badge').nth(1);
    if (await modelBadge.count() > 0) {
      await modelBadge.click();
      await page.waitForTimeout(300);
      await snap(page, '03-model-filter-active', testInfo);
      await page.locator('.dmx-badge', { hasText: 'All' }).first().click();
    }
  });

  test('T6: Collapsible universe groups', async ({ page }, testInfo) => {
    await loadApp(page);
    const groupBtn = page.locator('.dmx-group-btn');
    if (await groupBtn.count() > 0) {
      await groupBtn.click();
      await page.waitForTimeout(300);
      await snap(page, '04-universe-expanded', testInfo);
      const header = page.locator('.dmx-group-label-toggle').first();
      await header.click();
      await page.waitForTimeout(300);
      await snap(page, '04-universe-collapsed', testInfo);
    }
  });

  test('T4: Controls sorted by channel index', async ({ page }, testInfo) => {
    await loadApp(page);
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await hero.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    await snap(page, '05-channel-order', testInfo, { element: hero });
  });

  test('T7: Auto-open raw + star for raw-only fixtures', async ({ page }, testInfo) => {
    await loadApp(page);
    const hz = page.locator('.fixture-panel', { hasText: 'HZ' }).first();
    await hz.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    await snap(page, '06-raw-auto-open', testInfo, { element: hz });
  });

  test('T8: Raw channels = ALL channels with read-only monitors', async ({ page }, testInfo) => {
    await loadApp(page);
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await hero.scrollIntoViewIfNeeded();
    const toggle = hero.locator('.rc-toggle');
    if (await toggle.count() > 0) {
      await toggle.click();
      await page.waitForTimeout(300);
    }
    await snap(page, '07-raw-all-channels', testInfo, { element: hero });

    const totalRaw = await hero.locator('.rc-grid .channel-fader').count();
    const readonlyRaw = await hero.locator('.rc-grid .channel-fader.readonly').count();
    expect(totalRaw).toBe(23);
    expect(readonlyRaw).toBeGreaterThan(0);
  });

  test('Cross-tab sync: color picker change propagates', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });
    await loadApp(tabA);
    await loadApp(tabB);

    // Tab B baseline.
    await snap(tabB, '08-sync-color-before-tabB', testInfo);

    // Tab A: set a vivid color.
    const hueBar = tabA.locator('.cp-hue').first();
    await hueBar.scrollIntoViewIfNeeded();
    const hueBox = await hueBar.boundingBox();
    if (hueBox) {
      await tabA.mouse.click(hueBox.x + hueBox.width * 0.3, hueBox.y + hueBox.height / 2);
      await tabA.waitForTimeout(200);
    }
    const square = tabA.locator('.cp-square').first();
    await square.scrollIntoViewIfNeeded();
    const sqBox = await square.boundingBox();
    if (sqBox) {
      await tabA.mouse.click(sqBox.x + sqBox.width * 0.85, sqBox.y + sqBox.height * 0.15);
      await tabA.waitForTimeout(300);
    }
    await snap(tabA, '08-sync-color-tabA-changed', testInfo);

    // Wait for sync, then screenshot Tab B.
    await tabB.waitForTimeout(2000);
    await snap(tabB, '08-sync-color-after-tabB', testInfo);

    const rgbA = await tabA.locator('.cp-rgb-text').first().textContent();
    const rgbB = await tabB.locator('.cp-rgb-text').first().textContent();
    expect(rgbA).toBe(rgbB);
    await ctx.close();
  });

  test('Cross-tab sync: position pad change propagates', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });
    await loadApp(tabA);
    await loadApp(tabB);

    const posB = tabB.locator('.position-control').first();
    if (await posB.count() === 0) { test.skip(); return; }

    await snap(tabB, '09-sync-pos-before-tabB', testInfo);

    const padA = tabA.locator('.pc-pad').first();
    await padA.scrollIntoViewIfNeeded();
    const padBox = await padA.boundingBox();
    if (padBox) {
      await tabA.mouse.click(padBox.x + padBox.width * 0.75, padBox.y + padBox.height * 0.25);
      await tabA.waitForTimeout(300);
    }
    await snap(tabA, '09-sync-pos-tabA-changed', testInfo);

    await tabB.waitForTimeout(2000);
    await snap(tabB, '09-sync-pos-after-tabB', testInfo);

    const degA = await tabA.locator('.pc-display').first().textContent();
    const degB = await tabB.locator('.pc-display').first().textContent();
    expect(degA).toBe(degB);
    await ctx.close();
  });

  test('Cross-tab sync: fader change propagates', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 900 });
    await tabB.setViewportSize({ width: 800, height: 900 });
    await loadApp(tabA);
    await loadApp(tabB);

    await snap(tabB, '10-sync-fader-before-tabB', testInfo);

    const heroA = tabA.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const faderA = heroA.locator('.dimmer-fader:not(.readonly) .cf-track').first();
    await faderA.scrollIntoViewIfNeeded();
    const fBox = await faderA.boundingBox();
    if (fBox) {
      await tabA.mouse.click(fBox.x + fBox.width / 2, fBox.y + 5);
      await tabA.waitForTimeout(300);
    }
    await snap(tabA, '10-sync-fader-tabA-changed', testInfo);

    await tabB.waitForTimeout(2000);
    await snap(tabB, '10-sync-fader-after-tabB', testInfo);

    const valA = await heroA.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();
    const heroB = tabB.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const valB = await heroB.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();
    expect(valA).toBe(valB);
    await ctx.close();
  });
});
