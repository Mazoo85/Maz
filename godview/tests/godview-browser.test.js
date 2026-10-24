/*
 * GOD'S EYE VIEW — the page, driven in a real browser at phone size.
 *
 * The logic suite proves the feeds and the layers; this proves the page: it
 * boots clean, draws a globe, shows live quakes in the layer bar, opens the
 * details sheet, toggles a layer off, survives the feed going down, and shows
 * a readable message instead of crashing when the globe cannot load.
 *
 * The USGS and CelesTrak feeds are always answered from saved fixtures, so
 * this never depends on what the Earth (or the sky) did today. The globe library comes from the CDN;
 * set GLOBE_JS to a local copy of globe.gl.min.js (2.46.2) to run offline.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node godview/tests/godview-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const zlib = require('zlib');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const FIXTURE = fs.readFileSync(path.join(__dirname, 'fixtures', 'usgs-sample.json'));
/* The satellite fixtures are re-dated to a day ago on every run, so their
 * orbits are fresh whatever today's date is. (Pinning the page's clock
 * instead stops its timers in Playwright, and satellites move on a timer.) */
function checksum(line) {
  let sum = 0;
  for (const c of line.slice(0, 68)) sum += c >= '0' && c <= '9' ? Number(c) : c === '-' ? 1 : 0;
  return sum % 10;
}
function redate(text, date) {
  const yy = String(date.getUTCFullYear() % 100).padStart(2, '0');
  const dayOfYear = (date - Date.UTC(date.getUTCFullYear(), 0, 1)) / 86400000 + 1;
  const epoch = yy + Math.floor(dayOfYear).toString().padStart(3, '0') + '.' +
    (dayOfYear % 1).toFixed(8).slice(2);
  return text.split('\n').map((line) => {
    if (!line.startsWith('1 ') || line.length < 69) return line;
    const out = line.slice(0, 18) + epoch + line.slice(32, 68);
    /* A line whose checksum was deliberately broken stays broken. */
    const wasValid = checksum(line) === Number(line[68]);
    const sum = wasValid ? checksum(out) : (checksum(out) + 1) % 10;
    return out + sum + line.slice(69);
  }).join('\n');
}
const YESTERDAY = new Date(Date.now() - 86400000);

/* Weather pictures, made here so no binary fixtures are needed: a minimal PNG
 * encoder (8-bit RGBA, no filtering) over a fill function. */
function crc32(buf) {
  let c = ~0;
  for (const b of buf) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}
function png(width, height, fill) {
  const raw = Buffer.alloc((width * 4 + 1) * height);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) Buffer.from(fill(x, y)).copy(raw, y * (width * 4 + 1) + 1 + x * 4);
  }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const body = Buffer.concat([Buffer.from(type), data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(body));
    return Buffer.concat([len, body, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))
  ]);
}
/* Clouds: one white bank over 0–45°N, 0–90°E; sea everywhere else. */
const CLOUDS_PNG = png(64, 32, (x, y) => (x >= 32 && x < 48 && y >= 8 && y < 16 ? [245, 245, 245, 255] : [10, 40, 90, 255]));
/* Rain: one radar tile of the 4 × 4 has rain in it; the rest are clear. */
const RAIN_PNG = png(16, 16, () => [0, 120, 255, 220]);
const CLEAR_PNG = png(16, 16, () => [0, 0, 0, 0]);
const ADSB = fs.readFileSync(path.join(__dirname, 'fixtures', 'adsb-sample.json'));
const OPENSKY = fs.readFileSync(path.join(__dirname, 'fixtures', 'opensky-sample.json'));
const TFL = fs.readFileSync(path.join(__dirname, 'fixtures', 'tfl-jamcams-sample.json'));
const DIGITRAFFIC = fs.readFileSync(path.join(__dirname, 'fixtures', 'digitraffic-stations-sample.json'));
const CALTRANS = fs.readFileSync(path.join(__dirname, 'fixtures', 'caltrans-d4-sample.json'));
/* A camera picture: a grey road under a pale sky. */
const CAMERA_PNG = png(32, 24, (x, y) => (y < 10 ? [180, 200, 220, 255] : [90, 90, 90, 255]));
const RAIN_INDEX = JSON.stringify({
  version: '2.0',
  host: 'https://tilecache.rainviewer.com',
  radar: { past: [{ time: 1792563000, path: '/v2/radar/1792563000' }], nowcast: [] }
});
const STATIONS_TLE = redate(fs.readFileSync(path.join(__dirname, 'fixtures', 'celestrak-stations.txt'), 'utf8'), YESTERDAY);
const VISUAL_TLE = redate(fs.readFileSync(path.join(__dirname, 'fixtures', 'celestrak-visual.txt'), 'utf8'), YESTERDAY);
const GLOBE_URL = 'https://cdn.jsdelivr.net/npm/globe.gl@2.46.2/dist/globe.gl.min.js';

let chromium = null;
for (const spec of [
  'playwright',
  'playwright-core',
  path.join(ROOT, 'music', 'tests', 'node_modules', 'playwright'),
  path.join(ROOT, 'node_modules', 'playwright')
]) {
  try {
    chromium = require(spec).chromium;
    break;
  } catch (e) {
    /* try the next one */
  }
}
if (!chromium) {
  console.error('Playwright is not installed. Run: npm --prefix music/tests install');
  process.exit(2);
}

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8217;
const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.md': 'text/plain',
  '.json': 'application/json', '.geojson': 'application/json', '.svg': 'image/svg+xml',
  '.webmanifest': 'application/manifest+json'
};

const server = http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split('?')[0].split('#')[0]);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.join(ROOT, p);
  if (!file.startsWith(ROOT) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404);
    res.end('not found');
    return;
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  res.end(fs.readFileSync(file));
});

let failures = 0;
function check(cond, msg) {
  console.log((cond ? '  ok   ' : '  FAIL ') + msg);
  if (!cond) failures++;
}

function launchOptions() {
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio'] };
  for (const c of [
    process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    '/opt/pw-browsers/chromium/chrome-linux/chrome'
  ]) {
    if (c && fs.existsSync(c)) {
      opts.executablePath = c;
      break;
    }
  }
  return opts;
}

/* A phone-sized page with the network answered by us. `quakes`, `sats`,
 * `weather`, `adsb`, `opensky`, `cameras` and `globe` are 'ok' or 'down'.
 * Finland's camera pictures are always cut, to show the "no picture" note.
 * `relay` set to 'ok' or 'down' gives the page a planes relay to use. Errors caused on purpose (a feed
 * we cut) are not problems. */
async function openPage(browser, base, { quakes = 'ok', sats = 'ok', weather = 'ok', adsb = 'ok', opensky = 'ok', cameras = 'ok', globe = 'ok', relay = null } = {}) {
  const page = await browser.newPage({
    viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true
  });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => {
    if (m.type() !== 'error') return;
    if (/Failed to load resource|net::ERR_/.test(m.text())) return; // the cuts we made on purpose
    problems.push('console: ' + m.text());
  });
  await page.route('https://earthquake.usgs.gov/**', (route) =>
    quakes === 'ok'
      ? route.fulfill({ status: 200, contentType: 'application/json', body: FIXTURE,
        headers: { 'access-control-allow-origin': '*' } })
      : route.abort('failed'));
  await page.route('https://celestrak.org/**', (route) => {
    if (sats !== 'ok') return route.abort('failed');
    const body = /GROUP=stations/.test(route.request().url()) ? STATIONS_TLE : VISUAL_TLE;
    return route.fulfill({ status: 200, contentType: 'text/plain', body: body,
      headers: { 'access-control-allow-origin': '*' } });
  });
  const cors = { 'access-control-allow-origin': '*' };
  await page.route('https://wvs.earthdata.nasa.gov/**', (route) => (weather === 'ok' ?
    route.fulfill({ status: 200, contentType: 'image/png', body: CLOUDS_PNG, headers: cors }) : route.abort('failed')));
  await page.route('https://api.rainviewer.com/**', (route) => (weather === 'ok' ?
    route.fulfill({ status: 200, contentType: 'application/json', body: RAIN_INDEX, headers: cors }) : route.abort('failed')));
  await page.route('https://tilecache.rainviewer.com/**', (route) => {
    if (weather !== 'ok') return route.abort('failed');
    const rainy = /\/256\/2\/1\/1\//.test(route.request().url());
    return route.fulfill({ status: 200, contentType: 'image/png', body: rainy ? RAIN_PNG : CLEAR_PNG, headers: cors });
  });
  /* The plane samples are dated; serve them with the times moved to now, so
   * nothing is dropped as stale. */
  const fresh = (buf, field, scale) => {
    const j = JSON.parse(buf);
    j[field] = Math.floor(Date.now() / scale);
    if (j.states) for (const st of j.states) if (st[3] !== null) st[3] += j.time - 1792563000;
    return JSON.stringify(j);
  };
  await page.route('https://api.adsb.lol/**', (route) => (adsb === 'ok' ?
    route.fulfill({ status: 200, contentType: 'application/json', body: fresh(ADSB, 'now', 1), headers: cors }) : route.abort('failed')));
  await page.route('https://opensky-network.org/**', (route) => (opensky === 'ok' ?
    route.fulfill({ status: 200, contentType: 'application/json', body: fresh(OPENSKY, 'time', 1000), headers: cors }) : route.abort('failed')));
  const list = (body) => (route) => (cameras === 'ok' ?
    route.fulfill({ status: 200, contentType: 'application/json', body: body, headers: cors }) : route.abort('failed'));
  const picture = (route) => route.fulfill({ status: 200, contentType: 'image/png', body: CAMERA_PNG });
  await page.route('https://api.tfl.gov.uk/**', list(TFL));
  await page.route('https://tie.digitraffic.fi/**', list(DIGITRAFFIC));
  await page.route('https://cwwp2.dot.ca.gov/**', (route) =>
    (/\.json$/.test(new URL(route.request().url()).pathname) ? list(CALTRANS)(route) : picture(route)));
  await page.route('https://s3-eu-west-1.amazonaws.com/jamcams.tfl.gov.uk/**', picture);
  await page.route('https://weathercam.digitraffic.fi/**', (route) => route.abort('failed'));
  /* The page's settings come from the test, never from the real config.js, so
   * no check ever reaches the real planes relay. */
  await page.route('**/godview/js/config.js', (route) => route.fulfill({
    status: 200, contentType: 'text/javascript',
    body: 'window.GV_CONFIG = { planesRelay: ' + JSON.stringify(relay ? 'https://gv-relay.example.workers.dev' : '') + ' };'
  }));
  if (relay) {
    await page.route('https://gv-relay.example.workers.dev/**', (route) => {
      if (relay !== 'ok') return route.fulfill({ status: 502, contentType: 'application/json', headers: cors, body: '{"error":"no flight source answered"}' });
      const j = JSON.parse(fresh(ADSB, 'now', 1));
      return route.fulfill({ status: 200, contentType: 'application/json', headers: cors,
        body: JSON.stringify({ source: 'adsb.fi', ac: j.ac }) });
    });
  }
  await page.route(GLOBE_URL, (route) => {
    if (globe !== 'ok') return route.abort('failed');
    if (process.env.GLOBE_JS) {
      return route.fulfill({ status: 200, contentType: 'text/javascript', body: fs.readFileSync(process.env.GLOBE_JS) });
    }
    return route.continue();
  });
  await page.goto(base + '/godview/', { waitUntil: 'load' });
  /* Wait until every layer has answered (or the fallback is up) — a fixed wait
   * flakes on a slow software-rendered GPU. */
  await page.waitForFunction(() => {
    if (!document.getElementById('fallback').hidden) return true;
    const vals = [...document.querySelectorAll('.layer-btn .val')];
    return vals.length > 0 && vals.every((v) => v.textContent !== '' && v.textContent !== '…');
  }, null, { timeout: 15000 }).catch(() => {});
  await page.waitForTimeout(300);
  return { page, problems };
}

/* Turn the Earth to face a moving marker, then tap it like a finger would.
 * It never stops moving, and on a slow software GPU a frame can land between
 * finding it and tapping, so wait until it is shown and settled, and give it
 * three goes. Says what was under the finger if the sheet never opens. */
async function tapMarker(page, sel, layerId, index) {
  await page.evaluate(({ layerId, index }) => {
    const g = window.GODVIEW.globe;
    g.controls().autoRotate = false;
    const m = window.GODVIEW.layers.find((l) => l.id === layerId).html[index];
    g.pointOfView({ lat: m.lat, lng: m.lng, altitude: 2 }, 0);
  }, { layerId, index });
  await page.waitForTimeout(1200);
  const sheetOpen = () => page.evaluate(() => !document.getElementById('sheet').hidden);
  let underFinger = '';
  for (let attempt = 0; attempt < 3 && !(await sheetOpen()); attempt++) {
    await page.evaluate(() => { window.__markerBox = null; });
    await page.waitForFunction((s) => {
      const el = document.querySelector(s);
      if (!el || el.classList.contains('is-hidden')) return false;
      const r = el.getBoundingClientRect();
      const prev = window.__markerBox;
      window.__markerBox = [r.left, r.top];
      return prev && Math.abs(prev[0] - r.left) < 2 && Math.abs(prev[1] - r.top) < 2;
    }, sel, { timeout: 10000, polling: 250 }).catch(() => {});
    const box = await page.$eval(sel, (e) => {
      const r = e.getBoundingClientRect();
      return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
    });
    underFinger = await page.evaluate(({ x, y }) => {
      const el = document.elementFromPoint(x, y);
      return el ? el.tagName.toLowerCase() + (el.id ? '#' + el.id : '') + '.' + el.className : 'nothing';
    }, box);
    await page.touchscreen.tap(box.x, box.y);
    await page.waitForTimeout(300);
  }
  return { opened: await sheetOpen(), underFinger };
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}`;
  const browser = await chromium.launch(launchOptions());

  try {
    console.log('\nBOOT (phone, 390 × 844)');
    {
      const { page, problems } = await openPage(browser, base);
      check(problems.length === 0, 'boots with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));
      check((await page.title()) === "GOD'S EYE VIEW", "title is GOD'S EYE VIEW");
      check(await page.evaluate(() => document.getElementById('fallback').hidden), 'no fallback message');
      check(await page.evaluate(() => !!document.querySelector('#globe canvas')), 'the globe drew a canvas');
      check(await page.evaluate(() => window.GODVIEW.globe.polygonsData().length > 150), 'country outlines are on the globe');
      check(/^\d\d:\d\d:\d\d UTC$/.test(await page.textContent('#clock')), 'the UTC clock is ticking');

      console.log('\nEARTHQUAKES');
      const btn = 'button.layer-btn[data-layer="quakes"]';
      check((await page.getAttribute(btn, 'aria-pressed')) === 'true', 'quakes are on by default');
      check((await page.textContent(btn + ' .val')).trim() === '3', 'the button counts 3 quakes');
      check(await page.evaluate(() => window.GODVIEW.globe.pointsData().length === 3), '3 quake dots on the globe');
      check(await page.evaluate(() => window.GODVIEW.globe.ringsData().length === 2), '2 pulsing rings (M2.5+)');
      check(await page.evaluate(() => document.getElementById('live').classList.contains('is-on')), 'the LIVE light is on');

      console.log('\nSATELLITES');
      const sats = 'button.layer-btn[data-layer="sats"]';
      check((await page.getAttribute(sats, 'aria-pressed')) === 'true', 'satellites are on by default');
      check((await page.textContent(sats + ' .val')).trim() === '4', 'the button counts 4 satellites (ISS once)');
      await page.waitForFunction(() => document.querySelectorAll('.marker[data-layer="sats"]').length === 4, null, { timeout: 10000 })
        .catch(() => {});
      check((await page.$$('.marker[data-layer="sats"]')).length === 4, '4 satellite markers on the globe');
      check((await page.textContent('.marker[aria-label="ISS (ZARYA)"] .marker-label')) === 'ISS', 'the ISS is labelled');
      const lngBefore = await page.evaluate(() => window.GODVIEW.layers.find((l) => l.id === 'sats').html[0].lng);
      await page.waitForTimeout(2500);
      const lngAfter = await page.evaluate(() => window.GODVIEW.layers.find((l) => l.id === 'sats').html[0].lng);
      check(lngBefore !== lngAfter, 'the ISS moves on its own');
      const { opened, underFinger } = await tapMarker(page, '.marker[aria-label="ISS (ZARYA)"]', 'sats', 0);
      check(opened && (await page.textContent('#sheet-title')) === 'ISS (ZARYA)',
        'tapping the ISS opens its details' + (opened ? '' : ' — under the finger: ' + underFinger));
      if (opened) {
        check((await page.textContent('#sheet-rows')).includes('ALTITUDE'), 'which give its altitude');
        check((await page.getAttribute('#sheet-link', 'href')) === 'https://www.n2yo.com/satellite/?s=25544', 'and link to its live track');
        await page.tap('#sheet-close');
      }

      console.log('\nWEATHER');
      const wx = 'button.layer-btn[data-layer="weather"]';
      const wxVal = async () => (await page.textContent(wx + ' .val')).trim();
      check((await page.getAttribute(wx, 'aria-pressed')) === 'false', 'weather waits to be asked for');
      check((await wxVal()) === 'OFF', 'its button says OFF');
      check(await page.evaluate(() => document.getElementById('credits').hidden), 'no credit strip yet');
      await page.tap(wx);
      await page.waitForFunction((sel) => /UTC$/.test(document.querySelector(sel).textContent), wx + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await wxVal()) === '06:10 UTC', 'tapping it shows the radar time (' + (await wxVal()) + ')');
      const tex = await page.evaluate(() => {
        const t = window.GODVIEW.layers.find((l) => l.id === 'weather').texture;
        if (!t) return null;
        /* 22.5°N 45°E sits in the cloud bank; 22.5°S 45°W is open sea. */
        const at = (lat, lng) => {
          const x = Math.floor((lng + 180) / 360 * t.width);
          const y = Math.floor((90 - lat) / 180 * t.height);
          return Array.from(t.data.slice((y * t.width + x) * 4, (y * t.width + x) * 4 + 3));
        };
        return { w: t.width, h: t.height, cloud: at(22.5, 45), sea: at(-22.5, -45) };
      });
      check(tex && tex.w === 2048 && tex.h === 1024, 'a whole-world weather picture is made');
      check(tex && tex.cloud[0] > 150 && tex.cloud[1] > 150, 'clouds are painted where the clouds are');
      check(tex && tex.sea.join() === '3,20,12', 'open sea stays the dark globe colour');
      check(await page.evaluate(() => /^data:image\/jpeg/.test(window.GODVIEW.globe.globeImageUrl() || '')), 'the globe wears it');
      await page.waitForTimeout(800);
      check(await page.evaluate(() => !!window.GODVIEW.globe.globeMaterial().map), 'the globe surface shows the picture');
      const credits = await page.evaluate(() => { const c = document.getElementById('credits'); return c.hidden ? '' : c.textContent; });
      check(credits.includes('NASA') && credits.includes('RainViewer'), 'both sources are credited');
      await page.tap(wx);
      check((await wxVal()) === 'OFF', 'tapping again turns weather off');
      await page.waitForTimeout(1800);
      check(await page.evaluate(() => !window.GODVIEW.globe.globeMaterial().map), 'and the plain dark globe is back');
      check(await page.evaluate(() => document.getElementById('credits').hidden), 'and the credits go');

      console.log('\nPLANES');
      const pl = 'button.layer-btn[data-layer="planes"]';
      const plVal = async () => (await page.textContent(pl + ' .val')).trim();
      check((await page.getAttribute(pl, 'aria-pressed')) === 'false', 'planes wait to be asked for');
      await page.tap(pl);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === '3', pl + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await plVal()) === '3', 'tapping it shows 3 planes (' + (await plVal()) + ')');
      await page.waitForFunction(() => document.querySelectorAll('.marker--plane').length === 3, null, { timeout: 10000 })
        .catch(() => {});
      check((await page.$$('.marker--plane')).length === 3, '3 plane markers on the globe');
      check((await page.$eval('.marker[aria-label="UAL123"]', (e) => e.style.getPropertyValue('--rot'))) === '90deg',
        'UAL123 points east, along its track');
      const ualLng = () => page.evaluate(() => window.GODVIEW.layers.find((l) => l.id === 'planes').html
        .find((h) => h.item.hex === 'a1b2c3').lng);
      const lng0 = await ualLng();
      await page.waitForTimeout(2500);
      check((await ualLng()) > lng0, 'it flies on between updates');
      const ualIndex = await page.evaluate(() => window.GODVIEW.layers.find((l) => l.id === 'planes').html
        .findIndex((h) => h.item.hex === 'a1b2c3'));
      const tapped = await tapMarker(page, '.marker[aria-label="UAL123"]', 'planes', ualIndex);
      check(tapped.opened && (await page.textContent('#sheet-title')) === 'UAL123',
        'tapping it opens its details' + (tapped.opened ? '' : ' — under the finger: ' + tapped.underFinger));
      if (tapped.opened) {
        check((await page.textContent('#sheet-rows')).includes('35,000 ft'), 'which give its height');
        await page.tap('#sheet-close');
      }
      const plCredit = await page.evaluate(() => document.getElementById('credits').textContent);
      check(plCredit.includes('adsb.lol'), 'adsb.lol is credited');
      await page.tap(pl);
      check((await page.$$('.marker--plane')).length === 0 || (await plVal()) === 'OFF', 'tapping again turns planes off');
      await page.evaluate(() => { window.GODVIEW.globe.pointOfView({ lat: 20, lng: -40, altitude: 2.4 }, 0); });

      console.log('\nCAMERAS');
      const cam = 'button.layer-btn[data-layer="cameras"]';
      const camVal = async () => (await page.textContent(cam + ' .val')).trim();
      check((await page.getAttribute(cam, 'aria-pressed')) === 'false', 'cameras wait to be asked for');
      await page.tap(cam);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === '5', cam + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await camVal()) === '5', 'tapping it finds 5 cameras in London, Finland and California (' + (await camVal()) + ')');
      await page.waitForFunction(() => document.querySelectorAll('.marker[data-layer="cameras"]').length === 5, null, { timeout: 10000 })
        .catch(() => {});
      check((await page.$$('.marker[data-layer="cameras"]')).length === 5, '5 camera markers on the globe');
      const trafalgar = await page.evaluate(() => window.GODVIEW.layers.find((l) => l.id === 'cameras').html
        .findIndex((h) => h.item.id === 'tfl:00001.06514'));
      const camTap = await tapMarker(page, '.marker[aria-label="Trafalgar Sq/Cockspur St"]', 'cameras', trafalgar);
      check(camTap.opened && (await page.textContent('#sheet-title')) === 'Trafalgar Sq/Cockspur St',
        'tapping a camera opens it' + (camTap.opened ? '' : ' — under the finger: ' + camTap.underFinger));
      await page.waitForFunction(() => document.getElementById('sheet-image').naturalWidth > 0, null, { timeout: 10000 })
        .catch(() => {});
      const pic = await page.evaluate(() => {
        const i = document.getElementById('sheet-image');
        return { shown: !i.hidden && i.naturalWidth > 0, src: i.getAttribute('src') || '' };
      });
      check(pic.shown, 'and shows its latest picture');
      check(/jamcams\.tfl\.gov\.uk\/00001\.06514\.jpg\?t=\d+$/.test(pic.src), 'fetched fresh, not from the cache');
      check((await page.textContent('#credits')).includes('TfL'), 'TfL is credited');
      await page.tap('#sheet-close');
      check(await page.evaluate(() => document.getElementById('sheet-image').hidden), 'closing it puts the picture away');
      await page.evaluate(() => {
        const l = window.GODVIEW.layers.find((x) => x.id === 'cameras');
        window.GODVIEW.openSheet(l.html.find((h) => h.item.id === 'fi:C04507'));
      });
      await page.waitForFunction(() => !document.getElementById('sheet-image-note').hidden, null, { timeout: 10000 })
        .catch(() => {});
      check(await page.evaluate(() => !document.getElementById('sheet-image-note').hidden &&
        document.getElementById('sheet-image').hidden), 'a picture that will not load says so instead');
      await page.tap('#sheet-close');
      await page.tap(cam);
      await page.evaluate(() => { window.GODVIEW.globe.pointOfView({ lat: 20, lng: -40, altitude: 2.4 }, 0); });

      console.log('\nDETAILS SHEET');
      await page.evaluate(() => window.GODVIEW.openSheet(window.GODVIEW.globe.pointsData()[0]));
      check(await page.evaluate(() => !document.getElementById('sheet').hidden), 'tapping a quake opens the sheet');
      check((await page.textContent('#sheet-title')) === 'M6.1 EARTHQUAKE', 'the sheet names the biggest quake');
      check((await page.textContent('#sheet-rows')).includes('offshore Coquimbo, Chile'), 'and says where it was');
      check((await page.getAttribute('#sheet-link', 'href')) === 'https://earthquake.usgs.gov/earthquakes/eventpage/us0003', 'and links to USGS');
      await page.tap('#sheet-close');
      check(await page.evaluate(() => document.getElementById('sheet').hidden), 'CLOSE hides the sheet');

      console.log('\nTOGGLE');
      await page.tap(btn);
      check((await page.getAttribute(btn, 'aria-pressed')) === 'false', 'tapping turns quakes off');
      check((await page.textContent(btn + ' .val')).trim() === 'OFF', 'the button says OFF');
      check(await page.evaluate(() => window.GODVIEW.globe.pointsData().length === 0), 'the dots are gone');
      await page.tap(btn);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent.trim() === '3', btn + ' .val', { timeout: 10000 })
        .catch(() => {});
      check((await page.textContent(btn + ' .val')).trim() === '3', 'tapping again brings them back');

      console.log('\nPHONE LAYOUT');
      const box = await page.$eval(btn, (e) => { const r = e.getBoundingClientRect(); return { h: r.height, right: r.right }; });
      check(box.h >= 44, 'the layer button is at least 44 px tall (' + Math.round(box.h) + ')');
      check(box.right <= 390, 'the layer button fits on screen');
      const allFit = await page.$$eval('.layer-btn', (els) => els.every((e) => {
        const r = e.getBoundingClientRect();
        return r.left >= 0 && r.right <= window.innerWidth && r.bottom <= window.innerHeight;
      }));
      check(allFit, 'every layer button is on screen at once, no scrolling');
      check(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), 'no sideways scrolling');
      await page.waitForSelector('.mazNav-pill', { timeout: 5000 }).catch(() => {});
      const overlap = await page.evaluate(() => {
        const pill = document.querySelector('.mazNav-pill');
        if (!pill) return 'no pill';
        const a = pill.getBoundingClientRect();
        return ['#topbar h1', '#live', '#clock'].filter((sel) => {
          const b = document.querySelector(sel).getBoundingClientRect();
          return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
        }).join(', ');
      });
      check(overlap === '', 'the top bar stays clear of the MAZ nav pill' + (overlap ? ' — overlaps: ' + overlap : ''));
      const clockRight = await page.$eval('#clock', (e) => e.getBoundingClientRect().right);
      check(clockRight <= 390, 'the clock fits on screen');
      check(problems.length === 0, 'still no errors after all that' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nFEED DOWN');
    {
      const { page, problems } = await openPage(browser, base, { quakes: 'down' });
      const btn = 'button.layer-btn[data-layer="quakes"]';
      check((await page.textContent(btn + ' .val')).trim() === 'OFFLINE', 'the quake button says OFFLINE');
      check((await page.textContent('button.layer-btn[data-layer="sats"] .val')).trim() === '4', 'satellites carry on regardless');
      check(await page.evaluate(() => document.getElementById('fallback').hidden), 'the globe still shows');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nCELESTRAK DOWN');
    {
      const { page, problems } = await openPage(browser, base, { sats: 'down' });
      check((await page.textContent('button.layer-btn[data-layer="sats"] .val')).trim() === 'OFFLINE', 'the SATS button says OFFLINE');
      check((await page.textContent('button.layer-btn[data-layer="quakes"] .val')).trim() === '3', 'quakes carry on regardless');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nWEATHER SOURCES DOWN');
    {
      const { page, problems } = await openPage(browser, base, { weather: 'down' });
      const wx = 'button.layer-btn[data-layer="weather"]';
      await page.tap(wx);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === 'OFFLINE', wx + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await page.textContent(wx + ' .val')).trim() === 'OFFLINE', 'the WEATHER button says OFFLINE');
      check(await page.evaluate(() => !window.GODVIEW.globe.globeMaterial().map), 'the globe stays plain');
      check((await page.textContent('button.layer-btn[data-layer="sats"] .val')).trim() === '4', 'satellites carry on regardless');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nADSB.LOL DOWN, OPENSKY UP');
    {
      const { page, problems } = await openPage(browser, base, { adsb: 'down' });
      const pl = 'button.layer-btn[data-layer="planes"]';
      await page.tap(pl);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === '2', pl + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await page.textContent(pl + ' .val')).trim() === '2', 'OpenSky fills in: 2 planes');
      check((await page.textContent('#credits')).includes('OpenSky'), 'and OpenSky is credited');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nPLANES THROUGH THE RELAY');
    {
      const { page, problems } = await openPage(browser, base, { relay: 'ok', adsb: 'down', opensky: 'down' });
      const pl = 'button.layer-btn[data-layer="planes"]';
      await page.tap(pl);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === '3', pl + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await page.textContent(pl + ' .val')).trim() === '3', 'the relay brings 3 planes even though both services refuse the page');
      check((await page.textContent('#credits')).includes('adsb.fi (via relay)'), 'and the source is credited');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }
    {
      const { page, problems } = await openPage(browser, base, { relay: 'down' });
      const pl = 'button.layer-btn[data-layer="planes"]';
      await page.tap(pl);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === 'OFFLINE', pl + ' .val', { timeout: 30000 })
        .catch(() => {});
      check((await page.textContent(pl + ' .val')).trim() === 'OFFLINE', 'a relay with no source answering shows OFFLINE');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nBOTH FLIGHT SOURCES DOWN');
    {
      const { page, problems } = await openPage(browser, base, { adsb: 'down', opensky: 'down' });
      const pl = 'button.layer-btn[data-layer="planes"]';
      await page.tap(pl);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === 'OFFLINE', pl + ' .val', { timeout: 30000 })
        .catch(() => {});
      check((await page.textContent(pl + ' .val')).trim() === 'OFFLINE', 'the PLANES button says OFFLINE');
      check((await page.textContent('button.layer-btn[data-layer="quakes"] .val')).trim() === '3', 'quakes carry on regardless');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nEVERY CAMERA LIST DOWN');
    {
      const { page, problems } = await openPage(browser, base, { cameras: 'down' });
      const cam = 'button.layer-btn[data-layer="cameras"]';
      await page.tap(cam);
      await page.waitForFunction((sel) => document.querySelector(sel).textContent === 'OFFLINE', cam + ' .val', { timeout: 15000 })
        .catch(() => {});
      check((await page.textContent(cam + ' .val')).trim() === 'OFFLINE', 'the CAMERAS button says OFFLINE');
      check((await page.textContent('button.layer-btn[data-layer="sats"] .val')).trim() === '4', 'satellites carry on regardless');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nGLOBE LIBRARY UNREACHABLE');
    {
      const { page, problems } = await openPage(browser, base, { globe: 'down' });
      check(await page.evaluate(() => !document.getElementById('fallback').hidden), 'a fallback message shows');
      check((await page.textContent('#fallback-msg')).includes('could not be downloaded'), 'and says what went wrong');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }
  } finally {
    await browser.close();
    server.close();
  }

  console.log('\n' + (failures ? failures + ' check(s) FAILED' : 'all checks passed'));
  process.exit(failures ? 1 : 0);
})();
