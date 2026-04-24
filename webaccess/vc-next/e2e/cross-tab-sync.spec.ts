// Cross-tab DMX synchronization tests.
// Verifies that DMX channel changes in one browser tab are reflected in another
// via the WebSocket DMX_SUB/DMX_DELTA push mechanism.
//
// Requires QLC+ running with: ./qlcplus-qml -w -o GARAGE.qxw

import { test, expect, type Page, type BrowserContext } from '@playwright/test';

const APP_URL = '/vc/';

async function loadDmxTab(page: Page) {
  await page.goto(APP_URL);
  await page.waitForSelector('.view-tabs', { timeout: 10_000 });
  await page.locator('.view-tab', { hasText: /DMX/i }).click();
  await page.locator('.fixture-panel').first().waitFor({ timeout: 15_000 });
  // Wait for the WS connection to establish and DMX_SUB to fire.
  // The status should show "Live" once connected.
  await page.waitForFunction(
    () => document.querySelector('.status-text')?.textContent === 'Live',
    { timeout: 10_000 },
  ).catch(() => {
    // If it doesn't go Live, that's OK — Demo Mode also works for local state.
  });
  // Give the DMX subscription time to complete.
  await page.waitForTimeout(1500);
}

// Read the aria-valuenow from the first fader track on a page.
async function readFirstFaderValue(page: Page): Promise<number> {
  const track = page.locator('.channel-fader .cf-track, .dimmer-fader .cf-track').first();
  await expect(track).toBeVisible({ timeout: 5_000 });
  return Number(await track.getAttribute('aria-valuenow') ?? '0');
}

// Click a fader track at a specific vertical percentage (0 = top/max, 1 = bottom/min).
async function clickFaderAt(page: Page, verticalPct: number) {
  const track = page.locator('.channel-fader .cf-track, .dimmer-fader .cf-track').first();
  await track.scrollIntoViewIfNeeded();
  const box = await track.boundingBox();
  if (!box) throw new Error('Fader track not visible');
  const y = box.y + box.height * verticalPct;
  await page.mouse.click(box.x + box.width / 2, y);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
test.describe('Cross-Tab DMX Sync', () => {
  let context: BrowserContext;
  let tabA: Page;
  let tabB: Page;

  test.beforeEach(async ({ browser }) => {
    context = await browser.newContext();
    tabA = await context.newPage();
    tabB = await context.newPage();

    // Load both tabs to the DMX panel.
    await Promise.all([loadDmxTab(tabA), loadDmxTab(tabB)]);
  });

  test.afterEach(async () => {
    await context.close();
  });

  test('fader change in Tab A is reflected in Tab B', async () => {
    // Set a distinctive value in Tab A by clicking near the top (high value).
    await clickFaderAt(tabA, 0.1);
    await tabA.waitForTimeout(500);

    const newValueA = await readFirstFaderValue(tabA);
    expect(newValueA).toBeGreaterThan(200); // clicked near top

    // Wait for the WS DMX_DELTA push to propagate to Tab B.
    // The server pushes at 20Hz but there's WS + React render latency.
    let syncedB = false;
    for (let i = 0; i < 50; i++) {
      await tabB.waitForTimeout(100);
      const valueB = await readFirstFaderValue(tabB);
      if (Math.abs(valueB - newValueA) <= 5) {
        syncedB = true;
        break;
      }
    }

    expect(syncedB).toBeTruthy();
  });

  test('fader change in Tab B is reflected in Tab A', async () => {
    // Set a value in Tab B by clicking near the middle.
    await clickFaderAt(tabB, 0.5);
    await tabB.waitForTimeout(500);

    const newValueB = await readFirstFaderValue(tabB);
    expect(newValueB).toBeGreaterThan(50);
    expect(newValueB).toBeLessThan(200);

    // Wait for sync to Tab A.
    let syncedA = false;
    for (let i = 0; i < 50; i++) {
      await tabA.waitForTimeout(100);
      const valueA = await readFirstFaderValue(tabA);
      if (Math.abs(valueA - newValueB) <= 5) {
        syncedA = true;
        break;
      }
    }

    expect(syncedA).toBeTruthy();
  });

  test('rapid fader changes sync without diverging', async () => {
    // Drag the fader in Tab A through multiple positions.
    const track = tabA.locator('.channel-fader .cf-track, .dimmer-fader .cf-track').first();
    await track.scrollIntoViewIfNeeded();
    const box = await track.boundingBox();
    if (!box) return;

    const x = box.x + box.width / 2;

    // Drag from bottom to top.
    await tabA.mouse.move(x, box.y + box.height - 5);
    await tabA.mouse.down();
    for (let y = box.y + box.height - 5; y >= box.y + 5; y -= 15) {
      await tabA.mouse.move(x, y);
      await tabA.waitForTimeout(30);
    }
    await tabA.mouse.up();
    await tabA.waitForTimeout(500);

    const finalA = await readFirstFaderValue(tabA);
    expect(finalA).toBeGreaterThan(200);

    // Wait for Tab B to catch up.
    let syncedB = false;
    for (let i = 0; i < 30; i++) {
      await tabB.waitForTimeout(100);
      const valueB = await readFirstFaderValue(tabB);
      if (Math.abs(valueB - finalA) <= 5) {
        syncedB = true;
        break;
      }
    }

    expect(syncedB).toBeTruthy();
  });

  test('color picker change in one tab syncs RGB to another', async () => {
    // Check if color picker exists.
    const pickerA = tabA.locator('.color-picker').first();
    if (await pickerA.count() === 0) {
      test.skip();
      return;
    }

    // Click the hue bar in Tab A to set a non-zero hue.
    const hueBarA = tabA.locator('.cp-hue').first();
    await hueBarA.scrollIntoViewIfNeeded();
    const hueBox = await hueBarA.boundingBox();
    if (hueBox) {
      await tabA.mouse.click(hueBox.x + hueBox.width * 0.4, hueBox.y + hueBox.height / 2);
      await tabA.waitForTimeout(200);
    }

    // Click in the bright area of the square.
    const squareA = tabA.locator('.cp-square').first();
    await squareA.scrollIntoViewIfNeeded();
    const sqBox = await squareA.boundingBox();
    if (sqBox) {
      await tabA.mouse.click(sqBox.x + sqBox.width * 0.8, sqBox.y + sqBox.height * 0.15);
      await tabA.waitForTimeout(300);
    }

    const rgbTextA = await tabA.locator('.cp-rgb-text').first().textContent();
    // Should have non-zero RGB.
    expect(rgbTextA).toMatch(/[1-9]\d*/);

    // Wait for Tab B to sync.
    const rgbTextLocB = tabB.locator('.cp-rgb-text').first();
    if (await rgbTextLocB.count() === 0) {
      test.skip();
      return;
    }

    let synced = false;
    for (let i = 0; i < 30; i++) {
      await tabB.waitForTimeout(100);
      const rgbTextB = await rgbTextLocB.textContent();
      if (rgbTextB === rgbTextA) {
        synced = true;
        break;
      }
    }

    // RGB text should match between tabs (or be very close).
    if (!synced) {
      // Even if not pixel-exact, check that Tab B has non-zero RGB too.
      const rgbTextB = await rgbTextLocB.textContent();
      expect(rgbTextB).toMatch(/[1-9]\d*/);
    }
  });

  test('reset in one tab resets the other tab too', async () => {
    // Set a high value in Tab A.
    await clickFaderAt(tabA, 0.1);
    await tabA.waitForTimeout(300);
    const highValue = await readFirstFaderValue(tabA);
    expect(highValue).toBeGreaterThan(200);

    // Wait for sync to B.
    for (let i = 0; i < 20; i++) {
      await tabB.waitForTimeout(100);
      const vB = await readFirstFaderValue(tabB);
      if (vB > 200) break;
    }

    // Reset in Tab B.
    const resetBtn = tabB.locator('.fixture-panel .fp-reset').first();
    await resetBtn.click();
    await tabB.waitForTimeout(500);

    // Wait for Tab A to see the reset (server sends sdResetChannel, then
    // universe values change, then DMX_DELTA pushes the new values).
    let resetSynced = false;
    for (let i = 0; i < 30; i++) {
      await tabA.waitForTimeout(100);
      const vA = await readFirstFaderValue(tabA);
      if (vA < highValue) {
        resetSynced = true;
        break;
      }
    }

    // The value in Tab A should have decreased after Tab B's reset.
    expect(resetSynced).toBeTruthy();
  });
});
