/*
 * Smoke test for the single-file build.
 *
 * Inlining is exactly the kind of step that silently breaks a page — a lost
 * script, a stray "</script>", a stylesheet that never applied. This loads
 * songforge.html the way a person would (as a file on disk, no server) and
 * checks the app boots, composes, plays and stays free of errors.
 *
 *   node music/tests/standalone.test.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

let chromium;
try {
  chromium = require('playwright').chromium;
} catch (e) {
  try {
    chromium = require('playwright-core').chromium;
  } catch (e2) {
    console.error('Playwright is not installed. Run: npm --prefix music/tests install');
    process.exit(2);
  }
}

const FILE = path.join(__dirname, '..', 'songforge.html');

let failures = 0;
function check(cond, msg) {
  console.log((cond ? '  ok   ' : '  FAIL ') + msg);
  if (!cond) failures++;
}

/* Wait for the transport to actually move rather than assuming a fixed delay
     is enough; a loaded machine can start the audio clock late. */
  async function playheadAdvances(p, ms) {
    const first = parseInt(await p.locator('#seek').inputValue(), 10);
    const deadline = Date.now() + (ms || 6000);
    let last = first;
    while (Date.now() < deadline) {
      await p.waitForTimeout(150);
      last = parseInt(await p.locator('#seek').inputValue(), 10);
      if (last > first) break;
    }
    return { first: first, last: last, moved: last > first };
  }

function launchOptions() {
  const opts = {
    args: ['--no-sandbox', '--autoplay-policy=no-user-gesture-required', '--use-gl=swiftshader']
  };
  const candidates = [process.env.CHROMIUM_PATH, '/opt/pw-browsers/chromium-1194/chrome-linux/chrome'];
  for (let i = 0; i < candidates.length; i++) {
    if (candidates[i] && fs.existsSync(candidates[i])) { opts.executablePath = candidates[i]; break; }
  }
  return opts;
}

(async function () {
  if (!fs.existsSync(FILE)) {
    console.error('songforge.html is missing — run: node music/build-standalone.js');
    process.exit(1);
  }

  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 900, height: 1000 } });
  const errors = [];
  const offlineFonts = [];
  /* The page asks Google Fonts for its display faces and falls back cleanly
     when they don't arrive — which is the whole point of a file that has to
     work offline. So a failed request is tolerated only when it is one of
     those font URLs; every other failure is still an error. */
  function isFontHost(url) {
    return /^https:\/\/fonts\.(googleapis|gstatic)\.com\//.test(url);
  }
  function watch(p, label) {
    p.on('pageerror', function (e) { errors.push(label + 'pageerror: ' + e.message); });
    p.on('requestfailed', function (r) {
      if (isFontHost(r.url())) offlineFonts.push(r.url());
      else errors.push(label + 'request failed: ' + r.url());
    });
    p.on('console', function (m) {
      if (m.type() !== 'error') return;
      const text = m.text();
      if (/Failed to load resource/.test(text)) return;   // covered by requestfailed
      errors.push(label + 'console: ' + text);
    });
  }
  watch(page, '');

  // file:// — no server, exactly how someone would open it.
  await page.goto('file://' + FILE);
  await page.waitForTimeout(400);

  console.log('\n— the single file boots —');
  check(await page.locator('#genreChips .chip').count() === 8, 'all eight genres present');
  check(await page.locator('#moodChips .chip').count() === 5, 'all five moods present');
  check(await page.locator('#helpModal').isHidden(), 'help modal stays closed');
  check(await page.evaluate(function () {
    return ['Theory', 'Genres', 'Composer', 'Synth', 'Engine', 'Exporter']
      .every(function (k) { return !!window[k]; });
  }), 'every module inlined and running');
  check(await page.evaluate(function () {
    return getComputedStyle(document.body).backgroundImage.indexOf('gradient') >= 0;
  }), 'stylesheet applied');

  console.log('\n— it composes and plays —');
  await page.click('#generateBtn');
  await page.waitForTimeout(800);
  check(await page.locator('#songPanel').isVisible(), 'a song appears');
  check((await page.locator('#songTitle').textContent()).trim().length > 2, 'the song is named');
  check(await page.locator('.chord-cell').count() > 0, 'chords are shown');

  const moved = await playheadAdvances(page);
  check(moved.moved, 'playback advances (' + moved.first + ' → ' + moved.last + ')');

  console.log('\n— audio is real —');
  const audio = await page.evaluate(async function () {
    const song = window.Composer.compose({ seed: 'STANDALONE', genre: 'synthwave', length: 'short' });
    const from = 16 * 4, len = 8 * 4;
    Object.keys(song.tracks).forEach(function (k) {
      song.tracks[k] = song.tracks[k].filter(function (e) { return e.t >= from && e.t < from + len; })
        .map(function (e) { const c = {}; for (const f in e) c[f] = e[f]; c.t = e.t - from; return c; });
    });
    song.totalBeats = len;
    const mix = {};
    window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
    const buf = await window.Engine.renderOffline(song, mix);
    const ch = buf.getChannelData(0);
    let peak = 0, sum = 0, clipped = 0;
    const start = Math.floor(buf.sampleRate * 0.2);
    for (let i = start; i < ch.length; i++) {
      const a = Math.abs(ch[i]);
      if (a > peak) peak = a;
      if (a > 0.999) clipped++;
      sum += a * a;
    }
    return { peak: peak, rms: Math.sqrt(sum / (ch.length - start)), clipped: clipped };
  });
  check(audio.peak > 0.05 && audio.peak <= 1.0001, 'audible and inside full scale (peak ' + audio.peak.toFixed(3) + ')');
  check(audio.clipped === 0, 'no clipped samples');
  check(audio.peak / audio.rms > 2.5, 'dynamics intact (crest ' + (audio.peak / audio.rms).toFixed(1) + ')');

  console.log('\n— the editor —');
  /* Put the editor in a settled state before clicking on it: stopped, and not
     following the playhead. Toggling blind is not enough — if autoplay never
     started, a click on play would start it and the view would scroll away
     mid-edit. Read the state, then set it. */
  if (await page.evaluate(function () {
    return document.getElementById('playIcon').textContent === '❚❚';
  })) {
    await page.click('#playBtn');
  }
  if (await page.evaluate(function () {
    return document.getElementById('followBtn').classList.contains('on');
  })) {
    await page.click('#followBtn');
  }
  await page.waitForTimeout(200);
  check(await page.evaluate(function () {
    return document.getElementById('playIcon').textContent === '▶' &&
           !document.getElementById('followBtn').classList.contains('on');
  }), 'editor is settled: stopped, not following');
  check(await page.locator('#editPanel').isVisible(), 'edit panel appears with a song');
  check(await page.locator('#editTracks .chip').count() === 6, 'a chip for every part');

  const grid = page.locator('#editor');

  /* Click through the element itself so Playwright scrolls it into view and
     uses element-relative coordinates — the editor sits well below the fold. */
  async function clickGrid(x, y) {
    await grid.click({ position: { x: x, y: y } });
    await page.waitForTimeout(120);
  }
  const leadBefore = await page.evaluate(function () { return window.__song.tracks.lead.length; });
  await clickGrid(300, 150);
  const leadAfter = await page.evaluate(function () { return window.__song.tracks.lead.length; });
  check(leadAfter === leadBefore + 1, 'clicking the grid draws a note (' + leadBefore + ' → ' + leadAfter + ')');

  const drawn = await page.evaluate(function () {
    const song = window.__song, ed = window.__editor;
    const e = song.tracks.lead[song.tracks.lead.length - 1];
    const rel = ((e.p - window.Theory.midi(song.rootPc, 4)) % 12 + 12) % 12;
    // Where that note actually sits, so the erase click can find it.
    const row = ed.rowOfPitch(Math.round(e.p));
    return {
      inKey: song.scaleSteps.indexOf(rel) >= 0,
      y: ed.yOfRow(row) + ed.rowH() / 2,
      x: ed.xOfBeat(e.t + e.d / 2)
    };
  });
  check(drawn.inKey, 'the drawn note lands in the song\u2019s key');

  await page.click('#toolErase');
  await clickGrid(drawn.x, drawn.y);
  const erased = await page.evaluate(function () { return window.__song.tracks.lead.length; });
  check(erased === leadBefore, 'erase removes it again (' + leadAfter + ' → ' + erased + ')');
  await page.click('#toolDraw');

  await page.click('#editTracks .chip[data-id="drums"]');
  await page.waitForTimeout(150);
  const drumsBefore = await page.evaluate(function () { return window.__song.tracks.drums.length; });
  const drumY = await page.evaluate(function () {
    const ed = window.__editor;
    return ed.yOfRow(0) + ed.rowH() / 2;      // the kick row
  });
  await clickGrid(300, drumY);
  const drumsAfter = await page.evaluate(function () { return window.__song.tracks.drums.length; });
  check(drumsAfter !== drumsBefore, 'tapping the drum grid changes the pattern (' +
    drumsBefore + ' → ' + drumsAfter + ')');
  await page.click('#editTracks .chip[data-id="lead"]');
  await page.waitForTimeout(120);

  console.log('\n— the loop: what you draw teaches the generator —');
  const developed = await page.evaluate(function () {
    const song = window.__song;
    /* A deliberate four-note idea, drawn by hand — built from the song's OWN
       scale degrees rather than fixed MIDI numbers. Against a random key, two
       fixed pitches can snap to the same degree and the shape reads as flat,
       which says nothing about whether the motif survived. */
    const T = window.Theory;
    const root = T.midi(song.rootPc, 5);
    const deg = function (d) { return T.degreePitch(song.scaleSteps, root, d); };
    song.tracks.lead = [
      { t: 0,    d: 0.5, p: deg(0), v: 0.8 },
      { t: 0.5,  d: 0.5, p: deg(1), v: 0.8 },
      { t: 1.5,  d: 0.5, p: deg(3), v: 0.8 },
      { t: 2.5,  d: 1.0, p: deg(2), v: 0.8 }
    ];
    const motif = window.Composer.motifFromEvents(song, song.tracks.lead);
    const ok = window.Composer.developPart(song, 'lead');
    return {
      motifLength: motif ? motif.length : 0,
      contour: motif ? motif.map(function (m) { return m.contour; }) : [],
      ok: ok,
      notesAfter: song.tracks.lead.length,
      spread: song.tracks.lead.length
        ? Math.max.apply(null, song.tracks.lead.map(function (e) { return e.t; })) : 0
    };
  });
  check(developed.motifLength === 4, 'it reads the four notes as a motif');
  const c = developed.contour;
  check(c[0] === 0 && c[1] > c[0] && c[2] > c[1] && c[3] < c[2],
    'the shape survives — up, up, then down (' + c.join(',') + ')');
  check(developed.ok, 'develop accepts the idea');
  check(developed.notesAfter > 20, 'the idea is spread across the song (' + developed.notesAfter + ' notes)');
  check(developed.spread > 40, 'and reaches the far end of it (last note at beat ' +
    developed.spread.toFixed(0) + ')');

  console.log('\n— locks —');
  const locks = await page.evaluate(function () {
    const song = window.__song;
    const before = JSON.stringify(song.tracks.lead);
    return { before: before };
  });
  await page.click('#mixer .track[data-id="lead"] .lock-btn');
  await page.click('#rerollAllBtn');
  await page.waitForTimeout(250);
  const kept = await page.evaluate(function () { return JSON.stringify(window.__song.tracks.lead); });
  check(kept === locks.before, 'a locked part survives a re-roll of everything');
  const others = await page.evaluate(function () { return window.__song.tracks.bass.length > 0; });
  check(others, 'the unlocked parts were still rewritten');

  console.log('\n— exports —');
  const ex = await page.evaluate(async function () {
    const song = window.Composer.compose({ seed: 'ZIPTEST', genre: 'lofi', length: 'short' });
    const midi = window.Exporter.buildMidi(song);
    const bytes = new Uint8Array(await midi.arrayBuffer());
    const zip = window.Exporter.makeZip([{ name: 'song.mid', bytes: bytes }]);
    const zb = new Uint8Array(await zip.slice(0, 4).arrayBuffer());
    return {
      midiTag: String.fromCharCode.apply(null, bytes.slice(0, 4)),
      zipSig: zb[0] === 0x50 && zb[1] === 0x4b && zb[2] === 0x03 && zb[3] === 0x04,
      zipSize: zip.size,
      midiSize: midi.size
    };
  });
  check(ex.midiTag === 'MThd', 'MIDI still writes a valid header');
  check(ex.zipSig, 'zip writer produces a real PK archive');
  check(ex.zipSize > ex.midiSize, 'zip contains the file (' + ex.zipSize + ' ≥ ' + ex.midiSize + ')');

  console.log('\n— phone —');
  const phone = await browser.newPage({ viewport: { width: 390, height: 780 }, isMobile: true, hasTouch: true });
  watch(phone, 'phone ');
  await phone.goto('file://' + FILE);
  await phone.click('#generateBtn');
  await phone.waitForTimeout(600);
  const overflow = await phone.evaluate(function () {
    return document.documentElement.scrollWidth - document.documentElement.clientWidth;
  });
  check(overflow <= 1, 'no sideways scrolling at 390px (' + overflow + 'px)');

  console.log('\n— console —');
  check(errors.length === 0, 'no page errors' + (errors.length ? ':\n    ' + errors.slice(0, 6).join('\n    ') : ''));
  if (offlineFonts.length) {
    console.log('  note   web fonts unreachable here (' + offlineFonts.length +
      ' request(s)); the page fell back to its system stack, which is the offline behaviour');
  }

  await browser.close();
  console.log('\n' + (failures === 0 ? 'PASS' : failures + ' FAILURES'));
  process.exit(failures === 0 ? 0 : 1);
})().catch(function (e) { console.error(e); process.exit(1); });
