import { test, expect, type WebSocketRoute } from '@playwright/test';
import { fileURLToPath } from 'node:url';

test('refreshes a replaced patch and recalls pre-master fixture values', async ({ page }) => {
  let revision = 'first';
  let address = 0;
  let remote: WebSocketRoute;
  const writes: unknown[] = [];
  await page.route('http://dmx.test/vc/**', route => {
    const path = new URL(route.request().url()).pathname.slice('/vc/'.length) || 'index.html';
    return route.fulfill({ path: fileURLToPath(new URL('../dist/' + path, import.meta.url)) });
  });
  await page.route('**/vc.json', route => route.fulfill({ json: { pages: [] } }));
  await page.route('**/api/fixtures', route => route.fulfill({
    headers: { 'X-QLC-Patch-Revision': revision },
    json: [{ id: 7, name: 'Wash', universe: 0, address, channels: 1 }],
  }));
  await page.route('**/api/channels', route => route.fulfill({
    headers: { 'X-QLC-Patch-Revision': revision },
    json: [{ fixtureID: 7, index: 0, group: 'Intensity', colour: 'NoColour', name: 'Dimmer',
      controlByte: 'coarse', defaultValue: 0, capabilities: [] }],
  }));
  await page.route('**/api/preset-values?*', route => {
    const query = new URL(route.request().url()).searchParams;
    expect(query.get('revision')).toBe(revision);
    expect(query.get('fixtureID')).toBe('7');
    return route.fulfill({ json: [255] });
  });
  await page.routeWebSocket('**/qlcplusWS', ws => {
    remote = ws;
    ws.onMessage(raw => {
      if (!String(raw).startsWith('{')) return;
      const message = JSON.parse(String(raw));
      if (message.cmd === 'DMX_SUB')
        ws.send(JSON.stringify({ cmd: 'DMX_STATE', fixtureID: 7, universe: 0, address, values: [128] }));
      if (message.cmd === 'DMX_SET' || message.cmd === 'DMX_RESET') writes.push(message);
    });
  });
  await page.goto('http://dmx.test/vc/');
  await expect(page.locator('.fp-addr')).toHaveText('U1·1');
  await expect(page.getByRole('slider', { name: 'Dimmer', exact: true }).first()).toHaveAttribute('aria-valuenow', '128');
  page.once('dialog', dialog => dialog.accept('Full'));
  await page.locator('.fp-preset-save').click();
  await page.getByRole('button', { name: 'Full', exact: true }).click();
  await expect.poll(() => writes).toContainEqual({ cmd: 'DMX_SET', fixtureID: 7, channel: 0, value: 255, revision });

  address = 20;
  revision = 'second';
  remote!.send('DMX_PATCH_CHANGED|' + revision);
  await expect(page.locator('.fp-addr')).toHaveText('U1·21');
  await page.getByRole('button', { name: 'Reset fixture' }).click();
  await expect.poll(() => writes).toContainEqual({ cmd: 'DMX_RESET', fixtureID: 7, channel: 0, revision });

  remote!.close();
  address = 32;
  revision = 'third';
  await expect(page.locator('.fp-addr')).toHaveText('U1·33');
});
