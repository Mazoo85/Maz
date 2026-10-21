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

/* A phone-sized page with the network answered by us. `quakes`, `sats` and
 * `globe` are 'ok' or 'down'. Errors caused on purpose (a feed we cut) are not
 * problems. */
async function openPage(browser, base, { quakes = 'ok', sats = 'ok', globe = 'ok' } = {}) {
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
      /* Turn the Earth to face the ISS, then tap its marker like a finger would. */
      await page.evaluate(() => {
        const g = window.GODVIEW.globe;
        g.controls().autoRotate = false;
        const iss = window.GODVIEW.layers.find((l) => l.id === 'sats').html[0];
        g.pointOfView({ lat: iss.lat, lng: iss.lng, altitude: 2 }, 0);
      });
      await page.waitForTimeout(1200);
      /* It never stops moving, so tap where it is right now, as a finger would. */
      const issBox = await page.$eval('.marker[aria-label="ISS (ZARYA)"]', (e) => {
        const r = e.getBoundingClientRect();
        return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
      });
      await page.touchscreen.tap(issBox.x, issBox.y);
      await page.waitForTimeout(200);
      check((await page.textContent('#sheet-title')) === 'ISS (ZARYA)', 'tapping the ISS opens its details');
      check((await page.textContent('#sheet-rows')).includes('ALTITUDE'), 'which give its altitude');
      check((await page.getAttribute('#sheet-link', 'href')) === 'https://www.n2yo.com/satellite/?s=25544', 'and link to its live track');
      await page.tap('#sheet-close');

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
