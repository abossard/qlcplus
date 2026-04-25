// Animated GIF documentation tests — produces looping GIFs showing
// cross-tab sync and feature interactions in action.
//
// Run: npx playwright test e2e/animated-docs.spec.ts

import { test, expect, type Page } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';
import { pngsToGif } from './make-gif';

const APP_URL = '/vc/';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SHOTS_DIR = path.join(__dirname, 'screenshots');
const FRAMES_DIR = path.join(SHOTS_DIR, 'frames');

async function loadApp(page: Page) {
  await page.goto(APP_URL);
  await page.waitForSelector('.fixture-panel', { timeout: 15_000 });
  await page.waitForTimeout(1000);
}

/** Take a numbered frame screenshot and attach it. */
async function frame(
  page: Page,
  name: string,
  testInfo: { attach: (n: string, o: { path: string; contentType: string }) => Promise<void> },
): Promise<string> {
  fs.mkdirSync(FRAMES_DIR, { recursive: true });
  const filePath = path.join(FRAMES_DIR, `${name}.png`);
  await page.screenshot({ path: filePath, fullPage: false });
  await testInfo.attach(name, { path: filePath, contentType: 'image/png' });
  return filePath;
}

/** Build a GIF from frame paths and attach it. */
async function buildGif(
  name: string,
  frames: string[],
  testInfo: { attach: (n: string, o: { path: string; contentType: string }) => Promise<void> },
  opts?: { delay?: number },
) {
  const gifPath = path.join(SHOTS_DIR, `${name}.gif`);
  pngsToGif(frames, gifPath, { delay: opts?.delay ?? 1000, scale: 0.5, maxColors: 128 });
  await testInfo.attach(name, { path: gifPath, contentType: 'image/gif' });
  return gifPath;
}

// ---------------------------------------------------------------------------
test.describe.serial('Animated Documentation', () => {

  test('Cross-tab color sync animation', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 700 });
    await tabB.setViewportSize({ width: 800, height: 700 });
    await loadApp(tabA);
    await loadApp(tabB);

    const frames: string[] = [];

    // Frame 1: Tab B baseline (both tabs at default).
    frames.push(await frame(tabB, 'color-sync-01-before', testInfo));

    // Frame 2: Tab A sets hue.
    const hueBar = tabA.locator('.cp-hue').first();
    await hueBar.scrollIntoViewIfNeeded();
    const hueBox = await hueBar.boundingBox();
    if (hueBox) {
      await tabA.mouse.click(hueBox.x + hueBox.width * 0.25, hueBox.y + hueBox.height / 2);
      await tabA.waitForTimeout(200);
    }
    // Frame 2: Tab A picks saturated color.
    const square = tabA.locator('.cp-square').first();
    await square.scrollIntoViewIfNeeded();
    const sqBox = await square.boundingBox();
    if (sqBox) {
      await tabA.mouse.click(sqBox.x + sqBox.width * 0.85, sqBox.y + sqBox.height * 0.15);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'color-sync-02-tabA-set', testInfo));

    // Frame 3: Tab B receives the sync.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'color-sync-03-tabB-synced', testInfo));

    // Frame 4: Tab A changes to a different hue.
    if (hueBox) {
      await tabA.mouse.click(hueBox.x + hueBox.width * 0.7, hueBox.y + hueBox.height / 2);
      await tabA.waitForTimeout(200);
    }
    if (sqBox) {
      await tabA.mouse.click(sqBox.x + sqBox.width * 0.7, sqBox.y + sqBox.height * 0.3);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'color-sync-04-tabA-changed', testInfo));

    // Frame 5: Tab B updates again.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'color-sync-05-tabB-updated', testInfo));

    // Build GIF.
    await buildGif('cross-tab-color-sync', frames, testInfo, { delay: 1200 });

    // Verify sync.
    const rgbA = await tabA.locator('.cp-rgb-text').first().textContent();
    const rgbB = await tabB.locator('.cp-rgb-text').first().textContent();
    expect(rgbA).toBe(rgbB);

    await ctx.close();
  });

  test('Cross-tab position sync animation', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 700 });
    await tabB.setViewportSize({ width: 800, height: 700 });
    await loadApp(tabA);
    await loadApp(tabB);

    const padA = tabA.locator('.pc-pad').first();
    if (await padA.count() === 0) { test.skip(); return; }

    const frames: string[] = [];

    // Frame 1: Tab B baseline.
    frames.push(await frame(tabB, 'pos-sync-01-before', testInfo));

    // Frame 2: Tab A moves to top-right.
    await padA.scrollIntoViewIfNeeded();
    const padBox = await padA.boundingBox();
    if (padBox) {
      await tabA.mouse.click(padBox.x + padBox.width * 0.8, padBox.y + padBox.height * 0.2);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'pos-sync-02-tabA-moved', testInfo));

    // Frame 3: Tab B synced.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'pos-sync-03-tabB-synced', testInfo));

    // Frame 4: Tab A moves to bottom-left.
    if (padBox) {
      await tabA.mouse.click(padBox.x + padBox.width * 0.2, padBox.y + padBox.height * 0.8);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'pos-sync-04-tabA-moved2', testInfo));

    // Frame 5: Tab B synced again.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'pos-sync-05-tabB-synced2', testInfo));

    await buildGif('cross-tab-position-sync', frames, testInfo, { delay: 1000 });

    const degA = await tabA.locator('.pc-display').first().textContent();
    const degB = await tabB.locator('.pc-display').first().textContent();
    expect(degA).toBe(degB);

    await ctx.close();
  });

  test('Cross-tab fader sync animation', async ({ browser }, testInfo) => {
    const ctx = await browser.newContext();
    const tabA = await ctx.newPage();
    const tabB = await ctx.newPage();
    await tabA.setViewportSize({ width: 800, height: 700 });
    await tabB.setViewportSize({ width: 800, height: 700 });
    await loadApp(tabA);
    await loadApp(tabB);

    const frames: string[] = [];

    // Frame 1: Tab B baseline.
    frames.push(await frame(tabB, 'fader-sync-01-before', testInfo));

    // Frame 2: Tab A sets dimmer high.
    const heroA = tabA.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const faderA = heroA.locator('.dimmer-fader:not(.readonly) .cf-track').first();
    await faderA.scrollIntoViewIfNeeded();
    const fBox = await faderA.boundingBox();
    if (fBox) {
      await tabA.mouse.click(fBox.x + fBox.width / 2, fBox.y + 10);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'fader-sync-02-tabA-high', testInfo));

    // Frame 3: Tab B synced.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'fader-sync-03-tabB-synced', testInfo));

    // Frame 4: Tab A sets dimmer mid.
    if (fBox) {
      await tabA.mouse.click(fBox.x + fBox.width / 2, fBox.y + fBox.height * 0.5);
      await tabA.waitForTimeout(300);
    }
    frames.push(await frame(tabA, 'fader-sync-04-tabA-mid', testInfo));

    // Frame 5: Tab B synced.
    await tabB.waitForTimeout(2000);
    frames.push(await frame(tabB, 'fader-sync-05-tabB-synced2', testInfo));

    await buildGif('cross-tab-fader-sync', frames, testInfo, { delay: 1000 });

    const valA = await heroA.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();
    const heroB = tabB.locator('.fixture-panel', { hasText: 'HERO' }).first();
    const valB = await heroB.locator('.dimmer-fader:not(.readonly) .cf-value').first().textContent();
    expect(valA).toBe(valB);

    await ctx.close();
  });

  test('Feature walkthrough animation', async ({ page }, testInfo) => {
    await loadApp(page);
    const frames: string[] = [];

    // Frame 1: Initial view — DMX only, no VC tab.
    frames.push(await frame(page, 'walkthrough-01-initial', testInfo));

    // Frame 2: Click a model filter badge.
    const badge = page.locator('.dmx-badge').nth(1);
    if (await badge.count() > 0) {
      await badge.click();
      await page.waitForTimeout(300);
      frames.push(await frame(page, 'walkthrough-02-filtered', testInfo));
      await page.locator('.dmx-badge', { hasText: 'All' }).first().click();
      await page.waitForTimeout(300);
    }

    // Frame 3: Enable universe grouping.
    const groupBtn = page.locator('.dmx-group-btn');
    if (await groupBtn.count() > 0) {
      await groupBtn.click();
      await page.waitForTimeout(300);
      frames.push(await frame(page, 'walkthrough-03-grouped', testInfo));

      // Frame 4: Collapse a group.
      const header = page.locator('.dmx-group-label-toggle').first();
      await header.click();
      await page.waitForTimeout(300);
      frames.push(await frame(page, 'walkthrough-04-collapsed', testInfo));

      // Expand back.
      await header.click();
      await page.waitForTimeout(300);
    }

    // Frame 5: Scroll to HERO, show controls.
    const hero = page.locator('.fixture-panel', { hasText: 'HERO' }).first();
    await hero.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    frames.push(await frame(page, 'walkthrough-05-hero-controls', testInfo));

    // Frame 6: Open raw channels.
    const toggle = hero.locator('.rc-toggle');
    if (await toggle.count() > 0) {
      await toggle.click();
      await page.waitForTimeout(300);
      frames.push(await frame(page, 'walkthrough-06-raw-open', testInfo));
    }

    if (frames.length >= 3) {
      await buildGif('feature-walkthrough', frames, testInfo, { delay: 1500 });
    }
  });
});
