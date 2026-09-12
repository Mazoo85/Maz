#!/usr/bin/env node
/*
 * ZOMBOID: ANCHORAGE — the browser test.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node zomboid/tests/zomboid-browser.test.js
 *
 * zomboid-logic.test.js checks the map, the generator and the loot without a
 * browser. This checks the half that needs one — and in particular the thing
 * the logic tests cannot see at all: that the soundtrack ZOMBOID now consumes
 * from SONG FORGE (music/soundtrack, declared in shared/exchange.json) really
 * plays, rather than merely being loaded.
 *
 * A cross-project dependency that loads but does nothing is the worst kind:
 * check-exchange proves it is declared, check-links proves the file is there,
 * the page boots clean — and the game is silent. Only playing it finds that.
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

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8216;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
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
  const opts = {
    args: [
      '--no-sandbox', '--use-gl=swiftshader', '--mute-audio',
      // The audio is muted, but the context must still be allowed to start or
      // there is nothing to assert about. Muted-but-running is exactly what is
      // wanted here: the graph runs, CI hears nothing.
      '--autoplay-policy=no-user-gesture-required'
    ]
  };
  for (const c of [
    process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    '/opt/pw-browsers/chromium/chrome-linux/chrome'
  ]) {
    if (c && fs.existsSync(c)) { opts.executablePath = c; break; }
  }
  return opts;
}

/* ZOMBOID opens on a boot screen, then a title screen; Enter moves through both. */
async function startGame(page) {
  await page.keyboard.press('Enter');
  await page.waitForTimeout(250);
  await page.keyboard.press('Enter');
  await page.waitForTimeout(1200);
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 1100, height: 800 } });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

  try {
    await page.goto(`http://127.0.0.1:${PORT}/zomboid/`, { waitUntil: 'load' });
    await page.waitForTimeout(400);

    console.log('\nLOADING');
    check(problems.length === 0, 'the page loads clean' + (problems.length ? ' — ' + problems.join('; ') : ''));
    check(await page.evaluate(() => typeof window.MazSoundtrack === 'object'),
      'SONG FORGE\'s soundtrack surface is on the page');
    check(await page.evaluate(() => typeof window.AUDIO === 'object'), 'the game\'s audio layer is there');

    console.log('\nSTARTING A RUN');
    await startGame(page);
    const canvas = await page.evaluate(() => {
      const c = document.querySelector('canvas');
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      let lit = 0;
      for (let i = 0; i < d.length; i += 4 * 151) if (d[i] + d[i + 1] + d[i + 2] > 60) lit++;
      return { lit, w: c.width, h: c.height };
    });
    check(canvas.w > 0 && canvas.h > 0, `the canvas is sized (${canvas.w}x${canvas.h})`);
    check(canvas.lit > 0, 'Anchorage is on screen');

    console.log('\nTHE SOUNDTRACK');
    const playing = await page.evaluate(() => window.AUDIO.nowPlaying());
    check(!!playing, 'a soundtrack was created' + (playing ? '' : ' — the game is playing in silence'));
    if (playing) {
      check(playing.playing === true, 'it is actually playing, not merely loaded');
      check(playing.genre === 'chiptune', `it kept ZOMBOID's chiptune character (got ${playing.genre})`);
      check(typeof playing.title === 'string' && playing.title.length > 0,
        `SONG FORGE composed a real, titled track ("${playing.title}")`);
      check(playing.bpm > 20 && playing.bpm < 250, `at a plausible tempo (${playing.bpm} bpm)`);
      // The mood director runs every frame off the time of day and what is
      // nearby. Whatever it picked, it must be one SONG FORGE knows.
      const moods = await page.evaluate(() => window.MazSoundtrack.options().moods);
      check(moods.includes(playing.mood), `the mood is one SONG FORGE knows (${playing.mood})`);
    }

    console.log('\nTHE MUTE KEY STILL RULES THE MUSIC');
    // The game's own L key predates the integration. It must still win, or the
    // soundtrack has taken something away from the player.
    await page.keyboard.press('l');
    await page.waitForTimeout(400);
    check((await page.evaluate(() => window.AUDIO.isEnabled())) === false, 'L turns the audio off');
    check((await page.evaluate(() => window.AUDIO.nowPlaying().playing)) === false,
      'the soundtrack stops with it, rather than playing on regardless');
    await page.keyboard.press('l');
    await page.waitForTimeout(600);
    check((await page.evaluate(() => window.AUDIO.isEnabled())) === true, 'L turns it back on');
    check((await page.evaluate(() => window.AUDIO.nowPlaying().playing)) === true, 'and the music comes back');

    console.log('\nPLAYING ON');
    await page.keyboard.down('w');
    await page.waitForTimeout(1200);
    await page.keyboard.up('w');
    check(problems.length === 0, 'nothing threw while playing' + (problems.length ? ' — ' + problems.join('; ') : ''));
  } finally {
    await browser.close();
    server.close();
  }

  console.log('');
  if (failures) {
    console.log(`✗ ${failures} check(s) failed\n`);
    process.exit(1);
  }
  console.log('✓ ZOMBOID browser tests passed\n');
})().catch((e) => {
  console.error(e);
  server.close();
  process.exit(1);
});
