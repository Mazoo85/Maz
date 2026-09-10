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

    console.log('\nEMPTY INPUT');
    await page.fill('#idea', '');
    await page.click('#write');
    await page.waitForTimeout(150);
    check(/type what your film is about/i.test(await page.textContent('#status')), 'an empty idea is refused politely');

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
      // An .mp4 that is secretly VP9 is the exact failure this guards: a file
      // named for the format Apple devices play, that they cannot play.
      check(has('avc1') || has('avcC'), 'an .mp4 really carries H.264, not VP9 in an MP4 wrapper');
      check(!has('vp09'), 'no VP9 hiding inside the .mp4');
      check(has('mp4a') || has('esds'), 'the file carries the soundtrack (AAC)');
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
