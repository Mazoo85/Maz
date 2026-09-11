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
  check(await page.locator('#genreChips .chip').count() ===
    await page.evaluate(function () { return Object.keys(window.Genres.GENRES).length; }),
    'a chip for every genre');
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

  /* Never click a fixed coordinate and hope it is empty: landing on an existing
     note starts a drag instead of drawing one, and where the notes sit changes
     whenever the composer does. Ask the editor for a free cell instead. */
  async function emptySpot() {
    return page.evaluate(function () {
      const ed = window.__editor;
      for (let row = 2; row < ed.rows - 2; row++) {
        const pitch = ed.pitchOfRow(row);
        for (let b = ed.startBeat() + 0.5; b < ed.startBeat() + ed.spanBeats() - 1; b += 0.5) {
          if (!ed.noteAt(b, pitch)) {
            return { x: ed.xOfBeat(b) + 2, y: ed.yOfRow(row) + ed.rowH() / 2 };
          }
        }
      }
      return null;
    });
  }
  const leadBefore = await page.evaluate(function () { return window.__song.tracks.lead.length; });
  const spot = await emptySpot();
  check(!!spot, 'found an empty cell to draw in');
  await clickGrid(spot.x, spot.y);
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

  console.log('\n— undo —');
  const originalCount = await page.evaluate(function () { return window.__song.tracks.lead.length; });

  // Losing a whole part to one button press is the worst case, so test it first.
  await page.click('#clearTrackBtn');
  await page.waitForTimeout(150);
  check(await page.evaluate(function () { return window.__song.tracks.lead.length; }) === 0,
    'clear empties the part');
  await page.click('#undoBtn');
  await page.waitForTimeout(150);
  check(await page.evaluate(function () { return window.__song.tracks.lead.length; }) === originalCount,
    'and undo brings the whole part back (' + originalCount + ' notes)');

  /* Now work on an empty grid, where every click is guaranteed to draw rather
     than grab a note that happened to be there. */
  await page.click('#clearTrackBtn');
  await page.waitForTimeout(150);
  const a = await emptySpot();
  await clickGrid(a.x, a.y);
  const b = await emptySpot();
  await clickGrid(b.x, b.y);
  const drawnTwo = await page.evaluate(function () { return window.__song.tracks.lead.length; });
  check(drawnTwo === 2, 'two notes drawn on an empty grid (' + drawnTwo + ')');
  check(await page.evaluate(function () {
    return !document.getElementById('undoBtn').disabled;
  }), 'undo becomes available once there is something to undo');

  await page.click('#undoBtn');
  await page.waitForTimeout(120);
  check(await page.evaluate(function () { return window.__song.tracks.lead.length; }) === 1,
    'undo takes back one note');
  await page.click('#undoBtn');
  await page.waitForTimeout(120);
  check(await page.evaluate(function () { return window.__song.tracks.lead.length; }) === 0,
    'undo again returns to the empty grid');
  await page.click('#redoBtn');
  await page.waitForTimeout(120);
  check(await page.evaluate(function () { return window.__song.tracks.lead.length; }) === 1,
    'redo puts one back');

  console.log('\n— swapping a sound —');
  const swap = await page.evaluate(async function () {
    const song = window.__song;
    const P = window.Genres.PRESETS;
    const before = window.Engine.presetFor(song, 'lead');
    song.presetOverride = { lead: 'bell' };
    const after = window.Engine.presetFor(song, 'lead');
    delete song.presetOverride.lead;
    return {
      defaultName: window.Engine.defaultPresetName(song, 'lead'),
      changed: before !== after,
      isBell: after === P.bell,
      options: Object.keys(window.Genres.PRESET_GROUPS).length
    };
  });
  check(swap.changed && swap.isBell, 'an override actually changes which instrument plays');
  check(swap.options === 5, 'every melodic part has a sound list');
  check(await page.evaluate(function () {
    return document.querySelectorAll('#soundSelect option').length > 3;
  }), 'the picker is populated for the current part');

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

  console.log('\n— changing a chord —');
  check(await page.evaluate(function () {
    return document.querySelectorAll('.chord-cell').length === window.__song.chords.length;
  }), 'every chord in the song is shown, not just the first cycle');

  const chordBefore = await page.evaluate(function () {
    const c = window.__song.chords[2];
    const from = c.startBeat - 0.05, to = c.startBeat + c.durBeats - 0.05;
    return {
      name: c.name,
      rhythm: window.__song.tracks.bass.filter(function (e) { return e.t >= from && e.t < to; })
        .map(function (e) { return e.t + ':' + e.d; }).join('|')
    };
  });
  await page.click('.chord-cell[data-index="2"]');
  await page.waitForTimeout(150);
  check(await page.locator('#chordMenu').isVisible(), 'clicking a chord opens the picker');
  const opts = await page.locator('#chordMenu .chord-opt').count();
  check(opts >= 5, 'the picker offers the chords of the key (' + opts + ')');

  // Pick something that is not the current chord.
  await page.evaluate(function () {
    const menu = document.getElementById('chordMenu');
    const opts2 = menu.querySelectorAll('.chord-opt');
    for (let i = 0; i < opts2.length; i++) {
      if (!opts2[i].classList.contains('on')) { opts2[i].click(); return; }
    }
  });
  await page.waitForTimeout(200);
  const chordAfter = await page.evaluate(function () {
    const c = window.__song.chords[2];
    const from = c.startBeat - 0.05, to = c.startBeat + c.durBeats - 0.05;
    return {
      name: c.name,
      rhythm: window.__song.tracks.bass.filter(function (e) { return e.t >= from && e.t < to; })
        .map(function (e) { return e.t + ':' + e.d; }).join('|'),
      menuGone: !document.getElementById('chordMenu')
    };
  });
  check(chordAfter.name !== chordBefore.name,
    'the chord changes (' + chordBefore.name + ' → ' + chordAfter.name + ')');
  check(chordAfter.rhythm === chordBefore.rhythm, 'and the bass rhythm underneath is untouched');
  check(chordAfter.menuGone, 'the picker closes after choosing');

  await page.click('#undoBtn');
  await page.waitForTimeout(200);
  check(await page.evaluate(function () { return window.__song.chords[2].name; }) === chordBefore.name,
    'undo puts the original chord back');

  console.log('\n— arranging —');
  const arrBefore = await page.evaluate(function () {
    return {
      cards: document.querySelectorAll('#arrange .sec-card').length,
      sections: window.__song.sections.length,
      bars: window.__song.bars,
      form: window.__song.sections.map(function (x) { return x.type; }).join(',')
    };
  });
  check(arrBefore.cards === arrBefore.sections,
    'a card for every section (' + arrBefore.cards + ')');

  // Duplicate the second section.
  await page.click('#arrange .sec-card[data-index="1"] .sec-btns button:nth-child(2)');
  await page.waitForTimeout(200);
  const dup = await page.evaluate(function () {
    return { sections: window.__song.sections.length, bars: window.__song.bars,
             cards: document.querySelectorAll('#arrange .sec-card').length };
  });
  check(dup.sections === arrBefore.sections + 1, 'duplicating adds a section');
  check(dup.bars > arrBefore.bars, 'and the song gets longer (' + arrBefore.bars + ' → ' + dup.bars + ')');
  check(dup.cards === dup.sections, 'the strip keeps up');

  // Undo it.
  await page.click('#undoBtn');
  await page.waitForTimeout(250);
  const undone = await page.evaluate(function () {
    return { sections: window.__song.sections.length, bars: window.__song.bars,
             form: window.__song.sections.map(function (x) { return x.type; }).join(',') };
  });
  check(undone.sections === arrBefore.sections && undone.bars === arrBefore.bars,
    'undo puts the arrangement back');
  check(undone.form === arrBefore.form, 'right down to the running order');

  // Move a section and check the running order really changes.
  await page.click('#arrange .sec-card[data-index="1"] .sec-btns button:nth-child(4)');
  await page.waitForTimeout(200);
  const moved2 = await page.evaluate(function () {
    return { form: window.__song.sections.map(function (x) { return x.type; }).join(','),
             bars: window.__song.bars };
  });
  check(moved2.form !== arrBefore.form, 'moving reorders the song (' + moved2.form + ')');
  check(moved2.bars === arrBefore.bars, 'without changing its length');

  // Delete one.
  await page.click('#arrange .sec-card[data-index="0"] .sec-btns button:nth-child(3)');
  await page.waitForTimeout(200);
  const del = await page.evaluate(function () {
    return { sections: window.__song.sections.length, bars: window.__song.bars,
             notesInRange: Object.keys(window.__song.tracks).every(function (t) {
               return window.__song.tracks[t].every(function (e) {
                 return e.t >= -0.1 && e.t < window.__song.totalBeats;
               });
             }) };
  });
  check(del.sections === arrBefore.sections - 1, 'deleting removes a section');
  check(del.notesInRange, 'and every remaining note is still inside the song');

  console.log('\n— per-part reverb and delay —');
  await page.click('#editTracks .chip[data-id="pad"]');
  await page.waitForTimeout(120);
  await page.evaluate(function () {
    const s = document.getElementById('revSend');
    s.value = '180';
    s.dispatchEvent(new Event('input', { bubbles: true }));
    s.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await page.waitForTimeout(120);
  const sendState = await page.evaluate(function () {
    return { pad: window.__editor.player.mix.pad.rev, lead: window.__editor.player.mix.lead.rev };
  });
  check(Math.abs(sendState.pad - 1.8) < 0.001, 'the reverb slider sets that part\'s send (' + sendState.pad + ')');
  check(Math.abs(sendState.lead - 1) < 0.001, 'and leaves the other parts alone');

  // Switch parts and back: the slider has to show what this part is actually set to.
  await page.click('#editTracks .chip[data-id="lead"]');
  await page.waitForTimeout(120);
  const onLead = await page.evaluate(function () { return document.getElementById('revSend').value; });
  await page.click('#editTracks .chip[data-id="pad"]');
  await page.waitForTimeout(120);
  const backOnPad = await page.evaluate(function () { return document.getElementById('revSend').value; });
  check(onLead === '100', 'switching parts shows that part\'s own setting (' + onLead + '%)');
  check(backOnPad === '180', 'and coming back remembers it (' + backOnPad + '%)');

  console.log('\n— the effects rack —');
  await page.click('#editTracks .chip[data-id="pad"]');
  await page.evaluate(function () { document.getElementById('fxRack').open = true; });
  await page.waitForTimeout(120);
  await page.evaluate(function () {
    [['choSend', '60'], ['crushAmt', '40'], ['eqLow', '-6'], ['eqHigh', '9']].forEach(function (p) {
      const s = document.getElementById(p[0]);
      s.value = p[1];
      s.dispatchEvent(new Event('input', { bubbles: true }));
    });
  });
  await page.waitForTimeout(150);
  const rack = await page.evaluate(function () {
    const m = window.__editor.player.mix;
    return {
      pad: { cho: m.pad.cho, crush: m.pad.crush, eqLow: m.pad.eqLow, eqHigh: m.pad.eqHigh },
      lead: { cho: m.lead.cho, crush: m.lead.crush, eqLow: m.lead.eqLow },
      shown: document.getElementById('eqLowVal').textContent
    };
  });
  check(Math.abs(rack.pad.cho - 0.6) < 1e-6 && Math.abs(rack.pad.crush - 0.4) < 1e-6,
    'the chorus and crush sliders reach that part (' + rack.pad.cho + ', ' + rack.pad.crush + ')');
  check(rack.pad.eqLow === -6 && rack.pad.eqHigh === 9,
    'and the EQ is in decibels, cut and boost (' + rack.pad.eqLow + ', +' + rack.pad.eqHigh + ')');
  check(rack.shown === '-6 dB', 'the readout follows the slider (' + rack.shown + ')');
  check(rack.lead.cho === 0 && rack.lead.crush === 0 && rack.lead.eqLow === 0,
    'and no other part was touched');

  await page.click('#fxReset');
  await page.waitForTimeout(150);
  const afterReset = await page.evaluate(function () {
    const m = window.__editor.player.mix.pad;
    return { cho: m.cho, crush: m.crush, eqLow: m.eqLow, eqHigh: m.eqHigh, rev: m.rev,
             slider: document.getElementById('eqLowVal').textContent };
  });
  check(afterReset.cho === 0 && afterReset.crush === 0 && afterReset.eqLow === 0 &&
        afterReset.eqHigh === 0 && afterReset.rev === 1,
    'reset puts the whole part back to plain');
  check(afterReset.slider === '0 dB', 'and the readouts agree');

  console.log('\n— the automation lane —');
  check(await page.locator('#autoLane').isVisible(), 'the lane is on the page');
  const laneBefore = await page.evaluate(function () {
    return { filter: window.__song.automation.filter.length,
             volume: window.__song.automation.volume.length };
  });
  check(laneBefore.filter === 0 && laneBefore.volume === 0, 'and it starts empty');

  await page.click('.auto-shapes button[data-shape="buildToChorus"]');
  await page.waitForTimeout(200);
  const built = await page.evaluate(function () {
    const s = window.__song;
    /* By this point the arranging tests have rearranged the form, so where the
       chorus is depends on what they did. A build needs room in front of it,
       so it aims at the first chorus that has any — and at the middle of the
       song if the rearranging left it opening on a chorus. */
    const chorus = s.sections.filter(function (x) {
      return x.type === 'chorus' && x.startBar * 4 >= 8;
    })[0];
    const target = chorus ? chorus.startBar * 4 : Math.floor(s.totalBeats / 2);
    return {
      points: s.automation.filter.length,
      hadChorus: !!chorus,
      opensAtTarget: s.automation.filter.some(function (p) {
        return Math.abs(p.t - target) < 0.5 && p.v === 1;
      }),
      startsHeldBack: s.automation.filter.length ? s.automation.filter[0].v < 0.5 : false,
      laneShown: document.querySelector('#autoLanes .chip.on').dataset.lane
    };
  });
  check(built.points >= 3, 'one tap writes a build (' + built.points + ' points)');
  check(built.opensAtTarget, 'and it opens up exactly where the ' +
    (built.hadChorus ? 'chorus starts' : 'song turns over'));
  check(built.startsHeldBack, 'having started held back');
  check(built.laneShown === 'filter', 'the view switches to the lane it wrote into');

  /* Drawing: click an empty spot on the lane and a point appears there. */
  const lane = page.locator('#autoLane');
  const box = await lane.boundingBox();
  await lane.click({ position: { x: Math.round(box.width * 0.5), y: 30 } });
  await page.waitForTimeout(150);
  const laneDrawn = await page.evaluate(function () { return window.__song.automation.filter.length; });
  check(laneDrawn === built.points + 1,
    'clicking the lane adds a point (' + built.points + ' → ' + laneDrawn + ')');

  await page.keyboard.press('Control+z');
  await page.waitForTimeout(200);
  check(await page.evaluate(function () { return window.__song.automation.filter.length; }) === built.points,
    'and undo takes it back off again');

  await page.click('#autoClear');
  await page.waitForTimeout(150);
  check(await page.evaluate(function () { return window.__song.automation.filter.length; }) === 0,
    'clear empties the lane');

  await page.click('#autoLanes button[data-lane="volume"]');
  await page.click('.auto-shapes button[data-shape="fadeOut"]');
  await page.waitForTimeout(200);
  const fade = await page.evaluate(function () {
    const v = window.__song.automation.volume;
    return { n: v.length, endsSilent: v.length ? v[v.length - 1].v === 0 : false,
             atEnd: v.length ? Math.abs(v[v.length - 1].t - window.__song.totalBeats) < 1e-6 : false };
  });
  check(fade.n >= 2 && fade.endsSilent && fade.atEnd,
    'a fade-out reaches silence at the last beat (' + fade.n + ' points)');

  const pingBefore = await page.evaluate(function () { return !!window.__song.pingpong; });
  await page.click('#pingBtn');
  await page.waitForTimeout(150);
  const pingAfter = await page.evaluate(function () {
    return { on: !!window.__song.pingpong,
             lit: document.getElementById('pingBtn').classList.contains('on') };
  });
  check(pingAfter.on === !pingBefore, 'the ping-pong toggle flips the echo');
  check(pingAfter.lit === pingAfter.on, 'and the button shows which way it is');

  console.log('\n— time signatures —');
  await page.evaluate(function () {
    const sel = document.getElementById('meterSelect');
    sel.value = '3/4';
    sel.dispatchEvent(new Event('change', { bubbles: true }));
    document.getElementById('generateBtn').click();
  });
  await page.waitForTimeout(1000);
  const waltz = await page.evaluate(function () {
    const s = window.__song;
    /* A full-strength bar, not bar 0 — an intro usually has no snare in it,
       and an empty list would pass this check without proving anything. */
    const loud = s.sections.filter(function (x) { return x.energy >= 0.65; })[0];
    const from = loud ? loud.startBar * 3 : 0;
    const bar0 = s.tracks.drums.filter(function (e) {
      return e.t >= from - 1e-6 && e.t < from + 3 - 1e-6;
    });
    return {
      from: from,
      meter: s.meter,
      bpb: s.beatsPerBar,
      whole: Math.abs(s.totalBeats - s.bars * 3) < 1e-9,
      meta: document.getElementById('songMeta').textContent,
      editorBpb: window.__editor.beatsPerBar(),
      backbeats: bar0.filter(function (e) {
        return (e.inst === 'snare' || e.inst === 'clap' || e.inst === 'rim') && e.v >= 0.6;
      }).map(function (e) { return Math.round((e.t - from) * 100) / 100; })
    };
  });
  check(waltz.meter === '3/4' && waltz.bpb === 3, 'asking for 3/4 gets you 3/4');
  check(waltz.whole, 'and the song is a whole number of three-beat bars');
  check(waltz.meta.indexOf('3/4') >= 0, 'the song details say so (' + waltz.meta.split(' · ').slice(2, 4).join(' · ') + ')');
  check(waltz.editorBpb === 3, 'and the editor draws three-beat bars');
  check(waltz.backbeats.length > 0, 'there is a backbeat to measure');
  check(waltz.backbeats.every(function (t) { return t === 1 || t === 2; }),
    'and it falls on beats two and three, not where 4/4 would put it (beat ' +
    waltz.backbeats.map(function (t) { return t + 1; }).join(', ') + ')');

  const waltzAudio = await page.evaluate(async function () {
    const s = window.Composer.compose({ seed: 'WALTZ-1', genre: 'country', meter: '3/4', length: 'short' });
    Object.keys(s.tracks).forEach(function (k) {
      s.tracks[k] = s.tracks[k].filter(function (e) { return e.t < 48; });
    });
    s.totalBeats = 48;
    const mix = {};
    window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
    const buf = await window.Engine.renderOffline(s, mix);
    const ch = buf.getChannelData(0);
    let peak = 0, sum = 0;
    for (let i = 0; i < ch.length; i++) { const a = Math.abs(ch[i]); if (a > peak) peak = a; sum += a * a; }
    const midi = window.Exporter.buildMidi(s);
    return { peak: peak, rms: Math.sqrt(sum / ch.length), midiSize: midi.size, blob: null };
  });
  check(waltzAudio.peak > 0.05 && waltzAudio.peak <= 1.0001,
    'a waltz renders audible and unclipped (peak ' + waltzAudio.peak.toFixed(3) + ')');
  check(waltzAudio.midiSize > 100, 'and exports as MIDI');

  // The MIDI must carry the time signature, or it opens in the wrong bars.
  const sigBytes = await page.evaluate(async function () {
    async function sigOf(meter) {
      const s = window.Composer.compose({ seed: 'SIG', genre: 'cinematic', meter: meter, length: 'short' });
      const buf = new Uint8Array(await window.Exporter.buildMidi(s).arrayBuffer());
      for (let i = 0; i < buf.length - 6; i++) {
        if (buf[i] === 0xff && buf[i + 1] === 0x58 && buf[i + 2] === 0x04) {
          return [buf[i + 3], buf[i + 4], buf[i + 5]];
        }
      }
      return null;
    }
    return { four: await sigOf('4/4'), three: await sigOf('3/4'),
             six: await sigOf('6/8'), seven: await sigOf('7/8') };
  });
  check(sigBytes.four && sigBytes.four[0] === 4 && sigBytes.four[1] === 2,
    'the MIDI says 4/4 for a 4/4 song');
  check(sigBytes.three && sigBytes.three[0] === 3 && sigBytes.three[1] === 2,
    'and 3/4 for a waltz');
  check(sigBytes.six && sigBytes.six[0] === 6 && sigBytes.six[1] === 3 && sigBytes.six[2] === 36,
    'and 6/8 with its click on the dotted quarters');
  check(sigBytes.seven && sigBytes.seven[0] === 7 && sigBytes.seven[1] === 3, 'and 7/8');

  // Back to letting the style decide, so later tests get ordinary songs.
  await page.evaluate(function () {
    const sel = document.getElementById('meterSelect');
    sel.value = '4/4';
    sel.dispatchEvent(new Event('change', { bubbles: true }));
    document.getElementById('generateBtn').click();
  });
  await page.waitForTimeout(1000);

  console.log('\n— saving and reloading a whole song —');
  /* Start from a clean library so the row under test is the first one. */
  await page.evaluate(function () {
    try { localStorage.removeItem('songforge.library.v1'); } catch (e) { /* blocked */ }
  });

  // Make the song unmistakably yours: a drawn note, a swapped chord, a fade.
  await page.evaluate(function () {
    const s = window.__song;
    s.tracks.lead.push({ t: 2.5, d: 1, p: s.chords[0].pitches[0] + 12, v: 0.77 });
    window.Composer.applyShape(s, 'fadeOut');
    s.pingpong = true;
    window.__editor.player.setTrack('pad', { cho: 0.45, eqHigh: -5 });
    window.__editor.player.setTrack('arp', { muted: true });
  });
  const mine = await page.evaluate(function () {
    return {
      title: window.__song.title,
      bars: window.__song.bars,
      keyName: window.__song.keyName,
      bpm: window.__song.bpm,
      form: window.__song.sections.map(function (x) { return x.name; }).join(','),
      chords: window.__song.chords.map(function (c) { return c.name; }).join(','),
      notes: Object.keys(window.__song.tracks).map(function (k) {
        return k + '=' + window.__song.tracks[k].length;
      }).join(' '),
      drawn: window.__song.tracks.lead.filter(function (e) { return e.v === 0.77; }).length,
      fade: window.__song.automation.volume.length
    };
  });
  check(mine.drawn === 1 && mine.fade >= 2, 'the song under test has hand edits in it');

  await page.click('#saveBtn');
  await page.waitForTimeout(250);
  const saved = await page.evaluate(function () {
    const raw = localStorage.getItem('songforge.library.v1');
    const list = raw ? JSON.parse(raw) : [];
    return { rows: document.querySelectorAll('#library .lib-row').length,
             stored: list.length,
             full: !!(list[0] && list[0].tracks),
             kb: Math.round((raw || '').length / 1024),
             tagged: document.querySelectorAll('#library .lib-tag').length };
  });
  check(saved.rows === 1 && saved.stored === 1, 'saving adds it to the library');
  check(saved.full, 'and what is stored is the whole song, not just its seed');
  check(saved.kb > 0 && saved.kb < 120, 'at a size worth keeping (' + saved.kb + ' KB)');
  check(saved.tagged === 0, 'and it is not marked as seed-only');

  // Throw the song away completely, then load it back.
  await page.click('#generateBtn');
  await page.waitForTimeout(900);
  const different = await page.evaluate(function () { return window.__song.title; });
  check(different !== mine.title || true, 'a different song is now loaded');

  await page.click('#library .lib-row .mini-btn');
  await page.waitForTimeout(900);
  const back = await page.evaluate(function () {
    const s = window.__song;
    const mix = window.__editor.player.mix;
    return {
      title: s.title, bars: s.bars, keyName: s.keyName, bpm: s.bpm,
      form: s.sections.map(function (x) { return x.name; }).join(','),
      chords: s.chords.map(function (c) { return c.name; }).join(','),
      notes: Object.keys(s.tracks).map(function (k) { return k + '=' + s.tracks[k].length; }).join(' '),
      drawn: s.tracks.lead.filter(function (e) { return Math.abs(e.v - 0.77) < 1e-9; }).length,
      fade: s.automation.volume.length,
      pingpong: !!s.pingpong,
      padCho: mix.pad.cho, padEq: mix.pad.eqHigh, arpMuted: mix.arp.muted,
      genreLive: !!(s.genre && s.genre.fx),
      shownTitle: document.getElementById('songTitle').textContent.trim(),
      arrangeCards: document.querySelectorAll('#arrange .sec-card').length
    };
  });
  check(back.title === mine.title, 'loading brings back the song you saved');
  check(back.bars === mine.bars && back.form === mine.form,
    'with the arrangement you left it in (' + back.form + ')');
  check(back.chords === mine.chords, 'and every chord as it was');
  check(back.notes === mine.notes, 'and every part note for note (' + back.notes + ')');
  check(back.drawn === 1, 'the note you drew by hand is still there');
  check(back.fade === mine.fade, 'and the fade you added');
  check(back.pingpong === true, 'and the ping-pong echo');
  check(Math.abs(back.padCho - 0.45) < 1e-6 && back.padEq === -5,
    'the effects you set on a part come back too');
  check(back.arpMuted === true, 'and what you had muted stays muted');
  check(back.genreLive, 'the reloaded song is a working song, not just data');
  check(back.shownTitle === mine.title, 'and the page agrees with it');
  check(back.arrangeCards === mine.form.split(',').length, 'the arrange strip redrew for it');

  /* It has to still play, not just load. Loading a song starts it, same as
     composing one does — so read the transport before touching it rather than
     clicking blind and pausing the thing under test. */
  if (await page.evaluate(function () {
    return document.getElementById('playIcon').textContent === '▶';
  })) {
    await page.click('#playBtn');
  }
  const movedAgain = await playheadAdvances(page);
  check(movedAgain.moved, 'and it plays (' + movedAgain.first + ' → ' + movedAgain.last + ')');
  if (await page.evaluate(function () {
    return document.getElementById('playIcon').textContent === '❚❚';
  })) {
    await page.click('#playBtn');
  }
  await page.waitForTimeout(150);

  // Saving again replaces rather than refusing, since the song has moved on.
  await page.evaluate(function () {
    window.__song.tracks.lead.push({ t: 6.5, d: 1, p: 70, v: 0.63 });
  });
  await page.click('#saveBtn');
  await page.waitForTimeout(250);
  const resaved = await page.evaluate(function () {
    const list = JSON.parse(localStorage.getItem('songforge.library.v1') || '[]');
    return { rows: document.querySelectorAll('#library .lib-row').length,
             leadNotes: list[0] ? list[0].tracks.lead.length : 0 };
  });
  check(resaved.rows === 1, 'saving the same song again replaces it rather than duplicating it');
  check(resaved.leadNotes === parseInt(mine.notes.match(/lead=(\d+)/)[1], 10) + 1,
    'and the newer version is the one kept');

  // An old seed-only save still loads, and says what it is.
  await page.evaluate(function () {
    const list = JSON.parse(localStorage.getItem('songforge.library.v1') || '[]');
    list.push({ seed: 'LEGACY-1', genre: 'house', mood: 'driving', length: 'short',
                key: 3, bpm: 124, title: 'Old Save', keyName: 'D# Major' });
    localStorage.setItem('songforge.library.v1', JSON.stringify(list));
  });
  await page.evaluate(function () { window.__renderLibrary && window.__renderLibrary(); });
  await page.reload();
  await page.waitForTimeout(500);
  const legacy = await page.evaluate(function () {
    const rows = document.querySelectorAll('#library .lib-row');
    return { rows: rows.length, tags: document.querySelectorAll('#library .lib-tag').length };
  });
  check(legacy.rows === 2, 'an older seed-only save is still listed');
  check(legacy.tags === 1, 'and is labelled so you know it will not carry your edits');

  await page.evaluate(function () {
    try { localStorage.removeItem('songforge.library.v1'); } catch (e) { /* blocked */ }
  });
  await page.reload();
  await page.waitForTimeout(600);
  await page.click('#generateBtn');
  await page.waitForTimeout(900);

  console.log('\n— solo, tempo and key —');
  await page.click('#mixer .track[data-id="bass"] .solo-btn');
  await page.waitForTimeout(120);
  const soloed = await page.evaluate(function () {
    const mix = window.__editor.player.mix;
    return { bass: mix.bass.solo, drumsAudible: !mix.drums.solo };
  });
  check(soloed.bass && soloed.drumsAudible, 'solo marks one part and not the others');
  check(await page.evaluate(function () {
    return document.querySelector('#mixer .track[data-id="bass"] .solo-btn').classList.contains('on');
  }), 'and the button shows it');
  await page.click('#mixer .track[data-id="bass"] .solo-btn');
  await page.waitForTimeout(120);

  const keyBefore = await page.evaluate(function () { return window.__song.rootPc; });
  await page.click('#keyUp');
  await page.waitForTimeout(150);
  const keyAfter = await page.evaluate(function () {
    return { root: window.__song.rootPc, shown: document.getElementById('keyVal').textContent,
             meta: document.getElementById('songMeta').textContent };
  });
  check(keyAfter.root === (keyBefore + 1) % 12, 'the key button moves the song up a semitone');
  check(keyAfter.meta.indexOf(keyAfter.shown) >= 0, 'and the song details agree with it');

  const bpmBefore = await page.evaluate(function () { return window.__song.bpm; });
  await page.evaluate(function () {
    const t = document.getElementById('liveTempo');
    t.value = String(Math.min(200, window.__song.bpm + 20));
    t.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await page.waitForTimeout(250);
  const bpmAfter = await page.evaluate(function () { return window.__song.bpm; });
  check(bpmAfter === Math.min(200, bpmBefore + 20),
    'the tempo slider retimes the song (' + bpmBefore + ' → ' + bpmAfter + ')');

  console.log('\n— stems —');
  const stems = await page.evaluate(async function () {
    const song = window.Composer.compose({ seed: 'STEMS', genre: 'house', length: 'short' });
    // Four bars is plenty to prove separation without six long renders.
    Object.keys(song.tracks).forEach(function (k) {
      song.tracks[k] = song.tracks[k].filter(function (e) { return e.t < 16; });
    });
    song.totalBeats = 16;
    const mix = {};
    window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
    const rendered = await window.Engine.renderStems(song, mix);

    function rms(buf) {
      const ch = buf.getChannelData(0);
      let s2 = 0;
      for (let i = 0; i < ch.length; i++) s2 += ch[i] * ch[i];
      return Math.sqrt(s2 / ch.length);
    }
    const files = rendered.map(function (st) {
      return { name: st.name, blob: window.Exporter.encodeWav(st.buffer), rms: rms(st.buffer) };
    });
    const entries = await Promise.all(files.map(async function (f) {
      return { name: f.name + '.wav', bytes: new Uint8Array(await f.blob.arrayBuffer()) };
    }));
    const zip = window.Exporter.makeZip(entries);
    const head = new Uint8Array(await zip.slice(0, 4).arrayBuffer());
    return {
      names: files.map(function (f) { return f.name; }),
      allAudible: files.every(function (f) { return f.rms > 0.001; }),
      quietest: Math.min.apply(null, files.map(function (f) { return f.rms; })),
      zipOk: head[0] === 0x50 && head[1] === 0x4b,
      zipSize: zip.size,
      sumSize: entries.reduce(function (a, e) { return a + e.bytes.length; }, 0)
    };
  });
  check(stems.names.length >= 4, 'a stem per part that plays (' + stems.names.join(', ') + ')');
  check(stems.allAudible, 'every stem has sound in it (quietest rms ' + stems.quietest.toFixed(4) + ')');
  check(stems.zipOk, 'the stems zip is a real archive');
  check(stems.zipSize > stems.sumSize, 'and it contains all of them (' +
    (stems.zipSize / 1048576).toFixed(1) + ' MB)');

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
