/*
 * Terra Desk for iPhone — the real page in real browsers at phone size.
 *
 * Opens the app as an iPhone 13 in WebKit (Safari's engine) and as a 390 × 844 phone in Chromium, checks that it
 * boots clean and draws the globe, that the menu, the layer list and the cards fit the screen, then switches on every
 * layer in turn with the app's own self-test and prints what each one did. Pictures of each step go to
 * terradesk/tests/shots/.
 *
 *   npm --prefix music/tests install && npx --prefix music/tests playwright install webkit chromium
 *   node terradesk/tests/terradesk-browser.test.cjs                       # this checkout, on a local server
 *   BASE_URL=https://mazoo85.github.io/Maz/terradesk/ node terradesk/tests/terradesk-browser.test.cjs   # the live site
 *
 * On a local server the planes relay refuses the page (it serves mazoo85.github.io only), so Aircraft is allowed to
 * fail there; on the live site every layer must work. Ship Watch needs your own aisstream.io key, so without one it
 * only has to say so. ENGINES=webkit limits the run to one engine.
 */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const SHOTS = path.join(__dirname, 'shots');
let pw = null;
for (const spec of ['playwright', path.join(ROOT, 'music', 'tests', 'node_modules', 'playwright'), path.join(ROOT, 'node_modules', 'playwright')]) {
  try { pw = require(spec); break; } catch (e) { /* next */ }
}
if (!pw) { console.error('Playwright is not installed. Run: npm --prefix music/tests install'); process.exit(2); }

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.webmanifest': 'application/manifest+json', '.wasm': 'application/wasm', '.svg': 'image/svg+xml' };
function serve() {
  const srv = http.createServer((req, res) => {
    const p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
    let f = path.join(ROOT, p.replace(/^\/Maz\//, '/'));
    if (!f.startsWith(ROOT)) { res.writeHead(403); res.end(); return; }
    if (fs.existsSync(f) && fs.statSync(f).isDirectory()) f = path.join(f, 'index.html');
    if (!fs.existsSync(f)) { res.writeHead(404); res.end('not found'); return; }
    res.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(res);
  });
  return new Promise(r => srv.listen(0, '127.0.0.1', () => r({ srv, base: `http://127.0.0.1:${srv.address().port}/Maz/terradesk/` })));
}

const LIVE = !!process.env.BASE_URL;
const LAYERS = ['aurora', 'launches', 'volcanoes', 'battles', 'aircraft', 'ships', 'lightning', 'oldmaps', 'ocean', 'worlds', 'trails', 'roadtrip', 'builder'];
const MAY_FAIL = new Set((process.env.MAY_FAIL || (LIVE ? 'ships' : 'ships,aircraft')).split(',').filter(Boolean));
const failures = [];
const fail = (what) => { failures.push(what); console.log('FAIL', what); };
const withTimeout = (p, ms, what) => Promise.race([p, new Promise((_, no) => setTimeout(() => no(new Error(what + ' took longer than ' + ms / 1000 + ' s')), ms))]);

async function run(engine, base) {
  const browserType = pw[engine];
  const launch = engine === 'chromium' ? { args: ['--no-sandbox', '--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] } : {};
  if (engine === 'chromium' && process.env.CHROMIUM_PATH) launch.executablePath = process.env.CHROMIUM_PATH;
  const browser = await browserType.launch(launch);
  const device = engine === 'webkit' ? pw.devices['iPhone 13'] : { viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true };
  const ctx = await browser.newContext({ ...device, serviceWorkers: 'allow' });
  const page = await ctx.newPage();
  const problems = [];
  page.on('pageerror', e => problems.push('page error: ' + e.message));
  page.on('console', m => { if (m.type() === 'error') problems.push('console: ' + m.text().slice(0, 300)); });
  const shot = async name => { try { await page.screenshot({ path: path.join(SHOTS, `${engine}-${name}.png`) }); } catch (e) { /* not fatal */ } };
  console.log(`\n=== ${engine.toUpperCase()} (${engine === 'webkit' ? 'iPhone 13, Safari engine' : 'phone 390 × 844'}) — ${base}`);

  await page.goto(base, { waitUntil: 'load', timeout: 90000 });
  const booted = await page.waitForFunction(() => window.terra && window.terra.viewer && window.terra.viewer.scene, null, { timeout: 90000 }).then(() => true, () => false);
  if (!booted) { await shot('boot'); fail(`${engine}: the app did not start. ${problems.join(' | ')}`); await browser.close(); return; }
  const gl = await page.evaluate(() => { const s = window.terra.viewer.scene; return { webgl2: !!s.context.webgl2, canvas: [s.canvas.clientWidth, s.canvas.clientHeight] }; });
  console.log('globe:', JSON.stringify(gl));
  await page.waitForFunction(() => window.terra.viewer.scene.globe.tilesLoaded, null, { timeout: 60000 }).catch(() => console.log('note: map tiles still loading after 60 s'));
  await page.waitForTimeout(2000);
  await shot('1-globe');

  // The screen: nothing runs off the side, the menu button is tappable and clear of the notch.
  const layout = await page.evaluate(() => {
    const r = id => { const e = document.getElementById(id); if (!e) return null; const b = e.getBoundingClientRect(); return { x: Math.round(b.left), y: Math.round(b.top), w: Math.round(b.width), h: Math.round(b.height) }; };
    return { vw: innerWidth, vh: innerHeight, scrollW: document.documentElement.scrollWidth, menuBtn: r('menuBtn'), status: r('cursor') };
  });
  console.log('layout:', JSON.stringify(layout));
  if (layout.scrollW > layout.vw + 1) fail(`${engine}: the page is wider than the screen (${layout.scrollW} > ${layout.vw})`);
  if (!layout.menuBtn || layout.menuBtn.w < 36 || layout.menuBtn.h < 36) fail(`${engine}: the menu button is missing or too small to tap`);

  await page.tap('#menuBtn');
  await page.waitForTimeout(600);
  const menu = await page.evaluate(() => { const s = document.getElementById('side').getBoundingClientRect(); return { rows: document.querySelectorAll('#layers .lyr').length, left: Math.round(s.left), right: Math.round(s.right), open: document.body.classList.contains('menu') }; });
  console.log('menu:', JSON.stringify(menu));
  await shot('2-menu');
  if (menu.rows !== 13) fail(`${engine}: the menu shows ${menu.rows} layers, not 13`);
  if (menu.left < 0 || menu.right > layout.vw + 1) fail(`${engine}: the menu runs off the screen`);
  await page.evaluate(() => document.body.classList.remove('menu'));

  // Search: a place name through Nominatim.
  const search = await page.evaluate(async () => { try { const r = await window.terra.T.json('https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&q=Eiffel%20Tower'); return r[0] ? r[0].display_name.slice(0, 40) : 'nothing'; } catch (e) { return 'FAIL ' + e.message; } });
  console.log('search:', search);
  if (/^FAIL/.test(search)) fail(`${engine}: search ${search}`);

  // Every layer, with its own self-test.
  for (const id of LAYERS) {
    let r;
    try { r = await withTimeout(page.evaluate(async id => { const r = await window.terra.test(id); const l = window.terra.layers.find(x => x.id === id); return Object.assign({}, r, { _status: l.statusText || '', _kind: l.statusKind || '' }); }, id), 120000, id); }
    catch (e) { r = { error: e.message.split('\n')[0] }; }
    const text = JSON.stringify(r);
    const bad = !r || !!r.error || r._kind === 'err' || /^error$/i.test(String(r._status)) || r.on === false;
    console.log(`${bad ? (MAY_FAIL.has(id) ? 'warn' : 'FAIL') : 'ok  '} ${id.padEnd(10)} ${text.slice(0, 300)}`);
    if (bad && !MAY_FAIL.has(id)) failures.push(`${engine}: ${id} ${text.slice(0, 200)}`);
    await page.waitForTimeout(1500);
    await shot('layer-' + id);
    // The layer's card fits the phone.
    const panel = await page.evaluate(() => { const p = document.getElementById('panel'); if (!p || p.hidden) return null; const b = p.getBoundingClientRect(); return { left: b.left, right: b.right, bottom: b.bottom }; });
    if (panel && (panel.left < -1 || panel.right > layout.vw + 1)) fail(`${engine}: the ${id} card runs off the screen`);
    try { await withTimeout(page.evaluate(id => window.terra.test(id, 'off'), id), 30000, id + ' off'); } catch (e) { fail(`${engine}: ${id} would not switch off: ${e.message}`); }
    await page.evaluate(() => { const c = document.getElementById('panelClose'); if (c && !document.getElementById('panel').hidden) c.click(); const d = document.getElementById('detailClose'); if (d && !document.getElementById('detail').hidden) d.click(); });
  }

  if (base.startsWith('https:')) {
    const sw = await page.evaluate(async () => { const r = await navigator.serviceWorker.getRegistration(); return r ? (r.active || r.waiting || r.installing ? 'registered' : 'pending') : 'none'; });
    console.log('offline helper:', sw);
    if (sw === 'none') fail(`${engine}: the offline helper did not register`);
  }
  const real = problems.filter(p => !/Failed to load resource|net::ERR|status of 4\d\d|status of 5\d\d|CORS|Access-Control|WebSocket|aisstream/i.test(p));
  for (const p of problems) console.log('  (seen) ' + p);
  if (real.length) fail(`${engine}: errors on the page: ${real.slice(0, 5).join(' | ')}`);
  await browser.close();
}

(async () => {
  fs.mkdirSync(SHOTS, { recursive: true });
  let local = null, base = process.env.BASE_URL;
  if (!base) { local = await serve(); base = local.base; }
  for (const engine of (process.env.ENGINES || 'webkit,chromium').split(',')) {
    try { await run(engine, base); } catch (e) { fail(`${engine}: ${e.message.split('\n')[0]}`); }
  }
  if (local) local.srv.close();
  console.log(failures.length ? `\n${failures.length} problem(s):\n- ` + failures.join('\n- ') : '\nall checks passed');
  process.exit(failures.length ? 1 : 0);
})();
