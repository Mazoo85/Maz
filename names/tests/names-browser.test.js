/*
 * NAME FORGE — end-to-end tests in a real browser.
 *
 * The logic suite proves the engine is honest; this one proves the app works:
 * the page boots clean, the button really rolls, every name it puts on screen
 * is an adjective and then a noun, the controls do what they say, a batch
 * exports a real file, a saved name survives a reload, and none of it scrolls
 * sideways on a phone.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node names/tests/names-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');

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

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8214;
const MIME = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.md': 'text/plain',
  '.webmanifest': 'application/manifest+json',
  '.png': 'image/png'
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
  const opts = { args: ['--no-sandbox'] };
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

const name = (page) => page.textContent('#nameOut');

/*
 * What is on screen: the name, and the two words it was built from. The chip
 * always lists them adjective first ("crimson + falcon"), so comparing the
 * name's own first word against the adjective says which way this one landed.
 */
async function rolled(page) {
  const text = (await page.textContent('#nameOut')).trim();
  const chip = (await page.textContent('#orderChip')).trim();
  const adjective = chip.split(' + ')[0];
  const noun = chip.split(' + ')[1].split(' · ')[0];
  return {
    text: text,
    adjective: adjective,
    noun: noun,
    adjectiveFirst: text.toLowerCase().split(/[ _-]/)[0] === adjective
  };
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}/names/`;
  const browser = await chromium.launch(launchOptions());

  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    const problems = [];
    page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
    page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

    console.log('\nBOOT');
    await page.goto(base, { waitUntil: 'load' });
    check(problems.length === 0, 'boots with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));

    const first = (await name(page)).trim();
    check(/^[A-Z][a-z]+ [A-Z][a-z]+$/.test(first), `a name is on screen straight away (${first})`);
    check(/1,000,000/.test(await page.textContent('#scaleStat')),
      'it states how many names it can make');

    console.log('\nROLLING');
    const seen = new Set();
    let adjFirst = 0;
    let wellShaped = 0;
    for (let i = 0; i < 60; i++) {
      await page.click('#roll');
      const one = await rolled(page);
      seen.add(one.text);
      if (one.adjectiveFirst) adjFirst++;
      if (/^[A-Z][a-z]+ [A-Z][a-z]+$/.test(one.text)) wellShaped++;
    }
    check(seen.size > 55, `60 rolls gave ${seen.size} different names`);
    check(adjFirst === 60, `all 60 rolls led with their adjective (${adjFirst}/60)`);
    check(wellShaped === 60, 'and every one of them read as "Adjective Noun"');

    console.log('\nCONTROLS');
    await page.selectOption('#order', 'noun-adjective');
    await page.click('#roll');
    check((await rolled(page)).adjectiveFirst === false,
      'asking for "noun first" turns the name round');

    await page.selectOption('#order', 'random');
    let mixed = new Set();
    for (let i = 0; i < 40; i++) {
      await page.click('#roll');
      mixed.add((await rolled(page)).adjectiveFirst);
    }
    check(mixed.size === 2, 'asking for "let the dice decide" gives both orders');

    await page.selectOption('#order', 'adjective-noun');
    await page.click('#roll');
    check((await rolled(page)).adjectiveFirst, 'and going back to "adjective first" sticks');

    await page.selectOption('#style', 'hyphen');
    await page.click('#roll');
    check(/^[a-z]+-[a-z]+$/.test((await name(page)).trim()), 'hyphen-case styles the name');
    await page.selectOption('#style', 'fused');
    await page.click('#roll');
    check(/^[A-Z][a-z]+[A-Z][a-z]+$/.test((await name(page)).trim()), 'OneWord styles the name');
    await page.selectOption('#style', 'title');

    console.log('\nBATCH');
    await page.click('.batch-btn[data-n="25"]');
    const rows = await page.$$eval('#batchList .n', (els) => els.map((e) => e.textContent.trim()));
    check(rows.length === 25, `a batch of 25 renders 25 rows (got ${rows.length})`);
    check(new Set(rows).size === 25, 'and they are all different');

    const download = await Promise.all([
      page.waitForEvent('download'),
      page.click('#exportTxt')
    ]).then(([d]) => d);
    const file = await download.path();
    const lines = fs.readFileSync(file, 'utf8').trim().split('\n');
    check(download.suggestedFilename() === 'names-25.txt', 'the .txt export is named for its size');
    check(lines.length === 25 && lines[0] === rows[0], 'the file holds the names that were on screen');

    console.log('\nSAVING');
    await page.click('#batchList li:first-child button[title="Save"]');
    check((await page.textContent('#libCount')) === '1', 'saving a name from the batch adds it');
    await page.click('#save');
    const saved = await page.$$eval('#libraryList .n', (els) => els.map((e) => e.textContent.trim()));
    check(saved.length === 2 && saved.indexOf(rows[0]) !== -1, 'the rolled name is saved too');

    await page.reload({ waitUntil: 'load' });
    const afterReload = await page.$$eval('#libraryList .n', (els) => els.map((e) => e.textContent.trim()));
    check(afterReload.join('|') === saved.join('|'), 'saved names survive a reload');

    console.log('\nOFFLINE');
    /*
     * The point of the service worker is a page that still works with no
     * signal, so the only honest test is to take the network away. A fresh
     * context gets its own service-worker registry, so this starts clean:
     * open the app once to let it install, then cut the connection at the
     * browser and reload. Everything after that comes out of the cache.
     */
    const offlineCtx = await browser.newContext();
    const off = await offlineCtx.newPage();
    const offProblems = [];
    off.on('pageerror', (e) => offProblems.push('uncaught: ' + e.message));
    await off.goto(base, { waitUntil: 'load' });
    // navigator.serviceWorker.ready never settles when nothing registers, so
    // it is raced against a deadline: without this, a page that lost its
    // registration would hang this suite instead of failing it.
    const registered = await off.evaluate(() => Promise.race([
      navigator.serviceWorker.ready.then((r) => !!r.active).catch(() => false),
      new Promise((res) => setTimeout(() => res(false), 10000))
    ]));
    check(registered, 'the service worker installs and activates');

    const manifest = await off.evaluate(() =>
      fetch('manifest.webmanifest').then((r) => r.json()).then((m) => ({
        name: m.name,
        display: m.display,
        icons: m.icons.length,
        maskable: m.icons.some((i) => i.purpose === 'maskable')
      })).catch(() => null));
    check(manifest !== null, 'the manifest is served and is valid JSON');
    check(manifest && manifest.display === 'standalone',
      'it asks to open as an app, not a browser tab');
    check(manifest && manifest.icons === 3 && manifest.maskable,
      'it declares icons including a maskable one');

    await offlineCtx.setOffline(true);
    // A page with no cache to fall back on fails this reload outright, so it
    // is reported as the failing check it is rather than a stack trace.
    let reloaded = true;
    await off.reload({ waitUntil: 'load' }).catch((e) => {
      reloaded = false;
      check(false, 'the app still loads with the network cut (' + e.message.split('\n')[0] + ')');
    });
    if (reloaded) {
      check((await off.title()).indexOf('Name Forge') !== -1, 'the app still loads with the network cut');
    }
    if (reloaded) {
      await off.click('#roll');
      const offlineName = (await off.textContent('#nameOut')).trim();
      check(/\S/.test(offlineName) && offlineName !== 'Roll your first name',
        `and still rolls a name offline (${offlineName})`);
      const offlineStyled = await off.evaluate(() =>
        getComputedStyle(document.querySelector('.stage')).display !== 'inline');
      check(offlineStyled, 'with its stylesheet, not as unstyled text');
    }
    check(offProblems.length === 0, 'nothing threw while offline' +
      (offProblems.length ? ': ' + offProblems.join('; ') : ''));
    await offlineCtx.setOffline(false);
    await offlineCtx.close();

    console.log('\nPHONE');
    const phone = await browser.newPage({ viewport: { width: 390, height: 780 } });
    await phone.goto(base, { waitUntil: 'load' });
    await phone.click('#roll');
    const overflow = await phone.evaluate(() =>
      document.documentElement.scrollWidth - document.documentElement.clientWidth);
    check(overflow <= 1, `no sideways scrolling at 390px (overflow ${overflow}px)`);
    check(/\S/.test((await phone.textContent('#nameOut')).trim()), 'and it still rolls a name');
    await phone.close();
    await page.close();
  } finally {
    await browser.close();
    server.close();
  }

  console.log(failures ? `\n✖ ${failures} failing check(s)\n` : '\n✓ NAME FORGE browser tests passed\n');
  process.exit(failures ? 1 : 0);
})().catch((e) => {
  console.error(e);
  server.close();
  process.exit(1);
});
