/*
 * Smoke test in headless Chromium: the globe renders, a tap opens a province, the
 * clock runs, a save survives a reload. Needs Playwright:
 *   npm install playwright   (CI does this), or point WARROOM_PLAYWRIGHT at an
 *   installed copy's index.mjs.
 *   node warroom/tests/smoke.playwright.mjs
 */
import { mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { serve } from './serve.mjs';
import { decodePng } from './png.js';

const here = dirname(fileURLToPath(import.meta.url));
const out = join(here, 'out');
mkdirSync(out, { recursive: true });
const pw = await import(process.env.WARROOM_PLAYWRIGHT || 'playwright');

const { server, port } = await serve(0);
const launch = { headless: true, args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] };
if (process.env.PLAYWRIGHT_CHROMIUM) launch.executablePath = process.env.PLAYWRIGHT_CHROMIUM;
const browser = await pw.chromium.launch(launch);
const failures = [];
const step = async (name, fn) => {
  try { await fn(); console.log('  ok   ' + name); } catch (e) { failures.push(name + ' — ' + e.message); console.log('  FAIL ' + name + ' — ' + e.message); }
};

try {
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error' && !/favicon/.test(m.text())) errors.push(m.text()); });
  page.on('requestfailed', (r) => { const why = (r.failure() || {}).errorText || ''; if (!/ABORTED/.test(why)) errors.push('request failed: ' + r.url() + ' ' + why); });
  page.on('response', (r) => { if (r.status() >= 400) errors.push(r.status() + ' ' + r.url()); });
  const settled = async () => { await page.waitForFunction(() => window.warroom && !window.warroom.cam.isFlying(), null, { timeout: 30000 }); await page.waitForTimeout(400); };
  await page.goto(`http://127.0.0.1:${port}/warroom/?fresh&seed=smoke`);

  await step('the page boots and shows the globe', async () => {
    await page.waitForSelector('#boot', { state: 'hidden', timeout: 90000 });
    await page.waitForTimeout(1500);
    if (errors.length) throw new Error('console errors: ' + errors.join(' | '));
    const date = await page.textContent('#date');
    if (!date.startsWith('1 Jan 1936')) throw new Error('date is ' + date);
  });

  await step('the globe is drawn (not blank)', async () => {
    const shot = decodePng(await page.screenshot({ type: 'png', clip: { x: 200, y: 100, width: 600, height: 600 } }));
    const seen = new Set(); let bright = 0;
    for (let i = 0; i < shot.data.length; i += shot.channels * 97) {
      seen.add((shot.data[i] >> 4) + ',' + (shot.data[i + 1] >> 4) + ',' + (shot.data[i + 2] >> 4));
      if (shot.data[i] + shot.data[i + 1] + shot.data[i + 2] > 120) bright++;
    }
    if (seen.size < 12 || bright < 50) throw new Error('globe looks blank: ' + JSON.stringify({ colours: seen.size, bright }));
  });

  await step('tapping the globe opens a province panel', async () => {
    const info = await page.evaluate(() => { const w = window.warroom; w.openProvince(w.data.byTag.GER.capital_province); return w.data.provinces[w.data.byTag.GER.capital_province].name; });
    await settled();
    const box = await page.locator('#globe').boundingBox();
    await page.mouse.click(box.x + box.width / 2, box.y + box.height / 2);
    await page.waitForTimeout(500);
    const text = await page.textContent('#panel');
    if (!/Controlled by/.test(text)) throw new Error('panel shows: ' + text.slice(0, 120));
    if (!/Germany/.test(text)) throw new Error('expected a German province near ' + info + ', panel shows: ' + text.slice(0, 160));
  });

  await step('map modes switch without errors', async () => {
    for (const m of ['terrain', 'states', 'weather', 'vp', 'political']) {
      await page.click(`#mapmodes button[data-mode="${m}"]`);
      await page.waitForTimeout(150);
    }
    if (errors.length) throw new Error(errors.join(' | '));
  });

  await step('the clock runs at speed 5 and pauses', async () => {
    await page.click('#topbar button[data-speed="5"]');
    await page.waitForTimeout(2500);
    await page.click('#topbar button[data-speed="0"]');
    const hour = await page.evaluate(() => window.warroom.state.hour);
    if (hour < 24) throw new Error('only ' + hour + ' hours passed');
  });

  await step('a save survives a reload', async () => {
    const hour = await page.evaluate(() => window.warroom.state.hour);
    await page.evaluate(() => window.warroom.send({ type: 'select_country', tag: 'FRA' }));
    await page.click('#saves-button');
    await page.fill('#save-name', 'smoke');
    await page.click('#save-now');
    await page.waitForTimeout(300);
    await page.goto(`http://127.0.0.1:${port}/warroom/?fresh`);
    await page.waitForSelector('#boot', { state: 'hidden', timeout: 90000 });
    await page.click('#saves-button');
    await page.click('#panel button[data-load="smoke"]');
    await page.waitForTimeout(300);
    const after = await page.evaluate(() => ({ hour: window.warroom.state.hour, player: window.warroom.state.player }));
    if (after.hour !== hour || after.player !== 'FRA') throw new Error('loaded ' + JSON.stringify(after) + ' expected hour ' + hour + ' and FRA');
    const name = await page.textContent('#player-name');
    if (name !== 'France') throw new Error('top bar shows ' + name);
  });

  await step('search finds Paris', async () => {
    await page.fill('#search', 'Pari');
    await page.waitForTimeout(200);
    const first = await page.textContent('#search-results li:first-child button');
    if (!/Paris/.test(first)) throw new Error('first hit is ' + first);
    await page.keyboard.press('Enter');
    await settled();
    const text = await page.textContent('#panel h2');
    if (!/Paris/.test(text)) throw new Error('panel is ' + text);
  });

  await page.screenshot({ path: join(out, 'smoke.png') });
  await page.evaluate(() => { window.warroom.cam.flyTo(50, 10, 1.5); });
  await settled();
  await page.screenshot({ path: join(out, 'smoke-europe.png') });
  if (errors.length) failures.push('console errors: ' + errors.join(' | '));
} finally {
  await browser.close();
  server.close();
}
console.log(`\nsmoke: ${7 - failures.length} passed, ${failures.length} failed`);
for (const f of failures) console.log('  - ' + f);
process.exitCode = failures.length ? 1 : 0;
