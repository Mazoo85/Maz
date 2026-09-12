#!/usr/bin/env node
/*
 * CODA PICS — the visual test.
 *
 *   node coda-pics/tests/visual.test.js            # check against the baseline
 *   node coda-pics/tests/visual.test.js --update   # re-record the baseline
 *
 * The other suites prove the app *drew something*. This one proves it drew the
 * *same* thing. That matters more here than in most projects: the promise the
 * gallery and every shared link rest on is that one prompt and one seed always
 * give one picture, and a quiet change anywhere in the painter breaks that for
 * pictures people have already kept.
 *
 * Each case is reduced to a 64-bit average hash — an 8x8 grid of "is this cell
 * brighter than the picture's mean?". That ignores the last bit of antialiasing
 * while still catching any real change of shape, colour or composition. A case
 * is a failure past a small Hamming distance, not on any difference at all.
 *
 * When a change to the painter is deliberate, re-record with --update and the
 * diff shows exactly which pictures moved.
 */
'use strict';

const fs = require('fs');
const http = require('http');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const BASELINE = path.join(__dirname, 'baseline.json');
const UPDATE = process.argv.indexOf('--update') >= 0;
const TOLERANCE = 6;              // bits of a 64-bit hash

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

/* The cases. Fixed prompt, fixed seed, fixed style: one picture each, chosen to
 * touch every part of the painter — sky, water, ground, a subject with a rim
 * light, a reflection, weather, and several finishing passes. */
const CASES = [
  { name: 'dragon over snow',      prompt: 'a red dragon over snowy mountains at sunset', seed: 3, style: 'auto' },
  { name: 'lighthouse in a storm', prompt: 'a lonely lighthouse in a storm',              seed: 5, style: 'auto' },
  { name: 'ship at sea',           prompt: 'a tall ship on the open sea at sunset',       seed: 1, style: 'auto' },
  { name: 'stag in a meadow',      prompt: 'a stag in a meadow at dawn',                  seed: 2, style: 'auto' },
  { name: 'neon city',             prompt: 'neon city street in the rain at night',       seed: 4, style: 'neon' },
  { name: 'cat under a tree',      prompt: 'a cat under a tree at night',                 seed: 2, style: 'auto' },
  { name: 'castle, watercolour',   prompt: 'a castle in the mountains at sunset',         seed: 4, style: 'watercolour' },
  { name: 'castle, blueprint',     prompt: 'a castle in the mountains at sunset',         seed: 4, style: 'blueprint' },
  { name: 'castle, stained glass', prompt: 'a castle in the mountains at sunset',         seed: 4, style: 'glass' },
  { name: 'whale and moon',        prompt: 'a whale under a huge moon',                   seed: 7, style: 'poster' },
  { name: 'cabin in falling snow', prompt: 'a tiny cabin in falling snow',                seed: 6, style: 'storybook' },
  { name: 'mushrooms in a cave',   prompt: 'giant mushrooms in a glowing cave',           seed: 5, style: 'auto' }
];

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.webmanifest': 'application/manifest+json', '.png': 'image/png',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.md': 'text/plain'
};
const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8213;
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

function distance(a, b) {
  if (!a || !b || a.length !== b.length) return 64;
  let bits = 0;
  for (let i = 0; i < a.length; i++) {
    let x = parseInt(a[i], 16) ^ parseInt(b[i], 16);
    while (x) { bits += x & 1; x >>= 1; }
  }
  return bits;
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 800, height: 600 } });
  const problems = [];
  page.on('pageerror', (e) => problems.push(e.message));

  let failures = 0;
  try {
    await page.goto(`http://127.0.0.1:${PORT}/coda-pics/`, { waitUntil: 'load' });

    const hashes = await page.evaluate((cases) => {
      /* Paint each case at a fixed small size and reduce it to 64 bits. */
      function hashOf(c) {
        const W = 192, H = 108;
        const canvas = document.createElement('canvas');
        canvas.width = W; canvas.height = H;
        const ctx = canvas.getContext('2d');
        const spec = window.CodaPrompt.parse(c.prompt, { seed: c.seed, style: c.style });
        const pal = window.CodaPaint.render(ctx, W, H, spec);
        window.CodaFinish.apply(ctx, W, H, spec, pal);

        const d = ctx.getImageData(0, 0, W, H).data;
        const cells = [];
        for (let gy = 0; gy < 8; gy++) {
          for (let gx = 0; gx < 8; gx++) {
            let sum = 0, n = 0;
            const x0 = Math.floor(gx * W / 8), x1 = Math.floor((gx + 1) * W / 8);
            const y0 = Math.floor(gy * H / 8), y1 = Math.floor((gy + 1) * H / 8);
            for (let y = y0; y < y1; y++) {
              for (let x = x0; x < x1; x++) {
                const i = (y * W + x) * 4;
                sum += 0.2126 * d[i] + 0.7152 * d[i + 1] + 0.0722 * d[i + 2];
                n++;
              }
            }
            cells.push(sum / Math.max(n, 1));
          }
        }
        const mean = cells.reduce((a, b) => a + b, 0) / cells.length;
        let hex = '';
        for (let i = 0; i < 64; i += 4) {
          let nib = 0;
          for (let b = 0; b < 4; b++) nib = (nib << 1) | (cells[i + b] > mean ? 1 : 0);
          hex += nib.toString(16);
        }
        return hex;
      }
      const out = {};
      cases.forEach((c) => { out[c.name] = hashOf(c); });
      return out;
    }, CASES);

    if (problems.length) {
      console.log('  FAIL  the page threw while painting: ' + problems.join('; '));
      failures++;
    }

    if (UPDATE) {
      fs.writeFileSync(BASELINE, JSON.stringify({
        note: 'Average hashes of fixed prompts. Re-record with: node coda-pics/tests/visual.test.js --update',
        tolerance: TOLERANCE,
        hashes: hashes
      }, null, 2) + '\n');
      console.log(`Recorded ${Object.keys(hashes).length} baseline pictures.`);
    } else if (!fs.existsSync(BASELINE)) {
      console.log('  FAIL  no baseline recorded — run with --update once, and commit it');
      failures++;
    } else {
      const base = JSON.parse(fs.readFileSync(BASELINE, 'utf8')).hashes || {};
      Object.keys(hashes).forEach((name) => {
        const d = distance(hashes[name], base[name]);
        const ok = base[name] && d <= TOLERANCE;
        console.log((ok ? '  ok    ' : '  FAIL  ') + name +
          (ok ? '' : ` — moved ${d} bits (was ${base[name] || 'not recorded'}, now ${hashes[name]})`));
        if (!ok) failures++;
      });
      Object.keys(base).forEach((name) => {
        if (!(name in hashes)) {
          console.log('  FAIL  ' + name + ' — in the baseline but no longer painted');
          failures++;
        }
      });
    }
  } catch (err) {
    console.log('  FAIL  the run itself failed: ' + err.message);
    failures++;
  } finally {
    await browser.close();
    server.close();
  }

  if (!UPDATE) {
    console.log(failures
      ? `\nFAILED ${failures} picture(s) changed. If that was deliberate, re-record:\n  node coda-pics/tests/visual.test.js --update`
      : '\nEvery picture is unchanged');
  }
  process.exit(failures ? 1 : 0);
})();
