/*
 * smoke-site — drives the whole site in a real browser.
 *
 * check-links proves the links *exist*; this proves they *work*: the hub lists
 * every project, each app boots without throwing, and the shared nav in each
 * app really does open and lead back to the hub.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node scripts/smoke-site.cjs
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');

/* Playwright may live at the repo root or under music/tests. */
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

/* ------------------------------------------------------------- server */
const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8211;
const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.md': 'text/plain'
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
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--mute-audio'] };
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

/* Collect console errors and uncaught exceptions for one page load. */
function watch(page) {
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => {
    if (m.type() === 'error') problems.push('console: ' + m.text());
  });
  page.on('requestfailed', (r) => {
    const url = r.url();
    if (url.startsWith(`http://127.0.0.1:${PORT}`)) {
      problems.push('failed request: ' + url.replace(`http://127.0.0.1:${PORT}`, ''));
    }
  });
  return problems;
}

const APPS = [
  { id: 'zomboid', url: '/zomboid/', name: 'ZOMBOID: ANCHORAGE' },
  { id: 'shooter', url: '/shooter/', name: 'DEAD SECTOR' },
  { id: 'music', url: '/music/', name: 'SONG FORGE' },
  { id: 'madlibs', url: '/madlibs/', name: 'MADLIBS STORY FORGE' }
];

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}`;
  const browser = await chromium.launch(launchOptions());

  try {
    /* ------------------------------------------------- the hub itself */
    console.log('\nHUB  /');
    {
      const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
      const problems = watch(page);
      await page.goto(base + '/', { waitUntil: 'load' });
      await page.waitForTimeout(300);

      check(problems.length === 0, 'loads with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));
      check((await page.title()) === 'MAZ ARCADE', 'title is MAZ ARCADE');

      const cards = await page.$$eval('a.card', (els) =>
        els.map((e) => ({ href: e.getAttribute('href'), name: e.querySelector('h2').textContent }))
      );
      check(cards.length === 7, `lists all 7 projects (found ${cards.length})`);

      // Every card must lead somewhere the server will actually serve.
      let dead = [];
      for (const c of cards) {
        const target = c.href.endsWith('/') ? c.href + 'index.html' : c.href;
        const res = await page.request.get(base + '/' + target.replace(/^\//, ''));
        if (!res.ok()) dead.push(c.name + ' → ' + c.href);
      }
      check(dead.length === 0, 'every card opens a real page' + (dead.length ? ' — dead: ' + dead.join(', ') : ''));

      /* Click straight through to the first game, the way a visitor would. */
      await page.click('a.card');
      await page.waitForLoadState('load');
      check(/\/zomboid\//.test(page.url()), 'clicking the first card navigates into the game');

      await page.close();
    }

    /* ------------------------------------------------- each app + nav */
    for (const app of APPS) {
      console.log(`\nAPP  ${app.url}`);
      const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
      const problems = watch(page);
      await page.goto(base + app.url, { waitUntil: 'load' });
      await page.waitForTimeout(700); // let the app boot and the nav inject

      check(problems.length === 0, `${app.name} boots clean` + (problems.length ? ' — ' + problems.join('; ') : ''));

      const pill = await page.$('.mazNav-pill');
      check(!!pill, 'shared nav pill is present');
      if (!pill) {
        await page.close();
        continue;
      }

      await pill.click();
      await page.waitForTimeout(300);

      const items = await page.$$eval('.mazNav-item', (els) =>
        els.map((e) => ({ href: e.getAttribute('href'), current: e.getAttribute('aria-current') }))
      );
      check(items.length === 7, `nav menu lists all 7 projects (found ${items.length})`);
      check(
        items.some((i) => i.current === 'page'),
        'nav marks the current project'
      );

      const hubHref = await page.$eval('.mazNav-logo', (e) => e.getAttribute('href'));
      check(!!hubHref, 'nav has a link back to the hub');

      // Actually walk back to the hub and confirm we land there.
      await page.click('.mazNav-logo');
      await page.waitForLoadState('load');
      check((await page.title()) === 'MAZ ARCADE', 'nav really returns to the hub');

      await page.close();
    }

    /* ------------------------------------- the games must actually draw */
    for (const app of APPS.slice(0, 2)) {
      console.log(`\nDRAW ${app.url}`);
      const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
      await page.goto(base + app.url, { waitUntil: 'load' });
      await page.waitForTimeout(1500);
      const painted = await page.evaluate(() => {
        const c = document.querySelector('canvas');
        if (!c || !c.width || !c.height) return false;
        const ctx = c.getContext('2d');
        if (!ctx) return false;
        const d = ctx.getImageData(0, 0, Math.min(c.width, 200), Math.min(c.height, 200)).data;
        for (let i = 0; i < d.length; i += 4) {
          if (d[i] || d[i + 1] || d[i + 2]) return true; // any non-black pixel
        }
        return false;
      });
      check(painted, `${app.name} paints its canvas`);
      await page.close();
    }
  } finally {
    await browser.close();
    server.close();
  }

  console.log(failures ? `\n✖ ${failures} failing check(s)\n` : '\n✓ site smoke test passed\n');
  process.exit(failures ? 1 : 0);
})().catch((e) => {
  console.error(e);
  server.close();
  process.exit(1);
});
