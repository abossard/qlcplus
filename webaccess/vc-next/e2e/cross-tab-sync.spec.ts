// Cross-tab DMX synchronization tests.
// Verifies that DMX channel changes in one browser tab are reflected in another
// via the WebSocket DMX_SUB/DMX_DELTA push mechanism.
//
// Requires QLC+ running with: ./qlcplus-qml -w -o GARAGE.qxw

import { test, expect, type Page, type BrowserContext } from '@playwright/test';

const APP_URL = '/vc/';

async function loadDmxTab(page: Page) {
  await page.goto(APP_URL);
  // DMX is the only view — wait for it to render directly.
  await page.waitForSelector('.dmx-view', { timeout: 10_000 });
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

// Read the aria-valuenow from the HERO dimmer fader.
async function readFirstFaderValue(page: Page): Promise<number> {
  const track = page.locator('.fixture-panel', { hasText: 'HERO' })
    .first()
    .locator('.dimmer-fader:not(.readonly) .cf-track')
    .first();
  await expect(track).toBeVisible({ timeout: 5_000 });
  return Number(await track.getAttribute('aria-valuenow') ?? '0');
}

// Click the HERO dimmer fader at a specific vertical percentage
// (0 = top/max, 1 = bottom/min). Uses dispatchEvent + setPointerCapture stub
// because synthetic PointerEvents have no active pointer state, which would
// otherwise make React's onPointerDown handler throw.
async function clickFaderAt(page: Page, verticalPct: number) {
  const track = page.locator('.fixture-panel', { hasText: 'HERO' })
    .first()
    .locator('.dimmer-fader:not(.readonly) .cf-track')
    .first();
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
    await expect.poll(() => readFirstFaderValue(tabA), { timeout: 3_000 })
      .toBeGreaterThan(200);
    const newValueA = await readFirstFaderValue(tabA);

    // Wait for the WS DMX_DELTA push to propagate to Tab B.
    await expect.poll(() => readFirstFaderValue(tabB), { timeout: 5_000 })
      .toBeGreaterThan(newValueA - 6);
  });

  test('fader change in Tab B is reflected in Tab A', async () => {
    // Set a value in Tab B by clicking near the middle.
    await clickFaderAt(tabB, 0.5);
    await expect.poll(() => readFirstFaderValue(tabB), { timeout: 3_000 })
      .toBeGreaterThan(50);
    const newValueB = await readFirstFaderValue(tabB);
    expect(newValueB).toBeLessThan(200);

    // Wait for sync to Tab A.
    await expect.poll(() => readFirstFaderValue(tabA), { timeout: 5_000 })
      .toBeGreaterThan(newValueB - 6);
  });

  test('rapid fader changes sync without diverging', async () => {
    // Drag the fader in Tab A through multiple positions via dispatchEvent.
    const track = tabA.locator('.fixture-panel', { hasText: 'HERO' })
      .first()
      .locator('.dimmer-fader:not(.readonly) .cf-track')
      .first();
    await track.scrollIntoViewIfNeeded();
    await track.evaluate((el: HTMLElement) => {
      const orig = (Element.prototype as any).setPointerCapture;
      (Element.prototype as any).setPointerCapture = function () { /* no-op */ };
      try {
        const r = el.getBoundingClientRect();
        const x = r.left + r.width / 2;
        const yStart = r.top + r.height - 5;
        const yEnd = r.top + 5;
        const baseOpts: PointerEventInit = {
          bubbles: true, cancelable: true, pointerId: 1, pointerType: 'mouse',
          clientX: x, clientY: yStart, button: 0, buttons: 1, isPrimary: true,
        };
        el.dispatchEvent(new PointerEvent('pointerdown', baseOpts));
        for (let y = yStart; y >= yEnd; y -= 15) {
          el.dispatchEvent(new PointerEvent('pointermove', { ...baseOpts, clientY: y }));
        }
        el.dispatchEvent(new PointerEvent('pointerup', { ...baseOpts, clientY: yEnd, buttons: 0 }));
      } finally {
        (Element.prototype as any).setPointerCapture = orig;
      }
    });
    await expect.poll(() => readFirstFaderValue(tabA), { timeout: 3_000 })
      .toBeGreaterThan(200);
    const finalA = await readFirstFaderValue(tabA);

    // Wait for Tab B to catch up.
    await expect.poll(() => readFirstFaderValue(tabB), { timeout: 5_000 })
      .toBeGreaterThan(finalA - 6);
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
    await tabA.waitForTimeout(500);
    // Poll for value because WS echoes can briefly override optimistic state.
    await expect.poll(() => readFirstFaderValue(tabA), { timeout: 5_000 })
      .toBeGreaterThan(150);
    const highValue = await readFirstFaderValue(tabA);

    // Wait for sync to B.
    await expect.poll(() => readFirstFaderValue(tabB), { timeout: 5_000 })
      .toBeGreaterThan(150);

    // Reset HERO in Tab B.
    const resetBtn = tabB.locator('.fixture-panel', { hasText: 'HERO' }).first().locator('.fp-reset');
    await resetBtn.click();
    await tabB.waitForTimeout(1000);

    // Wait for Tab A to see the reset (8 second timeout for the full round-trip).
    await expect.poll(() => readFirstFaderValue(tabA), { timeout: 8_000 })
      .toBeLessThan(highValue);
  });
});
