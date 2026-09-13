#!/usr/bin/env node
/*
 * CODA PICS — the browser test.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node coda-pics/tests/coda-browser.test.js
 *
 * The logic tests prove the pipeline is sound against a recording canvas. This
 * proves the page a person actually touches works: that pressing the button
 * puts real, varied pixels on a real canvas, that the controls change what is
 * drawn, that the gallery keeps and restores a picture, and that a saved file
 * comes out the other end. It also fails if a picture takes absurdly long,
 * because an image maker that stalls the phone is broken even when correct.
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

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8212;
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

/* What is actually on the canvas: how many distinct colours, whether it is one
 * flat field, and where the colour sits. A blank picture and a broken picture
 * look the same to a "did it throw?" test, so count pixels instead.
 *
 * The three channels are reported apart as well as together. Overall
 * brightness is nearly blind to a change of palette — a picture that goes
 * redder and bluer in equal measure has the same mean as before — so a test
 * that watches only `mean` can miss the very thing it is there to catch. */
const INSPECT = `(() => {
  const c = document.getElementById('canvas');
  const ctx = c.getContext('2d');
  const d = ctx.getImageData(0, 0, c.width, c.height).data;
  const seen = new Set();
  let r = 0, g = 0, b = 0, n = 0;
  for (let i = 0; i < d.length; i += 4 * 97) {
    seen.add((d[i] >> 3) + ',' + (d[i + 1] >> 3) + ',' + (d[i + 2] >> 3));
    r += d[i]; g += d[i + 1]; b += d[i + 2]; n++;
  }
  return { colours: seen.size, mean: (r + g + b) / (n * 3),
           r: r / n, g: g / n, b: b / n, w: c.width, h: c.height };
})()`;

/* How far apart two readings are as colour, rather than as brightness. */
function colourGap(a, b) {
  return Math.max(Math.abs(a.r - b.r), Math.abs(a.g - b.g), Math.abs(a.b - b.b));
}

/* Read the canvas only once it has stopped changing.
 *
 * The painted counter says a render finished, but a render can be followed by
 * another one the page started itself (a control that repaints on change), and
 * reading between the two gives the previous picture. Two identical reads in a
 * row means the canvas is at rest. */
async function settled(page, inspect) {
  let last = null;
  for (let i = 0; i < 40; i++) {
    const now = await page.evaluate(inspect);
    if (last && now.colours === last.colours && Math.abs(now.mean - last.mean) < 1e-9) {
      return now;
    }
    last = now;
    await page.waitForTimeout(120);
  }
  return last;
}

/* Wait for one more finished picture than there were before. The page counts
 * them on the canvas itself, so this can never pass on a paint that has been
 * scheduled but not yet run. */
async function painted(page, before, timeout) {
  await page.waitForFunction(
    (n) => Number(document.getElementById('canvas').dataset.painted || 0) > n,
    before, { timeout: timeout || 30000 }
  );
  return page.evaluate(() => Number(document.getElementById('canvas').dataset.painted || 0));
}

let paintsThisLoad = 0;

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 1100, height: 900 } });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

  try {
    await page.goto(`http://127.0.0.1:${PORT}/coda-pics/`, { waitUntil: 'load' });
    await page.waitForTimeout(300);
    check(problems.length === 0, 'the page loads clean' + (problems.length ? ' — ' + problems.join('; ') : ''));

    /* --------------------------------------------------------- painting */
    await page.fill('#prompt', 'a red dragon over snowy mountains at sunset');
    const started = Date.now();
    await page.click('#paint');
    let count = await painted(page, 0);
    const took = Date.now() - started;

    const first = await page.evaluate(INSPECT);
    check(first.w === 1280 && first.h === 720, `painted at the chosen size (got ${first.w}×${first.h})`);
    check(first.colours > 24, `the picture has real detail in it (${first.colours} distinct colours)`);
    check(first.mean > 12 && first.mean < 246, 'the picture is neither black nor white all over');
    check(took < 15000, `a picture takes a sensible time (${took}ms)`);

    /* Both overlays must really be gone: `hidden` on a flex element is undone
     * by an author display rule, which is exactly the kind of bug a
     * pixels-only test sails straight past. */
    check(!(await page.isVisible('#placeholder')), 'the "type something" note gets out of the way');
    check(!(await page.isVisible('#busy')), 'the "painting…" note gets out of the way');
    check(await page.isVisible('#canvas'), 'the picture itself is on screen');

    const status = await page.textContent('#status');
    check(/dragon/i.test(status) && /seed/i.test(status), 'it says what it drew and with which seed');
    const tags = await page.$$eval('.readout .tag', (els) => els.map((e) => e.textContent));
    check(tags.length >= 4, `it shows what each word was understood as (${tags.length} tags)`);
    check(tags.join(' ').indexOf('dragon') >= 0, 'the subject appears in what it understood');

    /* ------------------------------------------------- the same, again */
    await page.click('#paint');
    count = await painted(page, count);
    const same = await page.evaluate(INSPECT);
    check(same.colours === first.colours && Math.abs(same.mean - first.mean) < 0.001,
      'painting the same prompt twice gives the same picture');

    /* ------------------------------------------------------ another take */
    await page.click('#reroll');
    count = await painted(page, count);
    const rerolled = await page.evaluate(INSPECT);
    check(Math.abs(rerolled.mean - first.mean) > 0.0001, 'another take really paints something else');

    /* ------------------------------------------------------ the controls */
    await page.selectOption('#style', 'noir');
    count = await painted(page, count);
    const noir = await page.evaluate(`(() => {
      const c = document.getElementById('canvas');
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      let coloured = 0, n = 0;
      for (let i = 0; i < d.length; i += 4 * 97) {
        if (Math.abs(d[i] - d[i + 1]) > 8 || Math.abs(d[i + 1] - d[i + 2]) > 8) coloured++;
        n++;
      }
      return coloured / n;
    })()`);
    check(noir < 0.02, `choosing film noir really drains the colour (${(noir * 100).toFixed(1)}% left)`);

    await page.selectOption('#shape', 'tall');
    count = await painted(page, count);
    const tall = await page.evaluate(INSPECT);
    check(tall.h > tall.w, `choosing a tall shape repaints tall (${tall.w}×${tall.h})`);

    /* ---------------------------------------------------------- gallery */
    await page.click('#keep');
    await page.waitForTimeout(400);
    const cards = await page.$$('.gallery .card');
    check(cards.length === 1, `keeping a picture puts it in the gallery (${cards.length})`);
    const thumbInk = await page.evaluate(`(() => {
      const c = document.querySelector('.gallery .card canvas');
      if (!c) return -1;
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      const seen = new Set();
      for (let i = 0; i < d.length; i += 4 * 31) seen.add((d[i] >> 3) + ',' + (d[i + 1] >> 3));
      return seen.size;
    })()`);
    check(thumbInk > 8, `the kept picture repaints as a real thumbnail (${thumbInk} tones)`);

    await page.fill('#prompt', 'something else entirely');
    await page.click('.gallery .card canvas');
    count = await painted(page, count);
    const restored = await page.inputValue('#prompt');
    check(/dragon/i.test(restored), 'tapping a kept picture brings its words back');

    /* --------------------------------------------------------- download */
    const [download] = await Promise.all([
      page.waitForEvent('download', { timeout: 15000 }).catch(() => null),
      page.click('#download')
    ]);
    check(!!download, 'saving the picture really produces a file');
    if (download) {
      check(/^coda-pics-.*\.png$/.test(download.suggestedFilename()),
        'the saved file is a sensibly named .png (' + download.suggestedFilename() + ')');
    }

    /* ------------------------------------------------------- surprise me */
    await page.click('#surprise');
    count = await painted(page, count);
    const surprised = await page.inputValue('#prompt');
    check(surprised.length > 4, 'surprise me writes a prompt and paints it');
    const after = await page.evaluate(INSPECT);
    check(after.colours > 12, 'the surprise is a real picture');

    /* --------------------------------------------- words it does not know */
    await page.fill('#prompt', 'a griffin in a pine forest');
    await page.click('#paint');
    count = await painted(page, count);
    check(await page.isVisible('#unknown'), 'it admits when a word meant nothing to it');
    const unknownText = await page.textContent('#unknown');
    check(/griffin/i.test(unknownText), `it names the word it did not know (${unknownText})`);

    await page.fill('#prompt', 'a fox in a pine forest');
    await page.click('#paint');
    count = await painted(page, count);
    check(!(await page.isVisible('#unknown')), 'and says nothing when it understood everything');

    /* ------------------------------------------------------- six at once */
    await page.click('#six');
    await page.waitForTimeout(1200);
    const sheet = await page.$$('#sheetGrid .card canvas');
    check(sheet.length === 6, `six takes really renders six (${sheet.length})`);

    /* ----------------------------------------------------- a link to it */
    const link = await page.evaluate(`(() => {
      document.getElementById('share').click();
      return location.origin + location.pathname;
    })()`);
    check(typeof link === 'string' && link.length > 0, 'the share button runs without throwing');

    /* The link has to actually reproduce the picture, which is the only
     * thing that makes it worth having. */
    await page.fill('#prompt', 'a whale under a huge moon');
    await page.selectOption('#style', 'noir');
    await page.click('#paint');
    count = await painted(page, count);
    const before = await page.evaluate(INSPECT);
    const shared = await page.evaluate(`(() => {
      const c = document.getElementById('canvas');
      return { href: location.href, painted: c.dataset.painted };
    })()`);
    void shared;
    const url = await page.evaluate(`(() => {
      const s = document.getElementById('status').textContent || '';
      const m = s.match(/seed (\\d+)/);
      return location.origin + location.pathname + '#p=' +
        encodeURIComponent('a whale under a huge moon') + '&s=' + (m ? m[1] : '1') +
        '&y=noir&z=' + document.getElementById('shape').value;
    })()`);
    const page2 = await browser.newPage({ viewport: { width: 1100, height: 900 } });
    await page2.goto(url, { waitUntil: 'load' });
    await page2.waitForFunction(
      () => Number(document.getElementById('canvas').dataset.painted || 0) > 0,
      null, { timeout: 30000 }
    );
    const reopened = await page2.evaluate(INSPECT);
    check(reopened.colours === before.colours && Math.abs(reopened.mean - before.mean) < 0.001,
      'opening the shared link paints exactly the same picture');
    await page2.close();

    /* --------------------------------------- a kept picture cannot drift */
    await page.click('#keep');
    await page.waitForTimeout(300);
    const storedScene = await page.evaluate(`(() => {
      try {
        const kept = JSON.parse(localStorage.getItem('codaPics.gallery.v2') || '[]');
        return !!(kept[0] && kept[0].spec && kept[0].spec.scene && kept[0].spec.style);
      } catch (e) { return false; }
    })()`);
    check(storedScene,
      'the gallery stores the finished scene, not just the words that made it');

    /* ------------------------------------------------ one of your own photos
     * The fixture is a generated landscape, not anyone's photograph — see
     * tests/fixtures/README.md. It has a clear ridge and a sun to the right,
     * so every branch here has something real to read. */
    await page.setInputFiles('#photoFile', path.join(__dirname, 'fixtures', 'landscape.png'));
    await page.waitForSelector('#photoInfo:not([hidden])', { timeout: 15000 });
    check(true, 'a chosen photo is read and shown back');

    const photoNote = await page.textContent('#photoNote');
    check(/horizon found/i.test(photoNote), `it reports what it found (${photoNote})`);
    check(!(await page.isDisabled('#usePhotoSkyline')),
      'a photo with a clear horizon may lend it');

    await page.fill('#prompt', 'a dragon over the mountains');
    await page.selectOption('#style', 'poster');
    await page.click('#paint');
    count = await painted(page, count);
    const withPhoto = await settled(page, INSPECT);

    /* Its colours have to actually change the picture, or the checkbox lies. */
    await page.uncheck('#usePhotoColours');
    count = await painted(page, count);
    const withoutPhoto = await settled(page, INSPECT);
    /* Judged on colour, not on brightness. This check once read only the mean
     * of all three channels and failed in CI at "66.8 vs 66.9" — the picture
     * had changed colour considerably, and the brightness it gave up in one
     * channel it had taken back in another. */
    check(colourGap(withPhoto, withoutPhoto) > 2,
      `painting in the photo's colours really changes the picture ` +
      `(rgb ${withPhoto.r.toFixed(1)}/${withPhoto.g.toFixed(1)}/${withPhoto.b.toFixed(1)} ` +
      `vs ${withoutPhoto.r.toFixed(1)}/${withoutPhoto.g.toFixed(1)}/${withoutPhoto.b.toFixed(1)}, ` +
      `gap ${colourGap(withPhoto, withoutPhoto).toFixed(1)})`);
    await page.check('#usePhotoColours');
    count = await painted(page, count);

    await page.check('#usePhotoSkyline');
    count = await painted(page, count);
    check(true, 'its horizon can be used without throwing');

    await page.check('#usePhotoBackdrop');
    count = await painted(page, count);
    const onPhoto = await page.evaluate(INSPECT);
    check(onPhoto.colours > 12, 'a subject can be painted onto the photo itself');

    /* A picture painted onto a photo must not be kept, because the gallery
     * stores scenes and not photographs — and saying so beats bringing it
     * back wrong later. */
    await page.click('#keep');
    await page.waitForTimeout(250);
    const keepMsg = await page.textContent('#status');
    check(/gallery stores scenes/i.test(keepMsg),
      'and the gallery says why it cannot keep that one');
    await page.uncheck('#usePhotoBackdrop');
    count = await painted(page, count);

    /* The photo through the app's own styles, with nothing drawn on top. */
    await page.selectOption('#style', 'ukiyo');
    count = await painted(page, count);
    await page.waitForTimeout(300);
    await page.click('#stylePhoto');
    await page.waitForFunction(
      () => /Your photo, in/.test(document.getElementById('status').textContent || ''),
      null, { timeout: 20000 }
    );
    const styled = await page.evaluate(INSPECT);
    check(styled.colours > 8, 'the photo itself can be put through a style');
    const styledAlt = await page.getAttribute('#canvas', 'aria-label');
    check(/your photograph/i.test(styledAlt), `and says so for a screen reader (${styledAlt})`);

    await page.click('#clearPhoto');
    await page.waitForTimeout(200);
    check(!(await page.isVisible('#photoInfo')), 'the photo can be forgotten again');

    /* ------------------------------------------- a mixture of several photos
     * Three photos in three obviously different colour worlds, so a blend of
     * them is visibly not any one of them. */
    await page.setInputFiles('#photoFile', [
      path.join(__dirname, 'fixtures', 'landscape.png'),
      path.join(__dirname, 'fixtures', 'seafront.png'),
      path.join(__dirname, 'fixtures', 'forest.png')
    ]);
    await page.waitForSelector('#paletteBox:not([hidden])', { timeout: 20000 });
    const chips = await page.$$eval('.swatch span', (els) => els.map((e) => e.textContent));
    check(chips.length >= 4, `every photo leaves a palette behind (${chips.join(', ')})`);
    check(chips.some((c) => /just these 3/i.test(c)), 'and the three of them make a mixture');
    /* The accumulating memory: four photos have been through this page (one
     * earlier, three now) and the look must say so, not say three. */
    check(chips.some((c) => /your look . 4 photos/i.test(c)),
      `the look counts every photo ever shown, not just this batch (${chips.join(', ')})`);

    await page.fill('#prompt', 'a stag in a meadow at dawn');
    await page.selectOption('#style', 'poster');
    await page.click('#paint');
    count = await painted(page, count);

    async function paintIn(label) {
      const before = count;
      await page.click(`.swatch:has-text("${label}")`);
      count = await painted(page, before);
      return settled(page, INSPECT);
    }
    const mixture = await paintIn('Just these 3');
    const seafront = await paintIn('seafront');
    const forest = await paintIn('forest');
    check(Math.abs(mixture.mean - seafront.mean) > 0.5 &&
          Math.abs(mixture.mean - forest.mean) > 0.5,
      'the mixture paints as none of the photos it came from');
    check(Math.abs(seafront.mean - forest.mean) > 0.5,
      'and each photo paints as itself');

    /* The reload below starts the page again, so the running tally of pictures
     * is taken before it rather than after. */
    paintsThisLoad = count;

    /* The colours outlive the photos, which is the whole point of keeping
     * numbers rather than pixels. */
    await page.reload({ waitUntil: 'load' });
    await page.waitForSelector('#paletteBox:not([hidden])', { timeout: 15000 });
    const afterReload = await page.$$eval('.swatch span', (els) => els.map((e) => e.textContent));
    check(afterReload.some((c) => /just these 3/i.test(c)),
      'the kept colours come back after a reload, with no photos stored');
    check(afterReload.some((c) => /your look . 4 photos/i.test(c)),
      'and the look survives the page closing — that is what makes it cumulative');

    /* One more photo after the reload must move the count on rather than
     * restart it. A memory that resets when the tab closes is not a memory. */
    await page.setInputFiles('#photoFile', path.join(__dirname, 'fixtures', 'seafront.png'));
    await page.waitForFunction(
      () => [...document.querySelectorAll('.swatch span')]
        .some((e) => /your look . 5 photos/i.test(e.textContent)),
      null, { timeout: 20000 });
    check(true, 'a photo added in a later session keeps counting up (5)');

    await page.click('#clearPalettes');
    await page.waitForTimeout(250);
    check(!(await page.isVisible('#paletteBox')), 'and they can all be forgotten');
    check(await page.evaluate(() => !window.localStorage.getItem('codaPics.look.v1') ||
      window.localStorage.getItem('codaPics.look.v1') === 'null'),
      'forgetting means the accumulated look goes too, not just the named ones');
    count = await page.evaluate(
      () => Number(document.getElementById('canvas').dataset.painted || 0)
    );

    /* ------------------------------------------------- installable as an app
     * The manifest and its icons are what let someone add CODA PICS to a home
     * screen. They are easy to break by renaming a file and never notice,
     * because the page itself keeps working. */
    const manifestHref = await page.getAttribute('link[rel="manifest"]', 'href');
    check(manifestHref === 'manifest.webmanifest', `the page links its manifest (got ${manifestHref})`);

    const mres = await page.request.get(`http://127.0.0.1:${PORT}/coda-pics/manifest.webmanifest`);
    check(mres.ok(), 'the manifest is served');
    let manifest = null;
    try { manifest = JSON.parse(await mres.text()); } catch (e) { /* reported below */ }
    check(!!manifest, 'the manifest is valid JSON');
    if (manifest) {
      check(manifest.name === 'CODA PICS', `the manifest names the app (got ${manifest.name})`);
      check(manifest.display === 'standalone', 'it opens as an app, not a browser tab');
      check(!!manifest.start_url && !!manifest.scope, 'it has a start url and a scope');
      check(Array.isArray(manifest.icons) && manifest.icons.length >= 2,
        `it declares icons (${manifest.icons ? manifest.icons.length : 0})`);
      check(manifest.icons.some((i) => String(i.purpose).includes('maskable')),
        'one icon is maskable, so Android does not frame it in a white box');

      const dead = [];
      for (const icon of manifest.icons) {
        const r = await page.request.get(`http://127.0.0.1:${PORT}/coda-pics/${icon.src}`);
        if (!r.ok()) dead.push(icon.src);
      }
      check(dead.length === 0, 'every icon the manifest names exists' + (dead.length ? ' — missing: ' + dead.join(', ') : ''));
    }

    const apple = await page.request.get(`http://127.0.0.1:${PORT}/coda-pics/icons/apple-touch-icon.png`);
    check(apple.ok(), 'the iOS home-screen icon exists');

    const sw = await page.request.get(`http://127.0.0.1:${PORT}/coda-pics/sw.js`);
    check(sw.ok(), 'the offline worker is served');

    check(paintsThisLoad >= 7,
      `every button and control painted a fresh picture (${paintsThisLoad} in all)`);
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
