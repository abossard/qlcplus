import { test, expect } from '@playwright/test';

// Helper: start a review session via API and set it in localStorage
async function startReviewSession(page: import('@playwright/test').Page) {
  // Clean up any previous test runs
  await page.request.delete('/api/reset');

  await page.goto('/');
  await page.evaluate(() => localStorage.clear());

  // Start a new session via button click
  await page.reload();
  await page.locator('#btn-start').waitFor({ timeout: 5000 });
  await page.locator('#btn-start').click();

  // Wait until check items render
  await page.locator('.check-item').first().waitFor({ timeout: 5000 });
}

test.describe('Manual Review Runner', () => {

  test.beforeEach(async ({ page }) => {
    await startReviewSession(page);
  });

  test.afterEach(async ({ page }) => {
    // Clean up test-reports
    await page.request.delete('/api/reset');
  });

  // ── Page loads with visible structure ──

  test('shows sections, test cases, and progress bar on load', async ({ page }) => {
    await expect(page.getByRole('heading', { level: 2 }).first()).toBeVisible();
    await expect(page.getByRole('heading', { level: 3 }).first()).toBeVisible();
    await expect(page.locator('#progress-text')).toContainText('0%');
    await expect(page.locator('#progress-text')).toContainText('items');
  });

  test('sidebar lists navigable sections and cases', async ({ page }) => {
    await expect(page.locator('.sidebar-section-header').first()).toBeVisible();
    await expect(page.locator('.sidebar-case').first()).toBeVisible();
  });

  // ── Pass / Fail / Skip via button clicks ──

  test('user clicks Pass button on a check item', async ({ page }) => {
    const progress = page.locator('#progress-text');
    const before = await progress.textContent();
    const total = Number(before?.match(/\((\d+) items\)/)?.[1] ?? 0);
    expect(total).toBeGreaterThan(100);

    await page.locator('[title="Pass (p)"]').first().click();
    await expect(progress).toContainText('1✅');
    await expect(progress).toContainText('— 1% complete');
    console.log(`Manual progress floor: total=${total}, display="${await progress.textContent()}"`);
  });

  test('user clicks Fail button on a check item', async ({ page }) => {
    await page.locator('[title="Fail (f)"]').first().click();
    await expect(page.locator('#progress-text')).toContainText('1❌');
  });

  test('user clicks Skip button on a check item', async ({ page }) => {
    await page.locator('[title="Skip (s)"]').first().click();
    await expect(page.locator('#progress-text')).toContainText('1⏭');
  });

  test('marking multiple items updates progress count', async ({ page }) => {
    await page.locator('[title="Pass (p)"]').nth(0).click();
    await page.locator('[title="Pass (p)"]').nth(1).click();
    await page.locator('[title="Pass (p)"]').nth(2).click();
    await expect(page.locator('#progress-text')).toContainText('3✅');
  });

  // ── Persistence ──

  test('pass/fail status survives page reload', async ({ page }) => {
    await page.locator('[title="Fail (f)"]').first().click();
    await expect(page.locator('#progress-text')).toContainText('1❌');

    // Wait for save to complete
    await page.waitForTimeout(300);

    await page.reload();
    await page.locator('.check-item').first().waitFor({ timeout: 5000 });

    await expect(page.locator('#progress-text')).toContainText('1❌');
  });

  // ── Notes ──

  test('user types a note and it survives reload', async ({ page }) => {
    const note = page.getByPlaceholder('Add note…').first();
    await note.fill('Looks broken on retina');
    await note.press('Tab');

    // Wait for save to complete
    await page.waitForTimeout(300);

    await page.reload();
    await page.locator('.check-item').first().waitFor({ timeout: 5000 });

    await expect(page.getByPlaceholder('Add note…').first()).toHaveValue('Looks broken on retina');
  });

  // ── Tester name ──

  test('user enters their name and it survives reload', async ({ page }) => {
    await page.locator('#tester-name').fill('Bob Tester');
    await page.locator('#tester-name').press('Tab');

    // Wait for save to complete
    await page.waitForTimeout(300);

    await page.reload();
    await page.locator('.check-item').first().waitFor({ timeout: 5000 });

    await expect(page.locator('#tester-name')).toHaveValue('Bob Tester');
  });

  // ── Keyboard: navigation + pass/fail/skip ──

  test('j/k navigate and p/f/s mark items via keyboard', async ({ page }) => {
    await expect(page.locator('.check-item.focused')).toBeVisible();

    // 'p' marks focused as pass, then auto-advances
    await page.keyboard.press('p');
    await expect(page.locator('#progress-text')).toContainText('1✅');

    // 'f' marks the next as fail
    await page.keyboard.press('f');
    await expect(page.locator('#progress-text')).toContainText('1❌');

    // 'k' goes back, 's' changes the failed one to skip
    await page.keyboard.press('k');
    await page.keyboard.press('s');
    await expect(page.locator('#progress-text')).toContainText('1✅');
    await expect(page.locator('#progress-text')).toContainText('1⏭');
    await expect(page.locator('#progress-text')).not.toContainText('1❌');
  });

  test('keyboard shortcuts do NOT fire when typing in a text field', async ({ page }) => {
    const note = page.getByPlaceholder('Add note…').first();
    await note.click();
    await page.keyboard.type('pfj');

    await expect(note).toHaveValue('pfj');
    await expect(page.locator('#progress-text')).toContainText('0✅');
  });

  test('arrow keys navigate between items', async ({ page }) => {
    await page.keyboard.press('ArrowDown');
    await page.keyboard.press('p');
    await page.keyboard.press('ArrowUp');
    await page.keyboard.press('ArrowUp');
    await page.keyboard.press('f');

    await expect(page.locator('#progress-text')).toContainText('1✅');
    await expect(page.locator('#progress-text')).toContainText('1❌');
  });

  // ── Toolbar buttons ──

  test('Help button opens and closes help overlay', async ({ page }) => {
    await expect(page.locator('#help-overlay')).toBeHidden();

    await page.locator('#btn-help').click();
    await expect(page.locator('#help-overlay')).toBeVisible();
    await expect(page.locator('.help-content h2')).toContainText('Keyboard Shortcuts');

    await page.locator('#help-overlay').click();
    await expect(page.locator('#help-overlay')).toBeHidden();
  });

  test('? key also toggles help overlay', async ({ page }) => {
    await page.keyboard.press('?');
    await expect(page.locator('#help-overlay')).toBeVisible();
    await page.keyboard.press('?');
    await expect(page.locator('#help-overlay')).toBeHidden();
  });

  test('Reset button clears all progress', async ({ page }) => {
    await page.keyboard.press('p');
    await page.keyboard.press('f');
    await expect(page.locator('#progress-text')).toContainText('1✅');

    page.on('dialog', dialog => dialog.accept());
    await page.locator('#btn-reset').click();

    // After reset, start screen should show
    await expect(page.locator('#start-screen')).toBeVisible();
  });

  // ── Sidebar navigation ──

  test('clicking a sidebar case scrolls to that test and sets focus', async ({ page }) => {
    const cases = page.locator('.sidebar-case');
    expect(await cases.count()).toBeGreaterThan(5);

    await cases.nth(5).click();
    await expect(page.locator('.check-item.focused')).toBeVisible();
  });

  test('sidebar icons update when items are marked', async ({ page }) => {
    const cases = page.locator('.sidebar-case');
    let pendingIndex = -1;
    for (let i = 0; i < await cases.count(); i++) {
      if ((await cases.nth(i).textContent())?.includes('⬜')) {
        pendingIndex = i;
        break;
      }
    }
    expect(pendingIndex).toBeGreaterThanOrEqual(0);

    await cases.nth(pendingIndex).click();
    await page.keyboard.press('p');

    await expect(cases.nth(pendingIndex)).not.toContainText('⬜');
  });

  // ── Screenshot: file upload ──

  test('uploading a screenshot file shows attachment tag', async ({ page }) => {
    const fileInput = page.locator('.screenshot-input').first();
    await fileInput.setInputFiles({
      name: 'evidence.png',
      mimeType: 'image/png',
      buffer: createMinimalPng(),
    });

    // Wait for upload to complete
    await expect(page.locator('.screenshot-tag').first()).toBeVisible({ timeout: 5000 });
    await expect(page.locator('.screenshot-tag').first()).toContainText('.png');
  });

  // ── Screenshot: clipboard paste ──

  test('pasting an image from clipboard attaches it to the focused item', async ({ page }) => {
    // Click the Paste hint to focus the check item
    await page.locator('.paste-hint').first().click();

    // Simulate clipboard paste with an image
    await page.evaluate(async () => {
      const canvas = document.createElement('canvas');
      canvas.width = 2;
      canvas.height = 2;
      const ctx = canvas.getContext('2d')!;
      ctx.fillStyle = '#ff0000';
      ctx.fillRect(0, 0, 2, 2);

      const blob: Blob = await new Promise(resolve =>
        canvas.toBlob(b => resolve(b!), 'image/png')
      );

      const dt = new DataTransfer();
      dt.items.add(new File([blob], 'clipboard.png', { type: 'image/png' }));

      // Dispatch on the focused element
      const focused = document.querySelector('.check-item.focused') || document;
      focused.dispatchEvent(new ClipboardEvent('paste', {
        clipboardData: dt,
        bubbles: true,
        cancelable: true,
      }));
    });

    // User should see a screenshot tag on the focused check item
    const focusedItem = page.locator('.check-item.focused');
    await expect(focusedItem.locator('.screenshot-tag').first()).toBeVisible({ timeout: 5000 });
    await expect(focusedItem.locator('.screenshot-tag').first()).toContainText('.png');
  });

  // ── Start / Conclude flow ──

  test('start screen shows when no active run, clicking start begins a run', async ({ page }) => {
    // Reset to get back to start screen
    await page.evaluate(() => localStorage.clear());
    await page.reload();

    await expect(page.locator('#start-screen')).toBeVisible();
    await expect(page.locator('#review-ui')).toBeHidden();

    await page.locator('#btn-start').click();
    await page.locator('.check-item').first().waitFor({ timeout: 5000 });

    await expect(page.locator('#start-screen')).toBeHidden();
    await expect(page.locator('#review-ui')).toBeVisible();
  });

  // ── Full end-to-end workflow ──

  test('complete workflow: name, mark items, add note, upload, reload', async ({ page }) => {
    // 1. Enter tester name
    await page.locator('#tester-name').fill('E2E Bot');
    await page.locator('#tester-name').press('Tab');

    // 2. Pass first item via keyboard
    await page.keyboard.press('p');

    // 3. Fail second item via button click
    await page.locator('[title="Fail (f)"]').nth(1).click();

    // 4. Add a note on the second item
    await page.getByPlaceholder('Add note…').nth(1).fill('Bug: flickers on resize');
    await page.getByPlaceholder('Add note…').nth(1).press('Tab');

    // 5. Upload a screenshot on the first item
    await page.locator('.screenshot-input').first().setInputFiles({
      name: 'proof.png',
      mimeType: 'image/png',
      buffer: createMinimalPng(),
    });

    // 6. Verify progress
    await expect(page.locator('#progress-text')).toContainText('1✅');
    await expect(page.locator('#progress-text')).toContainText('1❌');

    // Wait for all async saves
    await page.waitForTimeout(500);

    // 7. Reload and verify everything persisted
    await page.reload();
    await page.locator('.check-item').first().waitFor({ timeout: 5000 });

    await expect(page.locator('#tester-name')).toHaveValue('E2E Bot');
    await expect(page.locator('#progress-text')).toContainText('1✅');
    await expect(page.locator('#progress-text')).toContainText('1❌');
    await expect(page.getByPlaceholder('Add note…').nth(1)).toHaveValue('Bug: flickers on resize');
    await expect(page.locator('.screenshot-tag').first()).toContainText('.png');
  });
});

function createMinimalPng(): Buffer {
  const hex = '89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000a49444154789c626000000002000198e195280000000049454e44ae426082';
  return Buffer.from(hex, 'hex');
}
