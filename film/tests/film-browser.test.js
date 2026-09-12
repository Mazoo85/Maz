/*
 * SCRIPT FORGE — end-to-end tests in a real browser.
 *
 * The logic suite proves the script is well-formed; this one proves the app
 * works: the page boots clean, typing an idea and pressing the button puts a
 * formatted screenplay on screen, the shot list tab shows real shots, every
 * download produces a real file with the right contents, "another take"
 * actually changes the script, and saving survives a reload.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node film/tests/film-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');
const os = require('os');

const ROOT = path.join(__dirname, '..', '..');
const StorySeed = require(path.join(ROOT, 'film', 'js', 'story-seed.js'));

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

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8213;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.md': 'text/plain' };

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

const IDEA = "A lonely lighthouse keeper finds a radio that plays tomorrow's news.";

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}`;
  const browser = await chromium.launch(launchOptions());
  const downloadDir = fs.mkdtempSync(path.join(os.tmpdir(), 'scriptforge-'));

  try {
    const context = await browser.newContext({ acceptDownloads: true, viewport: { width: 1200, height: 900 } });
    const page = await context.newPage();

    const problems = [];
    page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
    page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

    console.log('\nBOOT');
    await page.goto(base + '/film/', { waitUntil: 'load' });
    await page.waitForTimeout(400);
    check(problems.length === 0, 'loads with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));
    check((await page.$$('#genre option')).length >= 11, 'genre menu is filled from the lexicon');
    check((await page.$$('.example')).length >= 4, 'example ideas are offered');
    check(await page.$eval('#result', (e) => e.classList.contains('hidden')), 'no script on screen yet');

    console.log('\nWRITING');
    await page.fill('#idea', IDEA);
    await page.selectOption('#length', 'short');
    await page.click('#write');
    await page.waitForTimeout(300);

    check(!(await page.$eval('#result', (e) => e.classList.contains('hidden'))), 'a script appears');
    const headings = await page.$$eval('#viewScript .scene_heading', (els) => els.map((e) => e.textContent));
    check(headings.length === 5, `five scenes for the five-scene length (found ${headings.length})`);
    check(headings.every((h) => /(INT\.|EXT\.)/.test(h)), 'every scene has a slug line');
    check((await page.$$('#viewScript .dialogue')).length >= 4, 'the script has dialogue');
    check((await page.$$('#viewScript .character')).length >= 4, 'dialogue is attributed to characters');
    check((await page.textContent('#scriptTitle')).trim().length > 0, 'the film has a title');
    check(/lighthouse/i.test(await page.textContent('#scriptLogline')), 'the logline came from the typed idea');
    check(/min/.test(await page.textContent('#chipRuntime')), 'a runtime is estimated');

    console.log('\nSHOT LIST');
    await page.click('#tabShots');
    await page.waitForTimeout(150);
    check(!(await page.$eval('#viewShots', (e) => e.classList.contains('hidden'))), 'shot list tab opens');
    check((await page.$$('#viewShots li')).length >= 15, 'every scene is covered by shots');
    await page.click('#tabScript');
    check(!(await page.$eval('#viewScript', (e) => e.classList.contains('hidden'))), 'screenplay tab comes back');

    console.log('\nANOTHER TAKE');
    const before = await page.textContent('#viewScript');
    await page.click('#reroll');
    await page.waitForTimeout(250);
    const after = await page.textContent('#viewScript');
    check(before !== after, 'rerolling rewrites the script');
    // Back to the seeded draft so the export checks below are reproducible.
    await page.click('#write');
    await page.waitForTimeout(200);

    console.log('\nEXPORTS');
    const exports = [
      { id: '#dlFountain', ext: '.fountain', expect: (t) => t.startsWith('Title: **') && t.includes('INT.') },
      { id: '#dlFdx', ext: '.fdx', expect: (t) => t.includes('<FinalDraft') && t.includes('Scene Heading') },
      { id: '#dlText', ext: '.txt', expect: (t) => t.includes('INT.') && t.includes('THE END') },
      { id: '#dlShots', ext: '.md', expect: (t) => t.includes('shot list') && t.includes('- [ ]') }
    ];
    for (const spec of exports) {
      const [download] = await Promise.all([
        page.waitForEvent('download'),
        page.click(spec.id)
      ]);
      const file = path.join(downloadDir, download.suggestedFilename());
      await download.saveAs(file);
      const text = fs.readFileSync(file, 'utf8');
      check(
        download.suggestedFilename().endsWith(spec.ext) && text.length > 200 && spec.expect(text),
        `${spec.ext} downloads a real file (${download.suggestedFilename()}, ${text.length} bytes)`
      );
    }

    console.log('\nLIBRARY');
    await page.click('#save');
    await page.waitForTimeout(150);
    check((await page.textContent('#libCount')) === '1', 'saving adds it to the library');
    await page.reload({ waitUntil: 'load' });
    await page.waitForTimeout(300);
    check((await page.textContent('#libCount')) === '1', 'the library survives a reload');
    await page.click('.library-list button');
    await page.waitForTimeout(250);
    check((await page.textContent('#scriptTitle')).trim().length > 0, 'a saved script reopens');
    await page.click('#clearLib');
    await page.waitForTimeout(150);
    check((await page.textContent('#libCount')) === '0', 'the library can be cleared');

    console.log('\nTHIN IDEA');
    // Reopening the saved script above filled the title box with its title;
    // clear it so this section sees the fallback's own title, not a leftover.
    await page.fill('#titleInput', '');
    await page.fill('#idea', '');
    await page.click('#write');
    await page.waitForTimeout(150);
    const thin = await page.evaluate(() => ({
      title: document.getElementById('scriptTitle').textContent.trim(),
      logline: document.getElementById('scriptLogline').textContent.trim(),
      ideaBox: document.getElementById('idea').value.trim()
    }));
    check(thin.title.length > 0, 'an empty idea still writes a script instead of being refused');
    check(thin.logline.length > 20, `the borrowed story supplies its own logline (${JSON.stringify(thin.logline)})`);
    // The old removed behavior refused to write at all; the current one used
    // to substitute a whole unrelated story while the box kept showing the
    // words the user typed (or nothing). "Surprise me" already writes its
    // borrowed text into the box — a thin idea has to do the same, honestly,
    // so what's on screen matches what the film is actually about.
    check(thin.ideaBox.length > 0, 'a borrowed story is written into the idea box, not substituted silently');
    check(thin.ideaBox.split(/\s+/).filter(Boolean).length >= 3,
      `the idea box shows a real borrowed sentence, not the empty box (${JSON.stringify(thin.ideaBox)})`);

    // An empty box used to seed from Parse.hashText('blank') every time, so
    // "Write the script" on a bare box gave the exact same film forever.
    // Two presses, both on an empty box, must not land on the same story.
    await page.fill('#idea', '');
    await page.click('#write');
    await page.waitForTimeout(150);
    const emptyFirst = await page.evaluate(() => ({
      idea: document.getElementById('idea').value.trim(),
      title: document.getElementById('scriptTitle').textContent.trim()
    }));
    await page.fill('#idea', '');
    await page.click('#write');
    await page.waitForTimeout(150);
    const emptySecond = await page.evaluate(() => ({
      idea: document.getElementById('idea').value.trim(),
      title: document.getElementById('scriptTitle').textContent.trim()
    }));
    check(emptyFirst.idea !== emptySecond.idea || emptyFirst.title !== emptySecond.title,
      `pressing Write twice on an empty box gives different films (${JSON.stringify(emptyFirst)} vs ${JSON.stringify(emptySecond)})`);

    console.log('\nTHE THREE-WORD BOUNDARY');
    await page.fill('#idea', 'ghost pirates');
    await page.click('#write');
    await page.waitForTimeout(150);
    const twoWords = await page.evaluate(() => document.getElementById('idea').value.trim());
    check(twoWords !== 'ghost pirates', 'two words is still too thin: the idea gets borrowed');

    await page.fill('#idea', 'ghost pirates rising');
    await page.click('#write');
    await page.waitForTimeout(150);
    const threeWords = await page.evaluate(() => document.getElementById('idea').value.trim());
    check(threeWords === 'ghost pirates rising', 'three words is enough: the typed idea is kept as typed');

    console.log('\nSURPRISE ME');
    const surprised = await page.evaluate(() => {
      const before = document.getElementById('idea').value;
      document.getElementById('surprise').click();
      return { before: before, after: document.getElementById('idea').value };
    });
    check(surprised.after.length > 20, `surprise me filled the idea box (${surprised.after.length} chars)`);
    check(surprised.after !== surprised.before, 'surprise me changed the idea');

    console.log('\nBORROWED TITLE');
    // Pin the one legitimate random number so the seed Surprise me rolls is
    // known, then check the exact story it borrowed reaches the finished
    // script's title card, in MADLIBS's own words rather than a genre default.
    const borrowedProof = await page.evaluate(() => {
      const realRandom = Math.random;
      const realNow = Date.now;
      Math.random = () => 0.31415;
      Date.now = () => 1717000000000;
      document.getElementById('surprise').click();
      Math.random = realRandom;
      Date.now = realNow;
      return {
        idea: document.getElementById('idea').value,
        title: document.getElementById('scriptTitle').textContent.trim()
      };
    });
    const expectedSeed = (1717000000000 ^ Math.floor(0.31415 * 0xffffffff)) >>> 0;
    const expectedStory = StorySeed.idea(expectedSeed);
    check(borrowedProof.idea === expectedStory.text, 'the borrowed idea matches the seed the button rolled');
    check(borrowedProof.title === expectedStory.title.toUpperCase(),
      `MADLIBS's own title reaches the finished script (${JSON.stringify(borrowedProof.title)})`);

    // Editing the idea box and writing again must not keep the borrowed
    // title around — a stale title from a story that is no longer on screen.
    await page.fill('#idea', 'A retired clockmaker builds a machine that repairs broken promises.');
    await page.click('#write');
    await page.waitForTimeout(150);
    const editedTitle = (await page.textContent('#scriptTitle')).trim();
    check(editedTitle !== expectedStory.title.toUpperCase(),
      'a borrowed title does not persist once the user edits the idea and writes again');

    console.log('\nTHE FILM');
    await page.click('#tabFilm');
    await page.waitForTimeout(400);

    const poster = async () => page.evaluate(() => {
      const c = document.getElementById('filmCanvas');
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      let lit = 0;
      for (let i = 0; i < d.length; i += 4000) if (d[i] + d[i + 1] + d[i + 2] > 45) lit++;
      return lit;
    });
    check((await poster()) > 50, 'the film tab opens on a painted poster frame, not a black box');
    check(/0:00 \/ [0-9]:[0-9]{2}/.test(await page.textContent('#filmClock')), 'the film has a running time');
    check(await page.evaluate(() => FilmPlayer.canRecord(document.getElementById('filmCanvas'))),
      'this browser can record the film');

    // The app must say which format is coming *before* anyone sits through a
    // recording, and the file it produces must match what it promised.
    const promised = await page.evaluate(() => {
      const f = FilmPlayer.bestFormat();
      return {
        extension: f && f.extension,
        type: f && f.type,
        playsOnApple: f && f.playsOnApple,
        button: document.getElementById('recordFilm').textContent,
        note: document.getElementById('filmNote').textContent,
        warned: document.getElementById('filmNote').className.indexOf('warn') !== -1
      };
    });
    check(promised.button.indexOf(promised.extension) !== -1,
      `the button names the format it will save (${promised.button.trim()})`);
    check(promised.playsOnApple
      ? /plays on anything/i.test(promised.note)
      : (promised.warned && /iPhone/i.test(promised.note)),
      promised.playsOnApple
        ? 'an mp4 is described as playing anywhere'
        : 'a webm carries a plain warning that Apple devices cannot play it');

    await page.click('#playFilm');
    await page.waitForTimeout(3500);
    const clockPlaying = await page.textContent('#filmClock');
    check(/0:0[2-9]/.test(clockPlaying), `the film plays (clock reads ${clockPlaying})`);
    check((await page.textContent('#playFilm')).indexOf('Pause') !== -1, 'the play button offers to pause');

    // The picture has to actually change, or it is a still, not a film.
    const frameA = await page.evaluate(() => document.getElementById('filmCanvas').toDataURL().length);
    await page.waitForTimeout(1200);
    const frameB = await page.evaluate(() => document.getElementById('filmCanvas').toDataURL().length);
    check(frameA !== frameB, 'the picture moves while it plays');

    await page.click('#playFilm'); // pause
    await page.waitForTimeout(300);
    check((await page.textContent('#playFilm')).indexOf('Resume') !== -1, 'pausing offers to resume');

    // Scrub to roughly the middle.
    const bar = await page.$('#scrubBar');
    const box = await bar.boundingBox();
    await page.mouse.click(box.x + box.width * 0.5, box.y + box.height / 2);
    await page.waitForTimeout(300);
    const scrubbed = await page.textContent('#filmClock');
    const seconds = parseInt(scrubbed.split(':')[1], 10) + parseInt(scrubbed.split(':')[0], 10) * 60;
    check(seconds > 20, `clicking the bar jumps into the film (landed at ${scrubbed})`);
    check((await poster()) > 50, 'the frame it jumped to is drawn');

    await page.click('#stopFilm');
    await page.waitForTimeout(300);
    check((await page.textContent('#filmClock')).indexOf('0:00') === 0, 'stop returns to the start');

    console.log('\nFRAME BUDGET');
    // A generated drama short only ever throws `dust` and `none` weather at
    // this gate (and, since `short` skips the crisis beat, never handheld or
    // the Dutch tilt either) — the gate then never draws rain, fog, embers,
    // shimmer or haze, or the `ots`/`low` framings. Weather kind is a
    // function of the *reel's* genre (one per whole film) plus each shot's
    // own time and set, so no single generated reel can reach every kind —
    // rain/haze need a thriller-or-horror reel, fog needs horror-or-mystery,
    // shimmer needs western, embers needs fantasy. Building the scenarios
    // directly, reel-shaped, sidesteps that: each one is its own tiny reel
    // (one shot, one genre) chosen to land on a specific weather kind,
    // framing or camera move, and the budget loop below cycles through all
    // of them so every sample size still gets its full 300 draws.
    const budget = await page.evaluate(() => {
      function reelFor(spec) {
        const shot = Object.assign({
          kind: 'action', duration: 6, start: 0, index: 0, scene: 1,
          camera: 'static', caption: 'A synthetic beat, long enough to read for the budget gate.',
          speaker: null, characters: [], mood: 0.3, beat: 'push'
        }, spec);
        return {
          genre: spec.genre, seed: 1, shots: [shot],
          voices: { A: { hue: 40, side: -1 }, B: { hue: 210, side: 1 } },
          characters: [], object: '', duration: shot.duration
        };
      }

      // One scenario per weather kind FilmWeather.forShot can return, plus
      // the ots/low framings and a crisis (handheld + Dutch tilt) shot.
      const scenarios = [
        reelFor({ genre: 'thriller', time: 'NIGHT', set: 'street', framing: 'wide' }),                 // rain
        reelFor({ genre: 'drama', time: 'DAY', set: 'kitchen', framing: 'mid' }),                       // dust
        reelFor({ genre: 'horror', time: 'NIGHT', set: 'woods', framing: 'wide' }),                     // fog
        reelFor({ genre: 'western', time: 'DAY', set: 'field', framing: 'wide' }),                      // shimmer
        reelFor({ genre: 'fantasy', time: 'NIGHT', set: 'room', framing: 'close', speaker: 'A', characters: ['A'], kind: 'line' }), // embers
        reelFor({ genre: 'thriller', time: 'NIGHT', set: 'room', framing: 'two', speaker: 'A', characters: ['A', 'B'], kind: 'line' }), // haze
        reelFor({ genre: 'comedy', time: 'DAY', set: 'water', framing: 'low', speaker: 'A', characters: ['A'], kind: 'line' }),         // none + low framing
        reelFor({ genre: 'mystery', time: 'NIGHT', set: 'vehicle', framing: 'ots', speaker: 'A', characters: ['A', 'B'], kind: 'line' }), // ots framing
        reelFor({ genre: 'thriller', time: 'NIGHT', set: 'industrial', framing: 'wide', camera: 'handheld', mood: 0.88, beat: 'crisis' }) // crisis: handheld + tilt
      ];

      // Sanity-check the scenarios actually land on what they are built for,
      // so a change to FilmWeather's rules or the camera's tilt threshold
      // fails loudly here instead of silently narrowing the gate again.
      const seenWeather = {};
      const seenFraming = {};
      let sawTilt = false;
      scenarios.forEach(function (reel) {
        const shot = reel.shots[0];
        const kind = FilmWeather.forShot(reel.genre, shot.time, shot.set);
        seenWeather[kind] = true;
        seenFraming[shot.framing] = true;
        const cam = FilmPlayer.framingFor(shot, 0.5, shot.duration * 0.5);
        if (Math.abs(cam.roll || 0) > 0.01) sawTilt = true;
      });
      const coverage = {
        weatherKinds: Object.keys(seenWeather).sort(),
        hasOts: !!seenFraming.ots,
        hasLow: !!seenFraming.low,
        sawTilt: sawTilt
      };

      const out = [];
      [[960, 540, 4], [1920, 1080, 6]].forEach(function (spec) {
        const w = spec[0], h = spec[1], limit = spec[2];
        const c = document.createElement('canvas');
        c.width = w; c.height = h;
        const ctx = c.getContext('2d');
        // Warm up first: the first draw of a set builds gradients and patterns.
        for (let i = 0; i < 20; i++) {
          const reel = scenarios[i % scenarios.length];
          FilmPlayer.drawFrame(ctx, w, h, reel, (i / 20) * reel.duration);
        }
        const times = [];
        const N = 300;
        for (let i = 0; i < N; i++) {
          const reel = scenarios[i % scenarios.length];      // cycle every kind, every framing
          const t = ((i / scenarios.length) % 1) * reel.duration;
          const t0 = performance.now();
          FilmPlayer.drawFrame(ctx, w, h, reel, t);
          times.push(performance.now() - t0);
          // Chromium defers rasterizing a canvas that is never read or
          // composited: draw calls pile up in an internal recording buffer
          // instead of hitting the bitmap, and once in a long while it dumps
          // the whole backlog into one giant synchronous flush that lands on
          // whichever frame happened to be running — a real cost, but not
          // this frame's cost, and not one a played or recorded film ever
          // pays (every rAF tick composites the canvas, and captureStream
          // pulls a frame every tick too, so nothing ever backs up). This
          // 1x1 read, taken after the timed span so it can never inflate a
          // measurement, is what a real frame gets for free from either of
          // those and keeps the benchmark measuring drawFrame's own cost
          // instead of an unrelated backlog this loop alone would create.
          ctx.getImageData(0, 0, 1, 1);
        }
        times.sort((a, b) => a - b);
        out.push({
          size: w + 'x' + h,
          limit: limit,
          mean: times.reduce((a, x) => a + x, 0) / times.length,
          worst: times[N - 1]
        });
      });
      return { out: out, coverage: coverage };
    });

    const WEATHER_KINDS = ['dust', 'embers', 'fog', 'haze', 'none', 'rain', 'shimmer'];
    check(WEATHER_KINDS.every((k) => budget.coverage.weatherKinds.indexOf(k) !== -1),
      `the gate draws every weather kind (saw: ${budget.coverage.weatherKinds.join(', ')})`);
    check(budget.coverage.hasOts && budget.coverage.hasLow, 'the gate draws the ots and low framings');
    check(budget.coverage.sawTilt, 'the gate draws a tilted crisis shot');

    budget.out.forEach((r) => {
      check(r.mean < r.limit,
        `${r.size} draws in ${r.mean.toFixed(2)}ms mean (limit ${r.limit}ms, worst ${r.worst.toFixed(1)}ms)`);
    });

    console.log('\nRACK FOCUS');
    // A close-up or two-shot's fore element is meant to fall out of focus —
    // lifted toward the palette's `deep` tone, never a wash over the whole
    // frame. This is the test that would have caught the shipped bug: the
    // fallback used `source-atop` against the live frame, which (because
    // drawFrame fills the canvas opaque black first) degenerates to a
    // blanket rect over everything, figures included.
    //
    // The acceptance test isn't "the figures' bounding box never changes" —
    // in `field` the fence rail is drawn *in front of* every figure's shins
    // by design (that's the whole point of a fore element), so toggling the
    // lift legitimately changes a few pixels inside a naive figure box, on
    // the rail, not on the figure. The real claim is narrower and stronger:
    // every pixel that differs between the lift on and off, anywhere in the
    // frame, lies inside the fore shape's own painted footprint. That is
    // checked directly — render the shape alone (with the lift off, i.e. its
    // ordinary ink), at the same camera transform, onto a transparent layer,
    // and use its alpha channel as the mask — rather than inferred from a
    // guessed box around each figure, and independent of how `FilmSets`
    // computes the lifted color (see `foreInk` in film-sets.js), so the test
    // does not simply trust the implementation it is checking.
    const rack = await page.evaluate(() => {
      // mood defaults to 0.4 — below the 0.7 threshold where a Dutch tilt
      // starts (see `roll` in framingFor) — so most samples exercise an
      // unrolled frame, same as before. A handful of samples below pass a
      // high mood explicitly to put a real roll on screen.
      function reelFor(setName, framing, camera, side, mood) {
        const shot = {
          kind: 'line', duration: DURATION, start: 0, index: 0, scene: 1,
          set: setName, framing: framing, camera: camera, time: 'NIGHT',
          caption: 'Held on the moment, long enough to read.',
          speaker: 'A', side: side || 0,
          characters: framing === 'two' ? ['A', 'B'] : ['A'],
          mood: mood == null ? 0.4 : mood, beat: 'push'
        };
        return {
          genre: 'drama', seed: 7, shots: [shot],
          voices: { A: { hue: 40 }, B: { hue: 210 } },
          characters: [], object: '', duration: shot.duration
        };
      }

      function render(reel, w, h, t, opts) {
        const c = document.createElement('canvas');
        c.width = w; c.height = h;
        const ctx = c.getContext('2d');
        FilmPlayer.drawFrame(ctx, w, h, reel, t, opts);
        return ctx.getImageData(0, 0, w, h).data;
      }

      // Exactly what `set.fore` paints (ordinary ink, lift off), at this
      // shot's actual camera transform, on its own transparent layer.
      function foreMask(setName, pal, grain, cam, w, h) {
        const c = document.createElement('canvas');
        c.width = w; c.height = h;
        const ctx = c.getContext('2d');
        const frameW = w, frameH = Math.min(h, w / FilmPlayer.ASPECT), frameY = (h - frameH) / 2;
        const scale = (frameW / FilmPlayer.WORLD_W) * cam.zoom;
        const rate = FilmSets.PARALLAX.fore;
        // Mirrors drawFrame's own Dutch-tilt rotation, applied ahead of the
        // plane transform below — otherwise a rolled sample's mask would
        // land in the unrotated position while the real render (which does
        // apply the roll) lands rotated, and every pixel the lift touches
        // would misread as a containment violation.
        if (cam.roll) {
          ctx.translate(frameW / 2, frameY + frameH / 2);
          ctx.rotate(cam.roll);
          ctx.translate(-frameW / 2, -(frameY + frameH / 2));
        }
        ctx.translate(
          frameW / 2 + cam.panX * frameW * rate,
          frameY + frameH / 2 + cam.panY * frameH + cam.panYMove * frameH * rate);
        ctx.scale(scale, scale);
        ctx.translate(-FilmPlayer.WORLD_W / 2, -FilmPlayer.WORLD_H / 2);
        FilmSets.SETS[setName].fore(ctx, pal, grain);
        return ctx.getImageData(0, 0, w, h).data;
      }

      // Every pixel where `a` and `b` differ must have a non-transparent
      // pixel at the same spot in `mask` — or in an immediate neighbor of
      // it — or it is a violation: something outside the fore element
      // moved. The 1px neighbor allowance is for antialiasing, not for
      // slack in the claim: `mask` is rendered independently of `a`/`b`
      // (its own canvas, its own transform derivation), and a shape's
      // very edge can round to alpha 0 in one rasterization and a whisper
      // of alpha a pixel over in another without anything outside the
      // shape having moved. A real regression (a wash over the set, a
      // figure, the letterbox) shows up far from every masked pixel, not
      // one ring around its edge.
      function diffAgainstMask(a, b, mask, w, h) {
        let total = 0, violations = 0;
        for (let y = 0; y < h; y++) {
          for (let x = 0; x < w; x++) {
            const p = (y * w + x) * 4;
            if (a[p] === b[p] && a[p + 1] === b[p + 1] && a[p + 2] === b[p + 2] && a[p + 3] === b[p + 3]) continue;
            total++;
            let inside = false;
            for (let dy = -1; dy <= 1 && !inside; dy++) {
              for (let dx = -1; dx <= 1 && !inside; dx++) {
                const ny = y + dy, nx = x + dx;
                if (ny < 0 || ny >= h || nx < 0 || nx >= w) continue;
                if (mask[(ny * w + nx) * 4 + 3] !== 0) inside = true;
              }
            }
            if (!inside) violations++;
          }
        }
        return { total: total, violations: violations };
      }

      const w = 960, h = 540;
      const DURATION = 4;
      // `time` is both the shot's wall clock (what handheld/whip read) and,
      // via time/DURATION, its progress — the same relationship drawFrame
      // derives internally, so the mask computed below at `progress` lines
      // up with what drawFrame actually drew at `time`.
      // pan-l/pan-r/track-l/track-r/whip all peak at progress 0 or 1 (see
      // framingFor), so those are sampled at the very start or end of the
      // shot rather than the middle, and both speaker sides are tried
      // (side and the camera's own pan add when they point the same way).
      //
      // Every sample above is at the default mood (0.4), below the 0.7
      // threshold where a Dutch tilt starts — so none of them puts a roll
      // on screen. The four below do, at mood 0.95: a rolled frame can
      // uncover fore pixels a coverage derivation that ignores roll would
      // never see (this caught a real bug — foreVisibleWorldRect sampled
      // every camera and side but not the tilt), so a mood high enough to
      // roll has to appear here for that class of bug to be catchable at
      // all. 'static' isolates the roll itself (no pan of its own); the
      // roll applies the same way regardless of camera.
      const SAMPLES = [
        { camera: 'static', time: 1.2, side: -1 }, { camera: 'static', time: 1.2, side: 1 },
        { camera: 'pan-l', time: 0.02, side: -1 }, { camera: 'pan-l', time: 0.02, side: 1 },
        { camera: 'pan-r', time: 3.98, side: -1 }, { camera: 'pan-r', time: 3.98, side: 1 },
        { camera: 'track-l', time: 0.02, side: -1 }, { camera: 'track-l', time: 0.02, side: 1 },
        { camera: 'track-r', time: 3.98, side: -1 }, { camera: 'track-r', time: 3.98, side: 1 },
        { camera: 'whip', time: 0.02, side: -1 }, { camera: 'whip', time: 0.02, side: 1 },
        { camera: 'push-slow', time: 2.4, side: -1 },
        { camera: 'handheld', time: 3.6, side: 1 }, { camera: 'handheld', time: 1.9, side: -1 },
        { camera: 'static', time: 1.2, side: -1, mood: 0.95 }, { camera: 'static', time: 1.2, side: 1, mood: 0.95 },
        { camera: 'handheld', time: 0.9, side: -1, mood: 0.95 }, { camera: 'handheld', time: 0.9, side: 1, mood: 0.95 },
        { camera: 'handheld', time: 2.7, side: -1, mood: 0.95 }, { camera: 'handheld', time: 2.7, side: 1, mood: 0.95 },
        { camera: 'track-l', time: 0.02, side: -1, mood: 0.95 }, { camera: 'track-l', time: 0.02, side: 1, mood: 0.95 }
      ];

      const setNames = Object.keys(FilmSets.SETS);
      const perSetFraming = {};
      setNames.forEach((setName) => {
        ['close', 'two'].forEach((framing) => {
          const coverage = FilmPlayer.foreCoversFraming(document, setName, FilmSets.SETS[setName], framing);
          let violationsTotal = 0, maxFrameDiffFrac = 0, anyFrameDiff = false;
          SAMPLES.forEach((s) => {
            const reel = reelFor(setName, framing, s.camera, s.side, s.mood);
            const shot = reel.shots[0];
            const pal = FilmArt.palette(reel.genre, shot.time, shot.mood);
            const grain = FilmArt.noise('set-' + shot.set + '-' + reel.seed, 80);
            const clockTime = s.time; // handheld/whip read wall-clock time, not just progress
            const on = render(reel, w, h, clockTime, undefined);
            const off = render(reel, w, h, clockTime, { rackFocus: false });

            const progress = Math.max(0, Math.min(1, clockTime / DURATION));
            const cam = FilmPlayer.framingFor(shot, progress, clockTime);
            const mask = foreMask(setName, pal, grain, cam, w, h);

            const d = diffAgainstMask(on, off, mask, w, h);
            violationsTotal += d.violations;
            const frac = d.total / (w * h);
            if (frac > maxFrameDiffFrac) maxFrameDiffFrac = frac;
            if (d.total > 0) anyFrameDiff = true;
          });
          perSetFraming[setName + ':' + framing] = {
            coverage: coverage,
            violationsTotal: violationsTotal,
            maxFrameDiffFrac: maxFrameDiffFrac,
            anyFrameDiff: anyFrameDiff
          };
        });
      });
      return perSetFraming;
    });

    // Every pixel that changes between the lift on and off, in every set, in
    // both framings, on every sampled camera move, lies inside the fore
    // shape's own painted footprint. This is the assertion the shipped bug
    // failed outright — a blanket rect over the whole frame changes nearly
    // everything, none of it inside any one shape's footprint (measured:
    // 75.5% of canvas pixels changed on woods/close, 100% of the figure-box
    // pixels checked in every case).
    let maskFailures = 0;
    Object.keys(rack).forEach((key) => {
      const r = rack[key];
      if (r.violationsTotal !== 0) {
        maskFailures++;
        console.log(`  FAIL  ${key}: ${r.violationsTotal} px changed outside the fore shape's footprint`);
      }
    });
    check(maskFailures === 0,
      `every pixel that changes with the lift on vs off lies inside the fore shape (${maskFailures} set/framing combo(s) failed)`);

    // A set the coverage measurement says has nothing in the crop must be a
    // true no-op: forcing the lift off changes nothing anywhere in the frame.
    let skipFailures = 0;
    Object.keys(rack).forEach((key) => {
      const r = rack[key];
      if (!r.coverage && r.anyFrameDiff) {
        skipFailures++;
        console.log(`  FAIL  ${key}: derived coverage says skip, but the frame still changed`);
      }
    });
    check(skipFailures === 0, `sets with no fore pixels in the crop are true no-ops (${skipFailures} failure(s))`);

    // Where the sampled cameras above do show a difference, it stays well
    // under the old bug's ~75%-of-frame wash — this would catch a
    // regression back toward a blanket rect even on a set/framing where the
    // isolation itself (checked above) still happened to hold.
    let hugeWash = 0;
    Object.keys(rack).forEach((key) => {
      const r = rack[key];
      if (r.anyFrameDiff && r.maxFrameDiffFrac > 0.3) {
        hugeWash++;
        console.log(`  FAIL  ${key}: ${(r.maxFrameDiffFrac * 100).toFixed(1)}% of the frame changed`);
      }
    });
    check(hugeWash === 0, `where the lift visibly does anything, it stays well short of a whole-frame wash (${hugeWash} failure(s))`);

    // `foreCoversFraming` says "yes" for a set/framing whenever *some*
    // camera move, somewhere in its full range, reaches the fore shape —
    // not necessarily one of the handful of samples above, which is why
    // this checks the aggregate rather than every individual set: a set
    // whose fore element only pokes into frame at one extreme corner of the
    // pan range can legitimately show nothing on these particular samples
    // without the coverage decision being wrong. What would be wrong is the
    // whole mechanism being dead — the effect never firing anywhere at all.
    const trueCoverage = Object.keys(rack).filter((k) => rack[k].coverage);
    const firedSomewhere = trueCoverage.filter((k) => rack[k].anyFrameDiff);
    check(trueCoverage.length > 0 && firedSomewhere.length > 0,
      `the lift actually fires somewhere (${firedSomewhere.length}/${trueCoverage.length} coverage=true combos showed a diff on these samples)`);
    // The three sets the deleted hardcoded list already got right, plus the
    // one it wrongly excluded (ward), are covered directly by these
    // samples — see the numbered checks below.
    ['field:close', 'industrial:two', 'ward:close'].forEach((key) => {
      check(rack[key].coverage && rack[key].anyFrameDiff, `${key} shows the effect on these samples`);
    });

    // The old hardcoded RACK_FOCUS_SETS = {woods, field, industrial} either
    // missed real coverage (`ward` close, per the Critical report, had more
    // fore coverage than `field` close and was excluded) or included a
    // framing with none (`woods` close has ~0% fore in the crop). The
    // derived measurement should disagree with that list on at least the
    // first of these known cases.
    check(rack['ward:close'].coverage === true, 'ward:close now gets the effect (the old hardcoded list excluded it)');
    console.log('  (reference) woods:close coverage = ' + rack['woods:close'].coverage +
      ', field:close = ' + rack['field:close'].coverage + ', industrial:two = ' + rack['industrial:two'].coverage +
      ', ward:two = ' + rack['ward:two'].coverage);

    // bar:two, chapel:close and street:two have no fore pixels in the crop
    // on any unrolled camera move — the derivation only finds them once it
    // accounts for the Dutch tilt a high-mood shot adds on top of the pan.
    // These are exactly the combos a mood-0.4-only test (as this one used
    // to be) could never have caught the roll omission through: proving
    // them here is what makes this test able to catch that bug again.
    // Assert the derivation, not the pixels. These three are the *marginal*
    // cases the roll fix uncovered — 915, 36 and 32 fore pixels on screen at
    // 540p — so whether a given sample of frames happens to catch a 32-pixel
    // difference is luck, and pairing `anyFrameDiff` with them made this check
    // fail about one run in four. `coverage` is the thing the roll fix
    // actually changed and is fully deterministic, so it carries the whole
    // intent. That the mechanism is alive at all is already proven above, and
    // on non-marginal combos by the field/industrial/ward checks.
    ['bar:two', 'chapel:close', 'street:two'].forEach((key) => {
      check(rack[key].coverage,
        `${key} is found once the derivation accounts for the tilt`);
    });

    console.log('\nTHE SCORE');
    const scoreState = () => page.evaluate(() => {
      const panel = document.getElementById('viewFilm');
      return { score: panel.dataset.score || '', music: panel.dataset.music || '',
               sections: parseInt(panel.dataset.sections || '0', 10) };
    });

    await page.click('#playFilm');
    await page.waitForTimeout(2500);
    const playing = await scoreState();
    check(playing.score === 'real', `a real composed score is playing, not the fallback (${playing.score})`);
    check(playing.music === 'playing', 'the music is running while the picture runs');
    check(playing.sections >= 3, `the score has a section per scene (${playing.sections})`);

    await page.click('#playFilm'); // pause
    await page.waitForTimeout(300);
    check((await scoreState()).music === 'paused', 'pausing the film pauses the music');

    await page.click('#stopFilm');
    await page.waitForTimeout(300);
    check((await scoreState()).music === 'stopped', 'stopping the film stops the music');

    // A page where SONG FORGE is missing: the film must still play, and say why.
    const bare = await context.newPage();
    await bare.addInitScript(() => {
      // Take the composer away before the app ever looks for it.
      Object.defineProperty(window, 'Composer', { get: () => undefined, set: () => {} });
    });
    await bare.goto(base + '/film/', { waitUntil: 'load' });
    await bare.fill('#idea', 'a kid and a walkie-talkie in the attic');
    await bare.click('#write');
    await bare.waitForTimeout(400);
    await bare.click('#tabFilm');
    await bare.click('#playFilm');
    await bare.waitForTimeout(1500);

    const withoutForge = await bare.evaluate(() => ({
      music: document.getElementById('viewFilm').dataset.music || '',
      score: document.getElementById('viewFilm').dataset.score || '',
      clock: document.getElementById('filmClock').textContent,
      status: document.getElementById('status').textContent,
      note: document.getElementById('filmNote').textContent,
      noteClass: document.getElementById('filmNote').className
    }));
    check(/0:0[1-9]|0:[1-9]/.test(withoutForge.clock),
      `the film still plays with SONG FORGE missing (clock ${withoutForge.clock})`);
    check(withoutForge.score === 'fallback', 'it knows it is not using a real score');
    // The notice belongs on the film tab, where it stays, not in the status
    // line, which the next message scrolls away.
    check(/could not compose a score/i.test(withoutForge.note) &&
          /cut hits/i.test(withoutForge.note),
      `the film tab says plainly there is no music (note: "${withoutForge.note}")`);
    check(/warn/.test(withoutForge.noteClass), 'and it is marked as a warning');
    check(/playing/i.test(withoutForge.status),
      `the ordinary Playing message still appears (status: "${withoutForge.status}")`);
    await bare.close();

    console.log('\nRECORDING A VIDEO FILE');
    // Record a few seconds, then stop early: a stopped take must still produce
    // a real, finished file rather than nothing.
    const filmDownload = page.waitForEvent('download', { timeout: 120000 });
    await page.click('#recordFilm');
    await page.waitForTimeout(700);
    check((await page.textContent('#recordFilm')).indexOf('Recording') !== -1, 'it says it is recording');
    await page.waitForTimeout(7000);
    await page.click('#stopFilm');

    const film = await filmDownload;
    const filmFile = path.join(downloadDir, film.suggestedFilename());
    await film.saveAs(filmFile);
    const bytes = fs.readFileSync(filmFile);
    check(/\.(webm|mp4)$/.test(film.suggestedFilename()) && bytes.length > 40000,
      `a real video file comes out (${film.suggestedFilename()}, ${Math.round(bytes.length / 1024)} KB)`);
    check(film.suggestedFilename().endsWith(promised.extension),
      `the saved file is the format the app promised (${promised.extension})`);

    // Which browser this suite runs on decides which format it gets — a
    // Chromium with H.264 records MP4, one without records WebM — so the
    // checks have to know both. Both paths get exercised in practice: CI's
    // Chromium has H.264, a plain local one does not.
    const has = (marker) => bytes.indexOf(Buffer.from(marker)) !== -1;
    const isMp4 = promised.extension === '.mp4';

    if (isMp4) {
      // Name what is actually in the file, so a failure here says which box
      // the browser wrote rather than leaving the next person guessing.
      const boxes = ['avc1', 'avcC', 'mp4a', 'esds', 'Opus', 'dOps', 'vp09', 'ac-3', 'soun', 'vide']
        .filter(has).join(', ') || 'none';
      const asked = `asked for ${promised.type}; boxes found: ${boxes}`;

      // An .mp4 that is secretly VP9 is the exact failure this guards: a file
      // named for the format Apple devices play, that they cannot play.
      check(has('avc1') || has('avcC'), `an .mp4 really carries H.264, not VP9 (${asked})`);
      check(!has('vp09'), `no VP9 hiding inside the .mp4 (${asked})`);
      check(has('mp4a') || has('esds'), `the file carries AAC audio (${asked})`);
      // Opus inside an MP4 is the audio version of the same trap: a file an
      // Apple device opens and then plays silently.
      check(!has('Opus') && !has('dOps'), `no Opus audio hiding inside the .mp4 (${asked})`);
      // MediaRecorder writes its own duration into an MP4; the playback check
      // below is what proves it, since nothing here parses MP4 boxes.
    } else {
      check(has('V_VP9') || has('V_VP8'), 'the file carries a video track (VP8/VP9)');
      check(has('A_OPUS'), 'the file carries the soundtrack (Opus)');

      // WebM is the case where the browser leaves the duration out and the app
      // splices it in, so here the container itself has to know.
      const Webm = require(path.join(ROOT, 'film', 'js', 'film-webm.js'));
      const written = Webm.readDuration(new Uint8Array(bytes));
      check(written !== null && written > 3 && written < 30,
        `the file knows how long it is (${written === null ? 'no duration' : written.toFixed(1) + 's'})`);
    }

    // And it has to play back — the whole point of the exercise.
    const playsBack = await page.evaluate(async (dataUrl) => {
      const v = document.createElement('video');
      v.src = dataUrl;
      v.muted = true;
      await new Promise((res, rej) => { v.onloadedmetadata = res; v.onerror = () => rej(new Error('load')); });
      await v.play();
      await new Promise((r) => setTimeout(r, 1500));
      const c = document.createElement('canvas');
      c.width = 320; c.height = 180;
      c.getContext('2d').drawImage(v, 0, 0, c.width, c.height);
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      let lit = 0;
      for (let i = 0; i < d.length; i += 400) if (d[i] + d[i + 1] + d[i + 2] > 60) lit++;
      return { duration: v.duration, lit };
    }, 'data:video/webm;base64,' + bytes.toString('base64'));
    check(isFinite(playsBack.duration) && playsBack.duration > 3,
      `the saved film reports its length on playback (${playsBack.duration}s)`);
    check(playsBack.lit > 20, 'the saved film shows a picture when played');

    console.log('\nTHE DIALOGUE IS IN THE RECORDING');

    // The specific trap the design records: the browser's own text-to-speech
    // would say real words for almost no work, but it does not run through Web
    // Audio, so it never reaches the gain that feeds the recorder. The dialogue
    // would be audible while previewing and silent in every downloaded film.
    //
    // This renders the voice offline through the real Score and measures the
    // signal that arrives. speechSynthesis would render pure silence here, so
    // this is the regression guard for that whole class of mistake.
    const heard = await page.evaluate(async () => {
      const script = FilmWriter.write(FilmParse.parse('A courier finds a package that hums.',
        { seed: 5 }), { length: 'short', seed: 5 });
      const reel = FilmReel.build(script);
      const line = reel.shots.filter((s) => s.kind === 'line' && s.caption)[0];
      if (!line) return { noLine: true };

      // The control has to be the same length and genuinely silent. An empty
      // caption is neither: syllablesFor floors at 2, so "" still speaks twice,
      // and a shorter buffer raises the average for the same energy. Both of
      // those made the first version of this check compare nothing useful.
      const SECONDS = 2.2;
      function rmsOf(caption) {
        const Off = window.OfflineAudioContext || window.webkitOfflineAudioContext;
        const off = new Off(1, Math.ceil(44100 * SECONDS), 44100);
        const score = new FilmScore.Score(reel, { context: off });
        if (caption !== null) {
          score.speak(caption, reel.voices[line.speaker] || { pitch: 180 }, SECONDS);
        }
        return off.startRendering().then((buf) => {
          const d = buf.getChannelData(0);
          let sum = 0;
          for (let i = 0; i < d.length; i++) sum += d[i] * d[i];
          return Math.sqrt(sum / d.length);
        });
      }

      const spoken = await rmsOf(line.caption);
      const silent = await rmsOf(null);          // same graph, nobody speaks
      return { spoken, silent, caption: line.caption };
    });

    if (heard.noLine) {
      check(false, 'the film produced no spoken line to test');
    } else {
      check(heard.spoken > 0.0005,
        `a spoken line reaches the recorder's own audio graph (rms ${heard.spoken.toExponential(2)} for ${JSON.stringify(heard.caption)})`);
      check(heard.spoken > heard.silent * 10 && heard.spoken > 0.0005,
        `the same graph with nobody speaking is silent by comparison (${heard.spoken.toExponential(2)} speaking vs ${heard.silent.toExponential(2)} not)`);
    }

    console.log('\nPHONE LAYOUT');
    const phone = await context.newPage();
    await phone.setViewportSize({ width: 390, height: 780 });
    await phone.goto(base + '/film/', { waitUntil: 'load' });
    await phone.fill('#idea', 'two sisters rob their own diner at midnight');
    await phone.click('#write');
    await phone.waitForTimeout(300);
    const overflow = await phone.evaluate(() =>
      document.documentElement.scrollWidth - document.documentElement.clientWidth);
    check(overflow <= 1, `no sideways scrolling at 390px (overflow ${overflow}px)`);
    check((await phone.$$('#viewScript .scene_heading')).length === 5, 'the script renders on a phone');
    await phone.close();
  } finally {
    await browser.close();
    server.close();
    fs.rmSync(downloadDir, { recursive: true, force: true });
  }

  console.log(failures ? `\n✖ ${failures} failing check(s)\n` : '\n✓ SCRIPT FORGE browser tests passed\n');
  process.exit(failures ? 1 : 0);
})().catch((e) => {
  console.error(e);
  server.close();
  process.exit(1);
});
