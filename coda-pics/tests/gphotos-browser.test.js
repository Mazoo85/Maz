#!/usr/bin/env node
/*
 * CODA PICS — the Google Photos connector, driven through the real page.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node coda-pics/tests/gphotos-browser.test.js
 *
 * gphotos.test.js proves the connector's own arithmetic. This proves the thing
 * that actually matters to a person: that pressing the buttons on the page
 * signs in, opens a picker, brings photographs back, and leaves their colours
 * in the palette box — with a real browser, real sessionStorage, a real
 * redirect out and back, and real image decoding.
 *
 * Google is faked, and only Google: the two seams the app leaves for it
 * (CODA_GPHOTOS_TRANSPORT, CODA_GPHOTOS_GO) replace the network and the
 * navigation, and nothing else about the page is changed. So this test fails
 * for the same reasons the real thing would fail — a mis-built auth URL, a
 * state that is not checked, a code left in the address bar, a photo that
 * arrives but is never measured.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
let chromium = null;
for (const spec of [
  'playwright',
  path.join(ROOT, 'music', 'tests', 'node_modules', 'playwright'),
  path.join(ROOT, 'node_modules', 'playwright')
]) {
  try { chromium = require(spec).chromium; break; } catch (e) { /* next */ }
}
if (!chromium) {
  console.error('Playwright is not installed. Run: npm --prefix music/tests install');
  process.exit(2);
}

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8219;
const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.webmanifest': 'application/manifest+json', '.json': 'application/json',
  '.png': 'image/png', '.svg': 'image/svg+xml', '.md': 'text/plain'
};
const server = http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split('?')[0].split('#')[0]);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.join(ROOT, p);
  if (!file.startsWith(ROOT) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404); res.end('not found'); return;
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  res.end(fs.readFileSync(file));
});

let failures = 0;
function check(cond, msg) {
  console.log((cond ? '  ok    ' : '  FAIL  ') + msg);
  if (!cond) failures++;
}

function launchOptions() {
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--mute-audio'] };
  for (const c of [
    process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    '/opt/pw-browsers/chromium/chrome-linux/chrome'
  ]) {
    if (c && fs.existsSync(c)) { opts.executablePath = c; break; }
  }
  return opts;
}

/*
 * The whole of Google, in one function, installed before any of the app's own
 * script runs so the app never sees a real endpoint. It answers the five calls
 * the flow makes and records every one of them for the assertions below.
 */
const FAKE_GOOGLE = () => {
  window.__gp = { go: [], calls: [] };
  window.CODA_GPHOTOS_GO = (url) => { window.__gp.go.push(url); };

  /* A real PNG, made here, so the app's decoder and canvas do real work. */
  function png(hue) {
    const c = document.createElement('canvas');
    c.width = 64; c.height = 48;
    const ctx = c.getContext('2d');
    const g = ctx.createLinearGradient(0, 0, 0, 48);
    g.addColorStop(0, 'hsl(' + hue + ',70%,68%)');
    g.addColorStop(1, 'hsl(' + ((hue + 40) % 360) + ',55%,24%)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 64, 48);
    return new Promise((r) => c.toBlob(r, 'image/png'));
  }

  function json(body) {
    return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(body) });
  }

  window.CODA_GPHOTOS_TRANSPORT = (url, opts) => {
    window.__gp.calls.push({ url: url, method: (opts && opts.method) || 'GET',
      auth: (opts && opts.headers && opts.headers.Authorization) || null });

    if (url.indexOf('oauth2.googleapis.com/token') >= 0) {
      window.__gp.tokenBody = opts.body;
      return json({ access_token: 'tok-123', expires_in: 3600 });
    }
    if (/\/v1\/sessions$/.test(url)) {
      return json({ id: 'sess-1', pickerUri: 'https://photos.google.com/picker/sess-1',
        pollingConfig: { pollInterval: '1s' } });
    }
    if (/\/v1\/sessions\//.test(url)) {
      return json({ id: 'sess-1', mediaItemsSet: true });
    }
    if (url.indexOf('/v1/mediaItems') >= 0) {
      return json({ mediaItems: [
        { id: 'a', mediaFile: { filename: 'dawn.jpg', mimeType: 'image/jpeg',
          baseUrl: 'https://lh3.example/a' } },
        { id: 'b', mediaFile: { filename: 'dusk.jpg', mimeType: 'image/jpeg',
          baseUrl: 'https://lh3.example/b' } },
        { id: 'c', mediaFile: { filename: 'clip.mp4', mimeType: 'video/mp4',
          baseUrl: 'https://lh3.example/c' } }
      ] });
    }
    if (url.indexOf('lh3.example') >= 0) {
      return png(url.indexOf('/a') >= 0 ? 30 : 210)
        .then((b) => ({ ok: true, status: 200, blob: () => Promise.resolve(b) }));
    }
    return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({}) });
  };
};

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 1100, height: 900 } });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

  const BASE = `http://127.0.0.1:${PORT}/coda-pics/`;

  try {
    await page.addInitScript(FAKE_GOOGLE);

    /* ------------------------------------------------------- signing in */
    console.log('\nSigning in');
    await page.goto(BASE, { waitUntil: 'load' });
    await page.waitForTimeout(250);

    check(await page.locator('#gpRedirect').textContent() === BASE,
      'the page shows the exact redirect URI Google will need');

    await page.click('#gphotos > summary');
    await page.click('#gpConnect');
    check((await page.locator('#gpNote').textContent()).indexOf('client ID') >= 0,
      'connecting with no client ID says so instead of failing silently');

    await page.fill('#gpClientId', 'test-client.apps.googleusercontent.com');
    await page.click('#gpConnect');
    await page.waitForFunction(() => window.__gp.go.length > 0, null, { timeout: 5000 });

    const authUrl = new URL(await page.evaluate(() => window.__gp.go[0]));
    const q = authUrl.searchParams;
    check(authUrl.origin + authUrl.pathname === 'https://accounts.google.com/o/oauth2/v2/auth',
      'it sends you to Google, and only to Google');
    check(q.get('client_id') === 'test-client.apps.googleusercontent.com',
      'it uses the client ID that was typed in');
    check(q.get('redirect_uri') === BASE, 'it asks to come back to this exact page');
    check(q.get('scope') === 'https://www.googleapis.com/auth/photospicker.mediaitems.readonly',
      'it asks for the picker scope and nothing wider');
    check(q.get('code_challenge_method') === 'S256' && (q.get('code_challenge') || '').length > 20,
      'it proves itself with PKCE rather than a secret it cannot keep');

    /* A page with no server cannot keep a secret in a file, so the verifier
     * has to survive the redirect in the browser — and nowhere else. */
    const kept = await page.evaluate(() => ({
      session: Object.keys(window.sessionStorage).filter((k) => k.indexOf('gphotos') === 0),
      local: Object.keys(window.localStorage).filter((k) => k.indexOf('gphotos') >= 0)
    }));
    check(kept.session.indexOf('gphotos.verifier') >= 0 && kept.session.indexOf('gphotos.state') >= 0,
      'the verifier and state ride out the redirect in sessionStorage');
    check(kept.local.length === 1 && kept.local[0].indexOf('clientId') > 0,
      'the only thing kept for good is the client ID, which is not a secret');

    /* ------------------------------- a sign-in that did not start here */
    await page.goto(BASE + '?code=stolen&state=not-ours', { waitUntil: 'load' });
    await page.waitForTimeout(400);
    check((await page.locator('#gpNote').textContent()).indexOf('did not come from this page') >= 0,
      'a code arriving with the wrong state is refused');
    check(await page.locator('#gpPick').isHidden(), 'and nothing is connected by it');

    /* ------------------------------------------------ back from Google */
    const state = q.get('state');
    await page.goto(BASE + '?code=good-code&state=' + encodeURIComponent(state),
      { waitUntil: 'load' });
    await page.waitForSelector('#gpPick:not([hidden])', { timeout: 8000 });
    check(true, 'the real state is accepted and the app connects');

    const body = await page.evaluate(() => window.__gp.tokenBody);
    check(/grant_type=authorization_code/.test(body) && /code_verifier=/.test(body) &&
      /code=good-code/.test(body),
      'the code is swapped for a token with the verifier that started it');

    check(page.url() === BASE,
      'the single-use code is wiped from the address bar, not left in history');
    check(await page.evaluate(() =>
      !window.sessionStorage.getItem('gphotos.verifier') &&
      !window.sessionStorage.getItem('gphotos.state')),
      'the verifier is thrown away the moment it has been used');

    /* -------------------------------------------------- picking photos */
    console.log('\nPicking photos');
    await page.click('#gpPick');
    await page.waitForSelector('#paletteBox:not([hidden])', { timeout: 15000 });
    await page.waitForFunction(
      () => (document.getElementById('gpNote').textContent || '').indexOf('Brought in') >= 0,
      null, { timeout: 15000 });

    check((await page.evaluate(() => window.__gp.go)).indexOf(
      'https://photos.google.com/picker/sess-1') >= 0,
      'you are sent to Google’s own picker to choose');

    const calls = await page.evaluate(() => window.__gp.calls);
    const photoCalls = calls.filter((c) => c.url.indexOf('lh3.example') >= 0);
    check(photoCalls.length === 2,
      'only the two photographs are fetched — the video is left alone');
    check(photoCalls.every((c) => /=w640-h640$/.test(c.url)),
      'each is asked for at a size worth measuring, not full resolution');
    check(calls.filter((c) => c.url.indexOf('accounts.google') >= 0 ||
      c.url.indexOf('photospicker') >= 0 || c.url.indexOf('lh3.example') >= 0)
      .filter((c) => c.url.indexOf('photospicker') >= 0 || c.url.indexOf('lh3.example') >= 0)
      .every((c) => c.auth === 'Bearer tok-123'),
      'every call to Google carries the token, and a baseUrl alone is not a public link');

    const chips = await page.locator('#paletteChips .swatch').allTextContents();
    check(chips.length >= 3, 'both photos left a palette behind, plus their mixture');
    check(chips.some((t) => /Mixture/i.test(t)),
      'the mixture of what was picked is offered as a palette of its own');
    check(await page.locator('#photoInfo').isVisible(),
      'the last photo is shown, measured, and ready to paint with');

    /* The whole point of the feature: those colours must reach a picture. */
    await page.fill('#prompt', 'a lighthouse on a cliff at sunset');
    await page.click('#paint');
    await page.waitForFunction(
      () => Number(document.getElementById('canvas').dataset.painted || 0) > 0,
      null, { timeout: 30000 });
    check(await page.evaluate(() => {
      const c = document.getElementById('canvas');
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      const seen = new Set();
      for (let i = 0; i < d.length; i += 4 * 97) seen.add((d[i] >> 3) + ',' + (d[i + 1] >> 3) + ',' + (d[i + 2] >> 3));
      return seen.size > 12;
    }), 'a picture is painted from photos that came out of Google Photos');

    /* -------------------------------------------------- disconnecting */
    await page.click('#gpForget');
    check(await page.locator('#gpPick').isHidden() && await page.locator('#gpConnect').isVisible(),
      'disconnecting puts the connect button back');
    check((await page.locator('#paletteChips .swatch').allTextContents()).length >= 3,
      'and the colours already kept survive it — they were never Google’s');

    check(problems.length === 0, 'nothing threw anywhere in all of that' +
      (problems.length ? ' — ' + problems.join('; ') : ''));
  } catch (err) {
    check(false, 'the run itself failed: ' + err.message);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(failures ? '\nFAILED ' + failures + ' check(s)' : '\nAll checks passed');
  process.exit(failures ? 1 : 0);
})();
