/*
 * End-to-end tests in a real browser.
 *
 * The logic suite proves the notes are right; this one proves the app actually
 * works: the page loads clean, the controls respond, the transport advances,
 * every genre renders *audible* audio (not silence, not clipping), and the WAV
 * and MIDI exports are real files.
 *
 *   npm --prefix music/tests install
 *   node music/tests/music-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build, or let playwright-core find
 * its own download.
 */
'use strict';

const http = require('http');
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

const ROOT = path.join(__dirname, '..');
const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8199;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };

const server = http.createServer(function (req, res) {
  let p = decodeURIComponent(req.url.split('?')[0].split('#')[0]);
  if (p === '/') p = '/index.html';
  const file = path.join(ROOT, p);
  if (!file.startsWith(ROOT) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404); res.end('not found'); return;
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  res.end(fs.readFileSync(file));
});

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
  const candidates = [process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome'];
  for (let i = 0; i < candidates.length; i++) {
    if (candidates[i] && fs.existsSync(candidates[i])) { opts.executablePath = candidates[i]; break; }
  }
  return opts;
}

(async function () {
  await new Promise(function (r) { server.listen(PORT, r); });
  const base = 'http://127.0.0.1:' + PORT + '/index.html';

  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 900, height: 1000 } });

  const errors = [];
  const offlineFonts = [];
  /* The page pulls its display faces from Google Fonts and falls back cleanly
     when they don't arrive. A failed request is tolerated only when it is one
     of those font URLs; anything else is still an error. */
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

  await page.goto(base, { waitUntil: 'networkidle' });

  console.log('\n— page load —');
  check((await page.title()).indexOf('SONG FORGE') === 0, 'title present');
  const genreCount = await page.evaluate(function () { return Object.keys(window.Genres.GENRES).length; });
  check(await page.locator('#genreChips .chip').count() === genreCount,
    'a chip for every genre (' + genreCount + ')');
  check(await page.locator('#moodChips .chip').count() === 5, 'five mood chips rendered');
  check(await page.locator('#songPanel').isHidden(), 'song panel hidden before generating');
  check(await page.locator('#helpModal').isHidden(), 'help modal is not covering the page');

  console.log('\n— generate —');
  await page.click('#generateBtn');
  await page.waitForTimeout(900);
  check(await page.locator('#songPanel').isVisible(), 'song panel appears');
  check((await page.locator('#songTitle').textContent()).trim().length > 2, 'song has a title');
  const trackCount = await page.evaluate(function () { return window.Engine.TRACKS.length; });
  check(await page.locator('#mixer .track').count() === trackCount,
    'a mixer row for every part (' + trackCount + ')');
  check(await page.locator('.chord-cell').count() > 0, 'chord strip filled');
  check(await page.evaluate(function () {
    return document.getElementById('playIcon').textContent;
  }) === '❚❚', 'transport switched to playing');

  const moved = await playheadAdvances(page);
  check(moved.moved, 'playhead advances (' + moved.first + ' → ' + moved.last + ')');

  console.log('\n— every genre makes an audible sound —');
  const audio = await page.evaluate(async function () {
    /* Rendering whole songs for every genre takes minutes. An eight-bar
       excerpt from the middle of the song — where the arrangement is at full
       strength — proves the same thing in seconds. */
    function excerpt(song, fromBar, bars) {
      const from = fromBar * 4;
      const len = bars * 4;
      Object.keys(song.tracks).forEach(function (k) {
        song.tracks[k] = song.tracks[k]
          .filter(function (e) { return e.t >= from && e.t < from + len; })
          .map(function (e) {
            const c = {};
            for (const f in e) c[f] = e[f];
            c.t = e.t - from;
            return c;
          });
      });
      song.totalBeats = len;
      return song;
    }

    const out = [];
    const ids = Object.keys(window.Genres.GENRES);
    for (let i = 0; i < ids.length; i++) {
      const song = excerpt(
        window.Composer.compose({ seed: 'AUDIT-' + ids[i], genre: ids[i], length: 'short' }), 16, 8);
      const mix = {};
      window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
      const buf = await window.Engine.renderOffline(song, mix);
      const ch = buf.getChannelData(0);
      const chR = buf.numberOfChannels > 1 ? buf.getChannelData(1) : ch;
      const start = Math.floor(buf.sampleRate * 0.2);
      let peak = 0, sum = 0, clipped = 0, diff = 0, sumR = 0;
      for (let k = start; k < ch.length; k++) {
        const a = Math.abs(ch[k]);
        if (a > peak) peak = a;
        if (a > 0.999) clipped++;
        sum += a * a;
        sumR += chR[k] * chR[k];
        diff += Math.abs(ch[k] - chR[k]);
      }
      const n = ch.length - start;
      const rms = Math.sqrt(sum / n);
      out.push({
        id: ids[i], peak: peak, rms: rms,
        clipped: clipped, seconds: buf.duration,
        // How much the two channels differ, relative to the signal: 0 = mono.
        stereo: rms > 0 ? (diff / n) / rms : 0,
        balance: Math.sqrt(sumR / n) / (rms || 1)
      });
    }
    return out;
  });

  audio.forEach(function (r) {
    check(r.peak > 0.05, r.id + ': audible (peak ' + r.peak.toFixed(3) + ')');
    check(r.rms > 0.04, r.id + ': has body (rms ' + r.rms.toFixed(4) + ')');
    check(r.peak / r.rms > 2.5, r.id + ': keeps its dynamics (crest ' + (r.peak / r.rms).toFixed(1) + ')');
    check(r.rms < 0.30, r.id + ': not mastered into a brick wall (rms ' + r.rms.toFixed(4) + ')');
    check(r.peak <= 1.0001, r.id + ': stays inside full scale');
    check(r.clipped === 0, r.id + ': no clipped samples (' + r.clipped + ')');
    check(r.seconds > 8, r.id + ': rendered ' + r.seconds.toFixed(1) + 's');
    check(r.stereo > 0.05, r.id + ': is actually stereo (channel difference ' + r.stereo.toFixed(3) + ')');
    check(r.balance > 0.8 && r.balance < 1.25,
      r.id + ': stays balanced left to right (' + r.balance.toFixed(2) + ')');
  });

  console.log('\n— every instrument makes a sound —');
  const voices = await page.evaluate(async function () {
    /* A preset with a broken envelope or a bad synthesis path renders silence,
       and silence is exactly what nobody notices until they pick that sound.
       So play every one of them and measure. */
    function measure(buf) {
      const ch = buf.getChannelData(0);
      const chR = buf.numberOfChannels > 1 ? buf.getChannelData(1) : ch;
      let peak = 0, sum = 0, bad = 0;
      for (let i = 0; i < ch.length; i++) {
        const v = ch[i];
        if (!isFinite(v)) { bad++; continue; }
        const a = Math.abs(v);
        if (a > peak) peak = a;
        sum += a * a;
      }
      let diff = 0;
      for (let i = 0; i < ch.length; i++) diff += Math.abs(ch[i] - chR[i]);
      return { peak: peak, rms: Math.sqrt(sum / ch.length), bad: bad, spread: diff / ch.length };
    }
    function buses(ctx) {
      const dry = ctx.createGain(); dry.connect(ctx.destination);
      const rev = ctx.createGain(); rev.connect(ctx.destination);
      const del = ctx.createGain(); del.connect(ctx.destination);
      return { dry: dry, rev: rev, del: del };
    }

    const notes = [];
    const names = Object.keys(window.Genres.PRESETS);
    for (let i = 0; i < names.length; i++) {
      const ctx = new OfflineAudioContext(2, 44100 * 2, 44100);
      window.Synth.playNote(ctx, buses(ctx), 0.05, 1.0, 220,
        window.Genres.PRESETS[names[i]], 0.9, { brightness: 1 });
      const m = measure(await ctx.startRendering());
      notes.push({ name: names[i], peak: m.peak, rms: m.rms, bad: m.bad });
    }

    const hits = [];
    const kits = Object.keys(window.Synth.KITS);
    const pieces = Object.keys(window.Synth.DRUM_PAN);
    for (let i = 0; i < pieces.length; i++) {
      const ctx = new OfflineAudioContext(2, 44100 * 2, 44100);
      window.Synth.playDrum(ctx, buses(ctx), 0.05, pieces[i], 0.9, 'electro', 1.0);
      const m = measure(await ctx.startRendering());
      hits.push({ name: pieces[i], peak: m.peak, bad: m.bad });
    }
    return { notes: notes, hits: hits, kits: kits.length };
  });

  /* The physically modelled strings deserve their own pass. Every other voice
     is a chain of oscillators and filters that cannot run away; these are a
     delay line feeding back into itself, where feedback of 1 or more would
     grow without bound. The feedback is derived from the note's own pitch, so
     it has to be checked across the whole range and not at one middle C. */
  const strings = await page.evaluate(async function () {
    const names = Object.keys(window.Genres.PRESETS).filter(function (n) {
      return window.Genres.PRESETS[n].kind === 'string';
    });
    const out = [];
    for (let i = 0; i < names.length; i++) {
      for (const freq of [55, 110, 220, 440, 880, 1760]) {
        const ctx = new OfflineAudioContext(2, 44100 * 4, 44100);
        const dry = ctx.createGain(); dry.connect(ctx.destination);
        const rev = ctx.createGain(); rev.connect(ctx.destination);
        const del = ctx.createGain(); del.connect(ctx.destination);
        window.Synth.playNote(ctx, { dry: dry, rev: rev, del: del },
          0.05, 1.0, freq, window.Genres.PRESETS[names[i]], 0.9, { brightness: 1 });
        const buf = await ctx.startRendering();
        const ch = buf.getChannelData(0);
        let peak = 0, bad = 0, early = 0, late = 0;
        const mid = Math.floor(ch.length / 2);
        for (let k = 0; k < ch.length; k++) {
          const v = ch[k];
          if (!isFinite(v)) { bad++; continue; }
          const a = Math.abs(v);
          if (a > peak) peak = a;
          if (k < mid) { if (a > early) early = a; } else if (a > late) late = a;
        }
        out.push({ name: names[i], freq: freq, peak: peak, bad: bad, early: early, late: late });
      }
    }
    return out;
  });
  check(strings.length > 0, 'there are physically modelled instruments to check');
  check(strings.every(function (r) { return r.bad === 0; }), 'the string model never produces broken samples');
  check(strings.every(function (r) { return r.peak < 2; }),
    'and never runs away (loudest ' +
    Math.max.apply(null, strings.map(function (r) { return r.peak; })).toFixed(2) + ')');
  check(strings.every(function (r) { return r.peak > 0.01; }),
    'it sounds at every pitch, low to high');
  /* A string decays. If the second half of the render is not quieter than the
     first, the loop is feeding itself rather than losing energy. */
  const sustaining = strings.filter(function (r) { return r.late >= r.early; });
  check(sustaining.length === 0,
    'and it dies away rather than sustaining itself' +
    (sustaining.length ? ': ' + sustaining.map(function (r) {
      return r.name + '@' + r.freq + 'Hz';
    }).join(', ') : ''));

  const mute = voices.notes.filter(function (n) { return n.peak < 0.01; });
  const nan = voices.notes.filter(function (n) { return n.bad > 0; });
  check(voices.notes.length >= 30, 'a full palette of instruments (' + voices.notes.length + ')');
  check(mute.length === 0, 'none of them are silent' +
    (mute.length ? ': ' + mute.map(function (n) { return n.name; }).join(', ') : ''));
  check(nan.length === 0, 'none of them produce broken samples' +
    (nan.length ? ': ' + nan.map(function (n) { return n.name; }).join(', ') : ''));
  const tooLoud = voices.notes.filter(function (n) { return n.peak > 1.4; });
  check(tooLoud.length === 0, 'none of them are wildly louder than the rest' +
    (tooLoud.length ? ': ' + tooLoud.map(function (n) {
      return n.name + ' ' + n.peak.toFixed(2); }).join(', ') : ''));

  const quietHits = voices.hits.filter(function (h) { return h.peak < 0.01; });
  check(voices.hits.length >= 13, 'a full drum kit (' + voices.hits.length + ' pieces)');
  check(quietHits.length === 0, 'every drum piece sounds' +
    (quietHits.length ? ': ' + quietHits.map(function (h) { return h.name; }).join(', ') : ''));

  console.log('\n— the kick pumps the mix —');
  const pump = await page.evaluate(async function () {
    /* Measure the duck directly rather than inferring it from loudness spread.
       Render with the DRUMS MUTED — the duck still fires, because it is
       triggered when a kick is scheduled, not by the kick's sound — then compare
       the level just after each kick against the level just before it. Ducking
       makes "after" markedly quieter; without it the ratio sits near 1. */
    function excerpt(song, fromBar, bars) {
      const from = fromBar * 4, len = bars * 4;
      Object.keys(song.tracks).forEach(function (k) {
        song.tracks[k] = song.tracks[k].filter(function (e) { return e.t >= from && e.t < from + len; })
          .map(function (e) { const c = {}; for (const f in e) c[f] = e[f]; c.t = e.t - from; return c; });
      });
      song.totalBeats = len;
      return song;
    }
    async function ratioFor(genre, forceOff) {
      const song = excerpt(window.Composer.compose({ seed: 'PUMP', genre: genre, length: 'short' }), 16, 8);
      /* Measure the duck, not the instruments. Songs now draw alternate sounds
         per seed, and a pad with a slow swell rises through the measurement
         window and masks a duck that is working perfectly well. Pin the genre's
         default instruments so this reads the same thing every time. */
      song.presetOverride = {};
      /* The pump is the song's own setting now, seeded from the style rather
         than read from it on every kick, so this turns it off at the song and
         not at the genre — reaching into `genre.fx` leaves `song.sidechain`
         carrying the style's value and "off" measures identical to "on". */
      const depth = song.sidechain === undefined ? song.genre.fx.sidechain : song.sidechain;
      if (forceOff) song.sidechain = 0;
      const mix = {};
      window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: t === 'drums' }; });
      const buf = await window.Engine.renderOffline(song, mix);
      const ch = buf.getChannelData(0);
      const rate = buf.sampleRate;
      const spb = 60 / song.bpm;
      const offset = 0.05;                       // renderOffline's scheduling offset

      function rms(fromSec, toSec) {
        const a = Math.max(0, Math.floor(fromSec * rate));
        const b = Math.min(ch.length, Math.floor(toSec * rate));
        if (b <= a) return 0;
        let s2 = 0;
        for (let i = a; i < b; i++) s2 += ch[i] * ch[i];
        return Math.sqrt(s2 / (b - a));
      }

      const kicks = song.tracks.drums.filter(function (e) { return e.inst === 'kick'; });
      let after = 0, before = 0, n = 0;
      for (let i = 0; i < kicks.length; i++) {
        const t = kicks[i].t * spb + offset;
        if (t < 0.4 || t > buf.duration - 0.5) continue;
        const a = rms(t + 0.012, t + 0.055);     // duck is at its deepest
        const b = rms(t - 0.055, t - 0.012);     // recovered from the previous one
        if (b > 1e-4) { after += a; before += b; n++; }
      }
      return { ratio: n ? (after / before) : 1, kicks: n, depth: depth };
    }
    const out = {};
    for (const g of ['house', 'trap', 'synthwave', 'ambient']) {
      out[g] = { on: await ratioFor(g, false), off: await ratioFor(g, true) };
    }
    return out;
  });

  ['house', 'trap', 'synthwave'].forEach(function (g) {
    const on = pump[g].on, off = pump[g].off;
    check(on.kicks > 4, g + ': found kicks to measure against (' + on.kicks + ')');
    check(on.ratio < 0.9, g + ': the mix ducks under the kick (level after/before = ' +
      on.ratio.toFixed(2) + ', depth ' + on.depth + ')');
    check(on.ratio < off.ratio - 0.05, g + ': and it is the duck doing it, not the music (' +
      off.ratio.toFixed(2) + ' with the duck off → ' + on.ratio.toFixed(2) + ' with it on)');
  });
  check(pump.ambient.on.depth === 0, 'ambient has no duck at all, by design');
  check(Math.abs(pump.ambient.on.ratio - pump.ambient.off.ratio) < 0.02,
    'and ambient is unchanged either way');

  console.log('\n— per-track reverb and delay sends —');
  const sends = await page.evaluate(async function () {
    /* One part, one genre, three settings. If the send does nothing, all three
       renders come back the same; if it works, the wet one is longer and louder
       and the muted one is silent no matter how high the send is turned up. */
    async function render(revAmt, muted) {
      /* Cinematic in 4/4: a long reverb, no tape-noise bed, and a fixed meter
         so this measures the send rather than the arrangement. */
      const song = window.Composer.compose({ seed: 'SENDS-1', genre: 'cinematic',
                                             meter: '4/4', length: 'short' });
      /* A marimba on the arp, deliberately. Pads hold their release for two or
         three seconds, so a window taken after the last note is still full of
         the instrument and only faintly of the reverb — which makes a send test
         that mostly measures the pad, and passes or fails on which notes the
         arrangement happened to write. A mallet stops dead; anything still
         ringing afterwards is reverb and nothing else. */
      song.presetOverride = { arp: 'marimba' };
      // Short enough to render fast, long enough for a reverb tail to show.
      Object.keys(song.tracks).forEach(function (k) {
        song.tracks[k] = song.tracks[k].filter(function (e) { return e.t < 24; });
      });
      let lastEnd = 0;
      song.tracks.arp.forEach(function (e) { lastEnd = Math.max(lastEnd, e.t + e.d); });
      song.totalBeats = Math.ceil(lastEnd) + 1;   // so the render always covers the tail
      const mix = {};
      window.Engine.TRACKS.forEach(function (t) {
        mix[t] = { volume: 1, muted: t !== 'arp', solo: false, rev: 1, del: 1 };
      });
      mix.arp.rev = revAmt;
      if (muted) mix.arp.muted = true;
      const buf = await window.Engine.renderOffline(song, mix);
      const ch = buf.getChannelData(0);
      const rate = buf.sampleRate;
      let s2 = 0, peak = 0;
      for (let i = 0; i < ch.length; i++) { s2 += ch[i] * ch[i]; if (Math.abs(ch[i]) > peak) peak = Math.abs(ch[i]); }

      /* The tail is whatever is still ringing once the last note has stopped —
         so measure from just after it, not at some fraction of the buffer: pick
         the window too late and the reverb has already died and every setting
         reads zero. */
      const spb = 60 / song.bpm;
      const a = Math.min(ch.length, Math.floor((lastEnd * spb + 0.05 + 0.25) * rate));
      const b = Math.min(ch.length, Math.floor((lastEnd * spb + 0.05 + 1.60) * rate));
      let t2 = 0;
      for (let i = a; i < b; i++) t2 += ch[i] * ch[i];
      return {
        rms: Math.sqrt(s2 / ch.length),
        peak: peak,
        tailSamples: b - a,
        tail: b > a ? Math.sqrt(t2 / (b - a)) : 0
      };
    }
    return {
      dry: await render(0, false),
      normal: await render(1, false),
      wet: await render(2, false),
      mutedWet: await render(2, true)
    };
  });

  const t6 = function (x) { return x.toFixed(6); };
  check(sends.normal.rms > 1e-4, 'the part renders on its own (rms ' + sends.normal.rms.toFixed(4) + ')');
  check(sends.normal.tailSamples > 1000 && sends.normal.tail > 5e-5,
    'there is a real tail to measure (' + t6(sends.normal.tail) + ')');
  check(sends.dry.tail < sends.normal.tail * 0.75,
    'turning the reverb send down shortens the tail (' +
    t6(sends.normal.tail) + ' → ' + t6(sends.dry.tail) + ')');
  check(sends.wet.tail > sends.normal.tail * 1.4,
    'and turning it up lengthens it (' + t6(sends.wet.tail) + ')');
  check(sends.mutedWet.peak < 1e-5,
    'a muted part sends nothing, however high the send (peak ' + sends.mutedWet.peak.toExponential(1) + ')');

  console.log('\n— the new effects, measured —');
  const fx = await page.evaluate(async function () {
    /* One measuring stick for all five. `bright` is the average step between
       consecutive samples relative to level — a signal full of high frequencies
       moves further per sample than a dull one — and `stereo` is how far the two
       channels differ, which is 0 for anything sitting in the middle. */
    function measure(buf, from, to) {
      const L = buf.getChannelData(0);
      const R = buf.numberOfChannels > 1 ? buf.getChannelData(1) : L;
      const a = Math.max(1, Math.floor((from === undefined ? 0 : from) * L.length));
      const b = Math.min(L.length, Math.floor((to === undefined ? 1 : to) * L.length));
      let s2 = 0, hf = 0, diff = 0, peak = 0;
      for (let i = a; i < b; i++) {
        s2 += L[i] * L[i];
        hf += Math.abs(L[i] - L[i - 1]);
        diff += Math.abs(L[i] - R[i]);
        if (Math.abs(L[i]) > peak) peak = Math.abs(L[i]);
      }
      const n = Math.max(1, b - a);
      const rms = Math.sqrt(s2 / n);
      return {
        rms: rms, peak: peak,
        bright: rms > 0 ? (hf / n) / rms : 0,
        stereo: rms > 0 ? (diff / n) / rms : 0
      };
    }

    /* Cinematic has no tape-noise bed, so the master really is only the parts
       being measured. Trimmed to 16 bars to keep eight renders quick. */
    function song(setup) {
      const s = window.Composer.compose({ seed: 'FX-1', genre: 'cinematic', length: 'short' });
      s.presetOverride = {};
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t < 64; });
      });
      s.totalBeats = 68;
      if (setup) setup(s);
      return s;
    }
    function mixWith(only, fields) {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: only ? t !== only : false, solo: false, rev: 1, del: 1, cho: 0,
                 eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0 };
      });
      if (only && fields) for (const k in fields) m[only][k] = fields[k];
      return m;
    }
    const render = function (s, m) { return window.Engine.renderOffline(s, m); };

    const out = {};

    // 1. Filter automation — muffled at the start, wide open at the end.
    const swept = song(function (s) {
      s.automation.filter = [{ t: 0, v: 0 }, { t: s.totalBeats, v: 1 }];
    });
    const flat = song();
    const sweptBuf = await render(swept, mixWith(null));
    const flatBuf = await render(flat, mixWith(null));
    out.sweepEarly = measure(sweptBuf, 0.05, 0.3);
    out.sweepLate = measure(sweptBuf, 0.62, 0.88);
    out.flatEarly = measure(flatBuf, 0.05, 0.3);
    out.flatLate = measure(flatBuf, 0.62, 0.88);

    // 2. Volume automation — a fade to nothing.
    const faded = song(function (s) {
      s.automation.volume = [{ t: s.totalBeats * 0.5, v: 1 }, { t: s.totalBeats, v: 0 }];
    });
    const fadeBuf = await render(faded, mixWith(null));
    out.fadeMid = measure(fadeBuf, 0.4, 0.5);
    out.fadeEnd = measure(fadeBuf, 0.93, 0.99);

    /* 3. Chorus — same part, send up. Measured on a deliberately mono sound:
       the pads already split themselves hard left and right, so there is no
       room left to show a widening that is really happening. */
    const monoLead = function () {
      return song(function (s) { s.presetOverride = { lead: 'chipLead' }; });
    };
    const dryCho = await render(monoLead(), mixWith('lead', { rev: 0, del: 0, cho: 0 }));
    const wetCho = await render(monoLead(), mixWith('lead', { rev: 0, del: 0, cho: 1 }));
    out.choOff = measure(dryCho);
    out.choOn = measure(wetCho);

    /* 4. Ping-pong — the same echoes, moved out to the sides. Measured in the
       tail after the part has stopped, where what is left is echoes and
       nothing else: measuring the whole render mixes the instrument's own
       stereo width into the reading.
       On the *same* part the reverb test uses, and with the window taken from
       that part. Measuring one track's tail in a render whose length was set by
       another track's last note is how this ended up pointed at a window with
       no signal in it at all — and two near-silent windows compare equal, which
       reads as "the effect does nothing" whether or not it does. */
    const echoSong = function (ping) { return song(function (s) { s.pingpong = ping; }); };
    const echoWindow = function (buf, s) {
      let lastEnd = 0;
      s.tracks.arp.forEach(function (e) { lastEnd = Math.max(lastEnd, e.t + e.d); });
      const spb = 60 / s.bpm;
      const total = buf.length / buf.sampleRate;
      return [Math.min(0.95, (lastEnd * spb + 0.3) / total),
              Math.min(0.99, (lastEnd * spb + 2.2) / total)];
    };
    const offSong = echoSong(false), onSong = echoSong(true);
    const centred = await render(offSong, mixWith('arp', { rev: 0, del: 2, cho: 0 }));
    const bouncing = await render(onSong, mixWith('arp', { rev: 0, del: 2, cho: 0 }));
    const wOff = echoWindow(centred, offSong), wOn = echoWindow(bouncing, onSong);
    out.pingOff = measure(centred, wOff[0], wOff[1]);
    out.pingOn = measure(bouncing, wOn[0], wOn[1]);

    // 5. Bit crush — grit is high-frequency energy that was not there before.
    const clean = await render(song(), mixWith('bass', { rev: 0, del: 0, crush: 0 }));
    const crushed = await render(song(), mixWith('bass', { rev: 0, del: 0, crush: 1 }));
    out.crushOff = measure(clean);
    out.crushOn = measure(crushed);

    return out;
  });

  check(fx.sweepEarly.bright < fx.sweepLate.bright * 0.8,
    'a filter sweep starts dull and ends bright (' +
    fx.sweepEarly.bright.toFixed(4) + ' → ' + fx.sweepLate.bright.toFixed(4) + ')');
  check(Math.abs(fx.flatEarly.bright - fx.flatLate.bright) < fx.flatEarly.bright * 0.5,
    'and it is the automation doing it, not the arrangement (' +
    fx.flatEarly.bright.toFixed(4) + ' → ' + fx.flatLate.bright.toFixed(4) + ' with the lane empty)');
  check(fx.sweepEarly.bright < fx.flatEarly.bright * 0.8,
    'the swept opening really is duller than the same music unswept');

  check(fx.fadeMid.rms > 0.01, 'the fade test has music to fade (rms ' + fx.fadeMid.rms.toFixed(4) + ')');
  check(fx.fadeEnd.peak < 0.02,
    'a volume fade actually reaches silence (peak ' + fx.fadeEnd.peak.toFixed(5) + ')');

  check(fx.choOff.rms > 1e-4, 'the chorus test has a part to thicken');
  check(fx.choOn.stereo > fx.choOff.stereo * 1.25,
    'chorus widens the part (stereo ' + fx.choOff.stereo.toFixed(3) + ' → ' + fx.choOn.stereo.toFixed(3) + ')');
  check(fx.choOn.rms > fx.choOff.rms,
    'and thickens it (rms ' + fx.choOff.rms.toFixed(4) + ' → ' + fx.choOn.rms.toFixed(4) + ')');

  /* A real floor, not a token one: two silent windows compare equal, so a test
     that only checks the ratio would pass on an empty measurement. */
  check(fx.pingOff.rms > 1e-4, 'the echo test has echoes to move (rms ' +
    fx.pingOff.rms.toExponential(1) + ')');
  check(fx.pingOn.stereo > fx.pingOff.stereo * 1.5,
    'ping-pong throws the echoes to the sides (stereo ' +
    fx.pingOff.stereo.toFixed(3) + ' → ' + fx.pingOn.stereo.toFixed(3) + ')');

  check(fx.crushOff.rms > 1e-4, 'the crush test has a part to wreck');
  check(fx.crushOn.bright > fx.crushOff.bright * 1.5,
    'crushing adds grit that was not there (brightness ' +
    fx.crushOff.bright.toFixed(4) + ' → ' + fx.crushOn.bright.toFixed(4) + ')');

  console.log('\n— reverb shapes and echo timing —');
  /* The four shapes are told apart by what the tail *does over time*, not by
     how loud it is. A gated reverb is a long tail with the end cut off, which
     sounds nothing like a short tail even though one loudness reading cannot
     separate them — so this measures the energy just after the gate point
     against the energy just before it. That single ratio distinguishes all
     three: a room decays, a gate stops dead, a reverse swells. */
  const verbs = await page.evaluate(async function () {
    const out = {};
    for (const kind of ['room', 'gated', 'reverse']) {
      const ctx = new OfflineAudioContext(2, 44100 * 4, 44100);
      const conv = ctx.createConvolver();
      conv.buffer = window.Synth.reverbImpulse(ctx, 2.6, 2.4, kind);
      const dry = ctx.createGain(); dry.gain.value = 0.001;
      const wet = ctx.createGain(); wet.gain.value = 1;
      dry.connect(ctx.destination);
      wet.connect(conv).connect(ctx.destination);
      window.Synth.playNote(ctx, { dry: dry, rev: wet, del: ctx.createGain() },
        0.05, 0.3, 330, window.Genres.PRESETS.pluck, 0.9, { brightness: 1 });
      const buf = await ctx.startRendering();
      const ch = buf.getChannelData(0);
      const band = function (a, b) {
        let s2 = 0;
        const A = Math.floor(a * 44100), B = Math.floor(b * 44100);
        for (let i = A; i < B; i++) s2 += ch[i] * ch[i];
        return Math.sqrt(s2 / Math.max(1, B - A));
      };
      let peak = 0, bad = 0;
      for (let i = 0; i < ch.length; i++) {
        if (!isFinite(ch[i])) { bad++; continue; }
        if (Math.abs(ch[i]) > peak) peak = Math.abs(ch[i]);
      }
      // The gate closes at 28% of the impulse, so 0.78s after a note at 0.05s.
      const pre = band(0.5, 0.75), post = band(0.95, 1.5);
      out[kind] = { peak: peak, bad: bad, pre: pre, post: post,
                    ratio: post / Math.max(1e-9, pre) };
    }
    return out;
  });

  ['room', 'gated', 'reverse'].forEach(function (k) {
    check(verbs[k].bad === 0 && verbs[k].peak > 1e-4,
      k + ' reverb: renders a real tail (peak ' + verbs[k].peak.toFixed(3) + ')');
  });
  check(verbs.room.ratio > 0.25 && verbs.room.ratio < 1,
    'a room tail decays gradually (' + verbs.room.ratio.toFixed(2) + ' of its energy carries on)');
  check(verbs.gated.ratio < verbs.room.ratio * 0.4,
    'a gated tail stops dead where a room tail is still ringing (' +
    verbs.gated.ratio.toFixed(2) + ' vs ' + verbs.room.ratio.toFixed(2) + ')');
  check(verbs.reverse.ratio > 1.5,
    'a reverse tail swells instead of fading (' + verbs.reverse.ratio.toFixed(2) + ')');

  /* Shimmer is not an impulse shape but an arrangement — a second, brighter
     convolution an octave up — so it is checked through the engine. */
  const shimmer = await page.evaluate(async function () {
    async function bright(kind) {
      const s = window.Composer.compose({ seed: 'SHIM-1', genre: 'ambient',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = { pad: 'marimba' };
      s.revKind = kind;
      s.glue = 0;
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t < 16; });
      });
      s.totalBeats = 20;
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: t !== 'pad', solo: false, rev: 2, del: 0, cho: 0,
                 mod: 0, autopan: 0, eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0, comp: 0, punch: 0 };
      });
      const buf = await window.Engine.renderOffline(s, m);
      const ch = buf.getChannelData(0);
      let s2 = 0, hf = 0;
      for (let i = 1; i < ch.length; i++) { s2 += ch[i] * ch[i]; hf += Math.abs(ch[i] - ch[i - 1]); }
      const rms = Math.sqrt(s2 / ch.length);
      return { rms: rms, bright: rms > 0 ? (hf / ch.length) / rms : 0 };
      /* Measured across the render rather than in a window after the last note:
         the shimmer is fed from the send input, so its extra content sits under
         the notes and their tails, not in a quiet patch at the end. */
    }
    return { room: await bright('room'), shimmer: await bright('shimmer') };
  });
  check(shimmer.room.rms > 1e-4, 'there is a reverb to shimmer (rms ' +
    shimmer.room.rms.toFixed(4) + ')');
  /* A deliberately modest margin, and an honest one. What is being measured is
     the whole mix, in which the reverb return is one contribution among many —
     the shimmer is a sheen over a tail, not a new instrument. Measured at +2%
     level and +3% brightness, which is small but consistent and repeatable;
     claiming more than that here would be claiming more than was measured. */
  check(shimmer.shimmer.bright > shimmer.room.bright * 1.02,
    'shimmer puts a brighter voice over the tail (' + shimmer.room.bright.toFixed(5) +
    ' → ' + shimmer.shimmer.bright.toFixed(5) + ')');
  check(shimmer.shimmer.rms > shimmer.room.rms * 1.01,
    'and adds to it rather than replacing it (' + shimmer.room.rms.toFixed(5) +
    ' → ' + shimmer.shimmer.rms.toFixed(5) + ')');

  console.log('\n— echo flavours —');
  /* Digital, tape and multi-tap, told apart by the three things that actually
     make them different instruments rather than three settings: how dark the
     repeats get as they go, whether they feed back at all, and where they sit
     across the stereo field.

     Measured on cinematic, which has no tape-noise bed. On a genre that does,
     the noise floor swamps a quiet tail and all three read as identical — which
     is exactly what the first run of this showed. */
  const echoes = await page.evaluate(async function () {
    function win(buf, fromSec, toSec) {
      const L = buf.getChannelData(0);
      const R = buf.numberOfChannels > 1 ? buf.getChannelData(1) : L;
      const sr = buf.sampleRate;
      const a = Math.max(1, Math.floor(fromSec * sr));
      const b = Math.min(L.length, Math.floor(toSec * sr));
      let peak = 0, s2 = 0, hf = 0, diff = 0, bad = 0;
      for (let i = a; i < b; i++) {
        if (!isFinite(L[i])) { bad++; continue; }
        const v = Math.abs(L[i]); if (v > peak) peak = v;
        s2 += L[i] * L[i];
        hf += Math.abs(L[i] - L[i - 1]);
        diff += Math.abs(L[i] - R[i]);
      }
      const n = Math.max(1, b - a);
      const rms = Math.sqrt(s2 / n);
      return { peak: peak, rms: rms, bad: bad, spread: diff / n,
               bright: rms > 0 ? (hf / n) / rms : 0 };
    }
    function song(kind) {
      /* One short note and nothing else, so everything after it is echo. */
      const s = window.Composer.compose({ seed: 'DELAY-1', genre: 'cinematic',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = { lead: 'marimba' };
      Object.keys(s.tracks).forEach(function (k) { s.tracks[k] = []; });
      s.tracks.lead = [{ t: 0, d: 0.4, p: 72, v: 1 }];
      s.totalBeats = 16;
      s.glue = 0;
      s.delDiv = 0.5;
      s.delFb = 0.55;
      s.delKind = kind;
      return s;
    }
    function mix() {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        /* Sent well above unity: the preset's own delay send is a twelfth, and
           at that level the echoes are too quiet to measure anything about. */
        m[t] = { volume: 1, muted: t !== 'lead', solo: false, rev: 0, del: 6, cho: 0,
                 mod: 0, autopan: 0, colour: 0, eqLow: 0, eqMid: 0, eqHigh: 0,
                 crush: 0, comp: 0, punch: 0 };
      });
      return m;
    }
    const out = {};
    for (const kind of ['digital', 'tape', 'multi']) {
      const buf = await window.Engine.renderOffline(song(kind), mix());
      out[kind] = { taps: win(buf, 0.35, 1.9), late: win(buf, 3.0, 7.0) };
    }
    return out;
  });

  check(echoes.digital.taps.peak > 0.01,
    'there are echoes to measure (peak ' + echoes.digital.taps.peak.toFixed(3) + ')');
  ['digital', 'tape', 'multi'].forEach(function (k) {
    check(echoes[k].taps.bad === 0 && echoes[k].late.bad === 0,
      k + ': never produces broken samples');
    check(echoes[k].taps.peak < 1, k + ': never runs away (peak ' +
      echoes[k].taps.peak.toFixed(3) + ')');
  });
  /* Tape's repeats are filtered and saturated on their way round the loop, so
     each one comes back darker than the last — by the late window they have
     been round it five or six times and the difference is plain. */
  check(echoes.tape.late.bright < echoes.digital.late.bright * 0.75,
    'tape repeats get darker every time round (brightness ' +
    echoes.digital.late.bright.toFixed(4) + ' → ' + echoes.tape.late.bright.toFixed(4) + ')');
  /* Multi-tap has no feedback at all: three taps and done. Its late window is
     the proof — a feedback delay is still ringing there and this is not. */
  check(echoes.multi.late.peak < echoes.digital.late.peak * 0.4,
    'multi-tap does not feed back, so it stops (late peak ' +
    echoes.digital.late.peak.toFixed(4) + ' → ' + echoes.multi.late.peak.toFixed(4) + ')');
  check(echoes.multi.taps.spread > echoes.digital.taps.spread * 2,
    'and it spreads its taps across the field (' +
    echoes.digital.taps.spread.toFixed(5) + ' → ' + echoes.multi.taps.spread.toFixed(5) + ')');

  console.log('\n— swirl and sweep —');
  /* Three modulation effects that are the same idea at different scales, plus
     auto-pan. The flanger has a feedback loop, so it gets checked for runaway
     the same way the plucked strings and the reverb sends were. */
  const swirl = await page.evaluate(async function () {
    function stats(buf, from) {
      const L = buf.getChannelData(0);
      const R = buf.numberOfChannels > 1 ? buf.getChannelData(1) : L;
      const a = Math.floor((from || 0) * L.length);
      let peak = 0, s2 = 0, diff = 0, bad = 0, hf = 0;
      for (let i = Math.max(1, a); i < L.length; i++) {
        if (!isFinite(L[i])) { bad++; continue; }
        const v = Math.abs(L[i]);
        if (v > peak) peak = v;
        s2 += L[i] * L[i];
        diff += Math.abs(L[i] - R[i]);
        hf += Math.abs(L[i] - L[i - 1]);
      }
      const n = L.length - a;
      const rms = Math.sqrt(s2 / n);
      return { peak: peak, rms: rms, bad: bad,
               /* Two stereo measures, because they answer different questions.
                  `stereo` is channel difference relative to level, which is
                  right for "is this part wide". `spread` is the raw difference,
                  which is right for "does this move across the field" — an
                  effect that pans *and* raises the level can lower the first
                  while plainly increasing the second. */
               spread: diff / n,
               stereo: rms > 0 ? (diff / n) / rms : 0,
               bright: rms > 0 ? (hf / n) / rms : 0 };
    }
    function song(setup) {
      const s = window.Composer.compose({ seed: 'SWIRL-1', genre: 'cinematic',
                                          meter: '4/4', length: 'short' });
      /* A deliberately mono sound. The pads here split themselves hard left and
         right, so a rotary's panning would be measured against a part that is
         already as wide as it can get — the same trap the chorus test fell
         into. */
      s.presetOverride = { pad: 'chipChord' };
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t < 32; });
      });
      s.totalBeats = 34;
      s.glue = 0;
      if (setup) setup(s);
      return s;
    }
    function mixWith(fields) {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: t !== 'pad', solo: false, rev: 0, del: 0, cho: 0,
                 mod: 0, autopan: 0, eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0, comp: 0, punch: 0 };
      });
      if (fields) for (const k in fields) m.pad[k] = fields[k];
      return m;
    }
    const out = { off: stats(await window.Engine.renderOffline(song(), mixWith())) };
    for (const kind of ['flanger', 'phaser', 'rotary']) {
      out[kind] = stats(await window.Engine.renderOffline(
        song(function (s) { s.modFx = kind; }), mixWith({ mod: 1 })));
    }
    out.swept = stats(await window.Engine.renderOffline(song(), mixWith({ autopan: 1 })));
    return out;
  });

  check(swirl.off.rms > 1e-4, 'there is a part to modulate (rms ' + swirl.off.rms.toFixed(4) + ')');
  ['flanger', 'phaser', 'rotary'].forEach(function (k) {
    check(swirl[k].bad === 0, k + ': never produces broken samples');
    check(swirl[k].peak < 2, k + ': never runs away (peak ' + swirl[k].peak.toFixed(2) + ')');
    check(swirl[k].rms > swirl.off.rms * 1.05,
      k + ': actually adds something (rms ' + swirl.off.rms.toFixed(4) + ' → ' +
      swirl[k].rms.toFixed(4) + ')');
  });
  /* Each of the three should differ from the others, or two of them are the
     same effect wearing different names. */
  const sig = ['flanger', 'phaser', 'rotary'].map(function (k) {
    return Math.round(swirl[k].bright * 1000) + '/' + Math.round(swirl[k].stereo * 100);
  });
  check(new Set(sig).size === 3, 'and all three are distinguishable (' + sig.join('  ') + ')');
  check(swirl.rotary.spread > swirl.off.spread * 1.3,
    'the rotary moves across the stereo field, as a spinning horn does (' +
    swirl.off.spread.toFixed(5) + ' → ' + swirl.rotary.spread.toFixed(5) + ')');
  check(swirl.swept.spread > swirl.off.spread * 1.3,
    'and auto-pan sweeps the part (' + swirl.off.spread.toFixed(5) + ' → ' +
    swirl.swept.spread.toFixed(5) + ')');

  console.log('\n— colour —');
  /* Ring, fold and wah are the three "ruin it on purpose" tones. They share one
     slider, so the two things that matter are that the slider is genuinely
     silent at zero and that each kind does something different at the top. */
  const colour = await page.evaluate(async function () {
    /* `change` is the headline measure here: how much of the part the effect
       actually rewrote, as a fraction of the untouched part's level. Brightness
       alone is the wrong lens for a ring modulator — it shifts every partial by
       a fixed number of hertz, so a tone moved from 500 Hz to 20 and 980 reads
       as barely brighter on average while sounding nothing like the original. */
    function stats(buf, dryData) {
      const L = buf.getChannelData(0);
      let peak = 0, s2 = 0, bad = 0, hf = 0, d2 = 0;
      for (let i = 1; i < L.length; i++) {
        if (!isFinite(L[i])) { bad++; continue; }
        const v = Math.abs(L[i]);
        if (v > peak) peak = v;
        s2 += L[i] * L[i];
        hf += Math.abs(L[i] - L[i - 1]);
        if (dryData) { const d = L[i] - dryData[i]; d2 += d * d; }
      }
      const n = L.length - 1;
      const rms = Math.sqrt(s2 / n);
      return { peak: peak, rms: rms, bad: bad,
               bright: rms > 0 ? (hf / n) / rms : 0,
               change: dryData ? Math.sqrt(d2 / n) : 0 };
    }
    function song(kind) {
      /* A flute: nearly a sine, so any harmonics in the result were put there
         by the effect rather than being in the instrument already. */
      const s = window.Composer.compose({ seed: 'COLOUR-1', genre: 'ambient',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = { lead: 'flute' };
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t < 32; });
      });
      s.totalBeats = 34;
      s.glue = 0;
      if (kind) s.colourFx = kind;
      return s;
    }
    function mixWith(amt) {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: t !== 'lead', solo: false, rev: 0, del: 0, cho: 0,
                 mod: 0, autopan: 0, colour: 0, eqLow: 0, eqMid: 0, eqHigh: 0,
                 crush: 0, comp: 0, punch: 0 };
      });
      m.lead.colour = amt;
      return m;
    }
    const dryBuf = await window.Engine.renderOffline(song(), mixWith(0));
    const dryData = dryBuf.getChannelData(0);
    const out = { off: stats(dryBuf) };
    /* The same render again, to find out how close "identical" can actually
       get. Chromium's offline renderer is not bit-exact run to run — two
       renders of the same graph differ in the last decimal place or so — so
       "unchanged" has to mean "no further from the reference than the
       reference is from itself", not "exactly equal". */
    out.floor = stats(await window.Engine.renderOffline(song(), mixWith(0)), dryData);
    for (const kind of ['ring', 'fold', 'wah']) {
      out[kind] = stats(await window.Engine.renderOffline(song(kind), mixWith(1)), dryData);
      out[kind + 'Zero'] = stats(await window.Engine.renderOffline(song(kind), mixWith(0)), dryData);
    }
    return out;
  });

  check(colour.off.rms > 1e-4, 'there is a part to colour (rms ' + colour.off.rms.toFixed(4) + ')');
  ['ring', 'fold', 'wah'].forEach(function (k) {
    check(colour[k].bad === 0, k + ': never produces broken samples');
    check(colour[k].peak < 2, k + ': never runs away (peak ' + colour[k].peak.toFixed(2) + ')');
    /* At zero the effect is built but blended out, so it must come back
       sample-for-sample identical to the untouched part — a colour that leaks
       at zero is a colour you can never turn off. */
    check(colour[k + 'Zero'].change <= Math.max(colour.floor.change * 4, 1e-9),
      k + ': silent at zero (difference from dry ' +
      colour[k + 'Zero'].change.toExponential(1) + ', renderer floor ' +
      colour.floor.change.toExponential(1) + ')');
    check(colour[k].change > colour.off.rms * 0.4,
      k + ': rewrites the part at full (changed ' +
      (colour[k].change / colour.off.rms * 100).toFixed(0) + '% of its level)');
  });
  /* Each kind leaves a different fingerprint, and each fingerprint is the one
     its own maths predicts. Multiplying a signal by a full-swing sine is what
     ring modulation *is*, and that halves the power — so an rms of exactly
     1/√2 is the proof the modulator is running rather than merely connected. */
  check(Math.abs(colour.ring.rms / colour.off.rms - 0.707) < 0.03,
    'ring modulation drops the level by √2, as multiplying by a tone must (' +
    (colour.ring.rms / colour.off.rms).toFixed(3) + ')');
  check(colour.fold.bright > colour.off.bright * 1.5,
    'folding adds harmonics that were never there (' + colour.off.bright.toFixed(4) +
    ' → ' + colour.fold.bright.toFixed(4) + ')');
  check(colour.wah.bright < colour.off.bright && colour.wah.rms < colour.off.rms * 0.5,
    'and the wah narrows the part to a band instead (brightness ' +
    colour.off.bright.toFixed(4) + ' → ' + colour.wah.bright.toFixed(4) + ')');
  const csig = ['ring', 'fold', 'wah'].map(function (k) {
    return Math.round(colour[k].bright * 1000) + '/' + Math.round(colour[k].rms * 10000);
  });
  check(new Set(csig).size === 3, 'all three colours are distinguishable (' + csig.join('  ') + ')');

  console.log('\n— pump depth and the rhythmic gate —');
  /* Both of these are movement in the level over time, so both are measured
     the same way: cut the render into 10 ms blocks and look at what the
     envelope does. How deep the troughs go says how hard the effect bites; how
     often it crosses its own midpoint says how fast it is doing it. A loudness
     reading averaged over the whole window can see neither. */
  const dyn = await page.evaluate(async function () {
    function env(buf, fromSec, toSec) {
      const L = buf.getChannelData(0);
      const sr = buf.sampleRate;
      const a = Math.max(0, Math.floor(fromSec * sr));
      const b = Math.min(L.length, Math.floor(toSec * sr));
      const blk = Math.floor(sr * 0.01);
      const raw = [];
      let bad = 0, s2 = 0;
      for (let i = a; i + blk < b; i += blk) {
        let t2 = 0;
        for (let j = i; j < i + blk; j++) {
          if (!isFinite(L[j])) { bad++; continue; }
          t2 += L[j] * L[j];
        }
        raw.push(Math.sqrt(t2 / blk));
      }
      for (let i = a; i < b; i++) s2 += L[i] * L[i];
      const sorted = raw.slice().sort(function (x, y) { return x - y; });
      const q = function (f) { return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * f))] || 0; };
      const lo = q(0.1), hi = q(0.9), mid = (lo + hi) * 0.5;
      let cross = 0, up = raw[0] > mid;
      for (let i = 1; i < raw.length; i++) {
        const nowUp = raw[i] > mid;
        if (nowUp !== up) { cross++; up = nowUp; }
      }
      return { rms: Math.sqrt(s2 / Math.max(1, b - a)), lo: lo, hi: hi, bad: bad,
               rate: cross / (toSec - fromSec) };
    }
    /* A held pad over a kick on every beat. The drums fader is muted but the
       kick events stay in the score, so the part still ducks where the kick
       lands — measured with the kick audible, the kick fills its own hole and
       the pump reads as doing nothing at all. */
    function song(setup) {
      const s = window.Composer.compose({ seed: 'DYN-1', genre: 'cinematic',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = {};
      Object.keys(s.tracks).forEach(function (k) { s.tracks[k] = []; });
      for (let b = 0; b < 16; b += 4) s.tracks.pad.push({ t: b, d: 4, p: 60, v: 0.9 });
      for (let b = 0; b < 16; b += 1) {
        s.tracks.drums.push({ t: b, d: 0.25, p: 36, v: 1, inst: 'kick' });
      }
      s.totalBeats = 16;
      s.glue = 0;
      s.sidechain = 0;
      if (setup) setup(s);
      return s;
    }
    function mix(fields) {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: t !== 'pad', solo: false, rev: 0, del: 0, cho: 0,
                 mod: 0, autopan: 0, colour: 0, chop: 0, eqLow: 0, eqMid: 0,
                 eqHigh: 0, crush: 0, comp: 0, punch: 0 };
      });
      if (fields) for (const k in fields) m.pad[k] = fields[k];
      return m;
    }
    const W = [1.0, 6.0];
    const run = async function (setup, fields) {
      return env(await window.Engine.renderOffline(song(setup), mix(fields)), W[0], W[1]);
    };
    return {
      flat: await run(),
      /* The same render again: Chromium's offline renderer is not bit-exact
         run to run, so "left alone" has to be measured against how far a
         render sits from an identical one, not against zero. */
      floor: await run(),
      pumpLight: await run(function (s) { s.sidechain = 0.3; }),
      pumpHard: await run(function (s) { s.sidechain = 0.7; }),
      slowBack: await run(function (s) { s.sidechain = 0.7; s.duckSpeed = 0; }),
      fastBack: await run(function (s) { s.sidechain = 0.7; s.duckSpeed = 1; }),
      chopOff: await run(null, { chop: 0 }),
      chopQ: await run(function (s) { s.chopRate = 1; }, { chop: 1 }),
      chopE: await run(function (s) { s.chopRate = 2; }, { chop: 1 }),
      chopS: await run(function (s) { s.chopRate = 4; }, { chop: 1 }),
      chopHalf: await run(null, { chop: 0.5 })
    };
  });

  check(dyn.flat.rms > 1e-3 && dyn.flat.bad === 0,
    'there is a steady part to pump (rms ' + dyn.flat.rms.toFixed(4) + ')');
  /* A deeper pump digs deeper troughs. Measured at the tenth percentile of the
     envelope — the quiet moments — because the loud ones barely move. */
  check(dyn.pumpLight.lo < dyn.flat.lo && dyn.pumpHard.lo < dyn.pumpLight.lo,
    'a harder pump digs deeper troughs (' + dyn.flat.lo.toFixed(4) + ' → ' +
    dyn.pumpLight.lo.toFixed(4) + ' → ' + dyn.pumpHard.lo.toFixed(4) + ')');
  check(dyn.pumpHard.lo < dyn.flat.lo * 0.8,
    'and the deepest setting is plainly audible, not a trim');
  /* A slow recovery holds the part down longer, so less of it gets through. */
  check(dyn.slowBack.rms < dyn.fastBack.rms * 0.9,
    'a slow pump holds the part down longer than a fast one (' +
    dyn.fastBack.rms.toFixed(4) + ' → ' + dyn.slowBack.rms.toFixed(4) + ')');

  const renderFloor = Math.abs(dyn.floor.rms - dyn.flat.rms);
  check(Math.abs(dyn.chopOff.rms - dyn.flat.rms) <= Math.max(renderFloor * 4, dyn.flat.rms * 1e-6),
    'chop at zero leaves the part alone (off by ' +
    Math.abs(dyn.chopOff.rms - dyn.flat.rms).toExponential(1) + ', renderer floor ' +
    renderFloor.toExponential(1) + ')');
  check(dyn.chopE.lo < dyn.flat.lo * 0.1 && dyn.chopE.bad === 0,
    'chop at full cuts the part to silence between steps (' +
    dyn.flat.lo.toFixed(4) + ' → ' + dyn.chopE.lo.toFixed(4) + ')');
  check(dyn.chopHalf.lo > dyn.chopE.lo && dyn.chopHalf.lo < dyn.flat.lo * 0.8,
    'and half way is half way (' + dyn.chopHalf.lo.toFixed(4) + ')');
  /* Each rate should open the gate twice as often as the one before, which is
     what makes them three rates rather than three names for one. */
  check(dyn.chopE.rate > dyn.chopQ.rate * 1.7 && dyn.chopS.rate > dyn.chopE.rate * 1.7,
    'each chop rate is twice the one before (' + dyn.chopQ.rate.toFixed(1) + ' → ' +
    dyn.chopE.rate.toFixed(1) + ' → ' + dyn.chopS.rate.toFixed(1) + ' per second)');

  console.log('\n— split glue —');
  /* Three compressors across three bands instead of one across everything.
     Two things have to be true: the split has to put the mix back together
     *exactly* as it found it, and it has to actually stop the kick pulling the
     rest of the mix down with it. The first is the one that can go silently
     wrong, so it is measured band by band rather than on the total. */
  const glue = await page.evaluate(async function () {
    /* One-pole filters, applied in JS to the finished render, so the
       measurement does not depend on the same filter recipe it is checking. */
    function lowpass(d, rate, fc) {
      const a = 1 / (1 + 1 / (2 * Math.PI * fc / rate));
      const y = new Float32Array(d.length);
      for (let i = 1; i < d.length; i++) y[i] = y[i - 1] + a * (d[i] - y[i - 1]);
      return y;
    }
    function highpass(d, rate, fc) {
      const rc = 1 / (2 * Math.PI * fc);
      const a = rc / (rc + 1 / rate);
      const y = new Float32Array(d.length);
      for (let i = 1; i < d.length; i++) y[i] = a * (y[i - 1] + d[i] - d[i - 1]);
      return y;
    }
    function rmsOf(d, from, to) {
      let s2 = 0, n = 0;
      for (let i = from; i < to && i < d.length; i++) { s2 += d[i] * d[i]; n++; }
      return Math.sqrt(s2 / Math.max(1, n));
    }
    function song(setup) {
      const s = window.Composer.compose({ seed: 'GLUE-2', genre: 'house', length: 'short' });
      s.presetOverride = {};
      Object.keys(s.tracks).forEach(function (k) { s.tracks[k] = []; });
      for (let b = 0; b < 16; b += 1) {
        s.tracks.drums.push({ t: b, d: 0.25, p: 36, v: 1, inst: 'kick' });
        /* Hats on the off-sixteenths only, so none lands on a kick and gets
           masked by it. */
        [0.25, 0.5, 0.75].forEach(function (o) {
          s.tracks.drums.push({ t: b + o, d: 0.25, p: 42, v: 0.55, inst: 'hat' });
        });
      }
      /* A sustained low note under it all: most of a mix's energy is in the
         bottom, and it is that energy a single compressor reacts to. */
      for (let b = 0; b < 16; b += 2) s.tracks.bass.push({ t: b, d: 2, p: 36, v: 1 });
      s.totalBeats = 16;
      s.sidechain = 0;
      if (setup) setup(s);
      return s;
    }
    function mix() {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: !(t === 'drums' || t === 'bass'), solo: false,
                 rev: 0, del: 0, cho: 0, mod: 0, autopan: 0, colour: 0, chop: 0,
                 eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0, comp: 0, punch: 0 };
      });
      return m;
    }
    async function measure(multi, amt) {
      const s = song(function (x) { x.glue = amt; x.glueMulti = multi; });
      const buf = await window.Engine.renderOffline(s, mix());
      const rate = buf.sampleRate;
      const L = buf.getChannelData(0);
      let peak = 0, bad = 0;
      for (let i = 0; i < L.length; i++) {
        if (!isFinite(L[i])) { bad++; continue; }
        const v = Math.abs(L[i]); if (v > peak) peak = v;
      }
      // Three bands of the finished mix, on the same splits the glue uses.
      const low = lowpass(L, rate, 180);
      const above = highpass(L, rate, 180);
      const mid = lowpass(above, rate, 2600);
      const high = highpass(above, rate, 2600);
      const n = L.length;
      const bands = [rmsOf(low, 0, n), rmsOf(mid, 0, n), rmsOf(high, 0, n)];

      /* How far the hat a sixteenth after the kick sits below the hat three
         sixteenths after it, by which point a compressor has let go. */
      const hp = highpass(L, rate, 4000);
      const spb = 60 / s.bpm, offset = 0.05;
      let near = 0, far = 0, k = 0;
      for (let i = 1; i < 15; i++) {
        const t = i * spb + offset;
        const a = rmsOf(hp, Math.floor((t + 0.25 * spb) * rate),
                            Math.floor((t + 0.25 * spb + 0.035) * rate));
        const b = rmsOf(hp, Math.floor((t + 0.75 * spb) * rate),
                            Math.floor((t + 0.75 * spb + 0.035) * rate));
        if (b > 1e-6) { near += a; far += b; k++; }
      }
      return { rms: rmsOf(L, 0, n), peak: peak, bad: bad, bands: bands,
               hold: k ? near / far : 1 };
    }
    return {
      none: await measure(false, 0),
      splitZero: await measure(true, 0),
      single: await measure(false, 0.9),
      split: await measure(true, 0.9)
    };
  });

  ['splitZero', 'split'].forEach(function (k) {
    check(glue[k].bad === 0, k + ': never produces broken samples');
    check(glue[k].peak < 1, k + ': never runs away (peak ' + glue[k].peak.toFixed(3) + ')');
  });
  /* The split has to be invisible when nothing is being compressed. A
     crossover that does not sum flat leaves a notch or a bump at the split
     frequencies, and nothing else in the app would ever complain about it —
     built with the wrong filter Q this read +7.5 dB at both splits and every
     band came back 47% too loud. */
  ['bass', 'middle', 'treble'].forEach(function (label, i) {
    const ref = glue.none.bands[i], got = glue.splitZero.bands[i];
    const db = 20 * Math.log10(got / ref);
    check(Math.abs(db) < 1,
      'splitting and rejoining leaves the ' + label + ' where it was (' +
      (db >= 0 ? '+' : '') + db.toFixed(2) + ' dB)');
  });
  /* And the point of the whole thing: with one compressor the kick drags the
     hats down with it, and with three it mostly does not. */
  const bias = function (k) { return 1 - glue[k].hold / glue.none.hold; };
  check(bias('single') > 0.03,
    'one compressor lets the kick drag the hats down (' +
    (bias('single') * 100).toFixed(1) + '%)');
  check(bias('split') < bias('single') * 0.85,
    'and splitting the bands holds them up better (' +
    (bias('split') * 100).toFixed(1) + '% against ' +
    (bias('single') * 100).toFixed(1) + '%)');
  check(glue.split.rms > glue.single.rms,
    'with more level for the same squeeze (' + glue.single.rms.toFixed(4) +
    ' → ' + glue.split.rms.toFixed(4) + ')');

  console.log('\n— compression —');
  /* Compression is the one effect that can quietly ruin everything. This chain
     has already been flattened once by a compressor handing back the gain it
     took, so every claim here is checked against the crest factor — peak over
     RMS, the actual measure of how much dynamic range is left. */
  const comp = await page.evaluate(async function () {
    function crest(buf) {
      const ch = buf.getChannelData(0);
      let peak = 0, sum = 0;
      const start = Math.floor(buf.sampleRate * 0.2);
      for (let i = start; i < ch.length; i++) {
        const a = Math.abs(ch[i]);
        if (a > peak) peak = a;
        sum += a * a;
      }
      const rms = Math.sqrt(sum / (ch.length - start));
      return { peak: peak, rms: rms, crest: rms > 0 ? peak / rms : 0 };
    }
    function song(setup) {
      const s = window.Composer.compose({ seed: 'COMP-1', genre: 'rock',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = {};
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t >= 32 && e.t < 64; })
          .map(function (e) { const c = {}; for (const f in e) c[f] = e[f]; c.t = e.t - 32; return c; });
      });
      s.totalBeats = 32;
      if (setup) setup(s);
      return s;
    }
    function mixWith(fields) {
      const m = {};
      window.Engine.TRACKS.forEach(function (t) {
        m[t] = { volume: 1, muted: false, solo: false, rev: 1, del: 1, cho: 0,
                 eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0, comp: 0, punch: 0 };
        if (fields) for (const k in fields) m[t][k] = fields[k];
      });
      return m;
    }
    const none = await window.Engine.renderOffline(song(function (s) { s.glue = 0; }), mixWith());
    const glued = await window.Engine.renderOffline(song(function (s) { s.glue = 1; }), mixWith());
    const squeezed = await window.Engine.renderOffline(song(function (s) { s.glue = 0; }),
      mixWith({ comp: 1 }));
    const punchy = await window.Engine.renderOffline(song(function (s) { s.glue = 0; }),
      mixWith({ punch: 1 }));
    return { none: crest(none), glued: crest(glued), squeezed: crest(squeezed), punchy: crest(punchy) };
  });

  check(comp.none.crest > 3, 'the uncompressed mix has real dynamics (crest ' +
    comp.none.crest.toFixed(1) + ')');
  check(comp.squeezed.crest < comp.none.crest * 0.9,
    'squeezing a part evens it out (crest ' + comp.none.crest.toFixed(1) + ' → ' +
    comp.squeezed.crest.toFixed(1) + ')');
  check(comp.squeezed.crest > 1.8,
    'but does not flatten it into a brick wall (crest ' + comp.squeezed.crest.toFixed(1) + ')');
  check(comp.glued.crest < comp.none.crest,
    'glue tightens the whole mix (crest ' + comp.glued.crest.toFixed(1) + ')');
  check(comp.glued.crest > comp.none.crest * 0.6,
    'gently — glue is not a mastering limiter (' +
    (comp.glued.crest / comp.none.crest).toFixed(2) + ' of the original range)');
  check(comp.glued.peak <= 1.0001 && comp.squeezed.peak <= 1.0001 && comp.punchy.peak <= 1.0001,
    'and nothing compressed ever leaves full scale');
  check(comp.punchy.crest >= comp.squeezed.crest,
    'turning the attack up keeps more transient than squeezing does (' +
    comp.punchy.crest.toFixed(1) + ' vs ' + comp.squeezed.crest.toFixed(1) + ')');

  console.log('\n— master tone and imaging —');
  /* Width and mono bass are the two that can silently destroy a mix rather
     than merely change it: width 0 must not collapse to silence, and mono bass
     must fix the low end without narrowing everything above it. */
  const mst = await page.evaluate(async function () {
    function bands(buf) {
      const L = buf.getChannelData(0);
      const R = buf.numberOfChannels > 1 ? buf.getChannelData(1) : L;
      let s2 = 0, diff = 0, peak = 0, bad = 0;
      // A crude low/high split: a running average is a lowpass.
      let lp = 0, lo = 0, hi = 0, sideLow = 0, sideLp = 0, bright = 0;
      for (let i = 0; i < L.length; i++) {
        if (!isFinite(L[i])) { bad++; continue; }
        const v = Math.abs(L[i]);
        if (v > peak) peak = v;
        s2 += L[i] * L[i];
        diff += Math.abs(L[i] - R[i]);
        if (i > 0) bright += Math.abs(L[i] - L[i - 1]);
        lp += (L[i] - lp) * 0.02;
        lo += lp * lp;
        hi += (L[i] - lp) * (L[i] - lp);
        /* The side signal's low end — the exact thing mono bass removes, and
           the only measurement that can tell whether it did. Overall channel
           difference cannot: most of it lives above the crossover. */
        const sd = (L[i] - R[i]) / 2;
        sideLp += (sd - sideLp) * 0.02;
        sideLow += sideLp * sideLp;
      }
      const n = L.length;
      const rms = Math.sqrt(s2 / n);
      return { rms: rms, spread: diff / n, peak: peak, bad: bad,
               lowRms: Math.sqrt(lo / n), highRms: Math.sqrt(hi / n),
               sideLow: Math.sqrt(sideLow / n),
               bright: rms > 0 ? (bright / n) / rms : 0 };
    }
    function make(setup) {
      const s = window.Composer.compose({ seed: 'MASTER-1', genre: 'house',
                                          meter: '4/4', length: 'short' });
      s.presetOverride = {};
      s.glue = 0;
      Object.keys(s.tracks).forEach(function (k) {
        s.tracks[k] = s.tracks[k].filter(function (e) { return e.t >= 32 && e.t < 64; })
          .map(function (e) { const c = {}; for (const f in e) c[f] = e[f]; c.t = e.t - 32; return c; });
      });
      s.totalBeats = 32;
      if (setup) setup(s);
      return s;
    }
    const mix = {};
    window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
    const r = {};
    r.flat = bands(await window.Engine.renderOffline(make(), mix));
    r.wide = bands(await window.Engine.renderOffline(make(function (s) { s.width = 2; }), mix));
    r.narrow = bands(await window.Engine.renderOffline(make(function (s) { s.width = 0; }), mix));
    r.mono = bands(await window.Engine.renderOffline(make(function (s) { s.monoBass = 1; }), mix));
    r.bassUp = bands(await window.Engine.renderOffline(make(function (s) { s.mEqLow = 9; }), mix));
    r.trebleUp = bands(await window.Engine.renderOffline(make(function (s) { s.mEqHigh = 9; }), mix));
    return r;
  });

  check(mst.flat.rms > 0.02, 'there is a mix to shape (rms ' + mst.flat.rms.toFixed(3) + ')');
  ['wide', 'narrow', 'mono', 'bassUp', 'trebleUp'].forEach(function (k) {
    check(mst[k].bad === 0 && mst[k].peak <= 1.0001,
      k + ': stays finite and inside full scale (peak ' + mst[k].peak.toFixed(3) + ')');
  });
  check(mst.wide.spread > mst.flat.spread * 1.3,
    'width widens (' + mst.flat.spread.toFixed(4) + ' → ' + mst.wide.spread.toFixed(4) + ')');
  check(mst.narrow.spread < mst.flat.spread * 0.3,
    'and collapses to the centre at zero (' + mst.narrow.spread.toFixed(5) + ')');
  check(mst.narrow.rms > mst.flat.rms * 0.5,
    'without collapsing to silence — a narrow mix is still a mix (rms ' +
    mst.narrow.rms.toFixed(3) + ')');
  check(mst.mono.highRms > mst.flat.highRms * 0.95,
    'while leaving everything above it alone (' + mst.flat.highRms.toFixed(4) +
    ' → ' + mst.mono.highRms.toFixed(4) + ')');
  check(mst.mono.spread > mst.flat.spread * 0.7,
    'and without narrowing the mix as a whole (' + mst.flat.spread.toFixed(4) +
    ' → ' + mst.mono.spread.toFixed(4) + ')');
  check(mst.bassUp.lowRms > mst.flat.lowRms * 1.15,
    'master bass lifts the low end (' + mst.flat.lowRms.toFixed(4) + ' → ' +
    mst.bassUp.lowRms.toFixed(4) + ')');
  /* Brightness, not the whole band above 140 Hz — a 4 kHz shelf barely moves
     that, because almost all of a mix lives below 4 kHz. */
  check(mst.trebleUp.bright > mst.flat.bright * 1.1,
    'and master treble lifts the top (' + mst.flat.bright.toFixed(4) + ' → ' +
    mst.trebleUp.bright.toFixed(4) + ')');

  /* Mono bass is checked on a signal built to isolate it rather than on a
     finished mix. In a real mix the low band's stereo content is already tiny
     and a gentle measuring filter leaks more from above it than the band itself
     contains — which is a limit of the measurement, not of the effect. A 60 Hz
     tone hard left against a 3 kHz tone hard right has its stereo content
     entirely in the low band, so what happens to it is unambiguous. */
  const monoProof = await page.evaluate(async function () {
    async function run(amount) {
      const ctx = new OfflineAudioContext(2, 44100, 44100);
      const lo = ctx.createOscillator(); lo.frequency.value = 60;
      const hi = ctx.createOscillator(); hi.frequency.value = 3000;
      const pl = ctx.createStereoPanner(); pl.pan.value = -1;
      const pr = ctx.createStereoPanner(); pr.pan.value = 1;
      const head = ctx.createGain();
      lo.connect(pl).connect(head);
      hi.connect(pr).connect(head);
      const song = window.Composer.compose({ seed: 'MONO', genre: 'house', length: 'short' });
      song.monoBass = amount;
      song.tracks = { drums: [], bass: [], chords: [], arp: [], lead: [], counter: [], pad: [] };
      song.totalBeats = 4;
      const mix = {};
      window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
      const graph = window.Engine.buildGraph(ctx, song, mix, false, 1);
      head.connect(graph.master);
      lo.start(0); hi.start(0); lo.stop(1); hi.stop(1);
      const buf = await ctx.startRendering();
      const L = buf.getChannelData(0), R = buf.getChannelData(1);
      let lp = 0, low = 0, hiE = 0, hp = 0;
      for (let i = 0; i < L.length; i++) {
        const sd = (L[i] - R[i]) / 2;
        lp += (sd - lp) * 0.02;
        low += lp * lp;
        hp = sd - lp;
        hiE += hp * hp;
      }
      return { sideLow: Math.sqrt(low / L.length), sideHigh: Math.sqrt(hiE / L.length) };
    }
    return { off: await run(0), on: await run(1) };
  });
  check(monoProof.off.sideLow > 0.01,
    'the mono-bass probe really does have stereo bass in it (' +
    monoProof.off.sideLow.toFixed(4) + ')');
  check(monoProof.on.sideLow < monoProof.off.sideLow * 0.6,
    'mono bass takes the stereo out of the low end (' + monoProof.off.sideLow.toFixed(4) +
    ' → ' + monoProof.on.sideLow.toFixed(4) + ')');
  check(monoProof.on.sideHigh > monoProof.off.sideHigh * 0.85,
    'and leaves the stereo above it alone (' + monoProof.off.sideHigh.toFixed(4) +
    ' → ' + monoProof.on.sideHigh.toFixed(4) + ')');

  console.log('\n— exports —');
  const ex = await page.evaluate(async function () {
    const song = window.Composer.compose({ seed: 'EXPORT-1', genre: 'house', length: 'short' });
    const full = window.Exporter.buildMidi(song);   // MIDI covers the whole song
    song.tracks.pad = song.tracks.pad.filter(function (e) { return e.t < 32; });
    ['drums', 'bass', 'chords', 'arp', 'lead'].forEach(function (k) {
      song.tracks[k] = song.tracks[k].filter(function (e) { return e.t < 32; });
    });
    song.totalBeats = 32;
    const mix = {};
    window.Engine.TRACKS.forEach(function (t) { mix[t] = { volume: 1, muted: false }; });
    const buf = await window.Engine.renderOffline(song, mix);
    const wav = window.Exporter.encodeWav(buf);
    const head = new Uint8Array(await wav.slice(0, 12).arrayBuffer());
    const mhead = new Uint8Array(await full.slice(0, 4).arrayBuffer());
    return {
      wavSize: wav.size,
      wavTag: String.fromCharCode.apply(null, head.slice(0, 4)) +
              String.fromCharCode.apply(null, head.slice(8, 12)),
      midiSize: full.size,
      midiTag: String.fromCharCode.apply(null, mhead)
    };
  });
  check(ex.wavTag === 'RIFFWAVE', 'wav header is RIFF/WAVE');
  check(ex.wavSize > 44 + 44100 * 4 * 5, 'wav is a real length (' + (ex.wavSize / 1048576).toFixed(1) + ' MB)');
  check(ex.midiTag === 'MThd', 'midi header is MThd');
  check(ex.midiSize > 500, 'midi has content (' + ex.midiSize + ' bytes)');

  console.log('\n— interaction —');
  await page.click('#mixer .track:nth-child(1) .mute-btn');
  await page.waitForTimeout(150);
  check(await page.evaluate(function () {
    return document.querySelector('#mixer .track .mute-btn').classList.contains('off');
  }), 'mute toggles');
  await page.click('#mixer .track:nth-child(5) .reroll');
  await page.waitForTimeout(200);
  check(await page.evaluate(function () {
    return document.getElementById('statusLine').textContent.indexOf('New') === 0;
  }), 're-roll rewrites a single part');
  await page.click('#playBtn');
  await page.waitForTimeout(150);
  check(await page.evaluate(function () {
    return document.getElementById('playIcon').textContent;
  }) === '▶', 'pause works');

  const hash = await page.evaluate(function () { return location.hash; });
  check(/seed=/.test(hash), 'url carries the seed');
  await page.goto(base + hash, { waitUntil: 'networkidle' });
  await page.waitForTimeout(700);
  check(await page.locator('#songPanel').isVisible(), 'a shared link restores the song');

  console.log('\n— phone viewport —');
  const phone = await browser.newPage({ viewport: { width: 390, height: 780 }, isMobile: true, hasTouch: true });
  watch(phone, 'phone ');
  await phone.goto(base, { waitUntil: 'networkidle' });
  await phone.click('#generateBtn');
  await phone.waitForTimeout(700);
  const overflow = await phone.evaluate(function () {
    return document.documentElement.scrollWidth - document.documentElement.clientWidth;
  });
  check(overflow <= 1, 'no sideways scrolling at 390px (' + overflow + 'px)');
  /* The tap check can only measure what is on screen, so open the collapsed
     panels first — controls hidden inside a <details> are exactly the ones
     that quietly ship too small to hit. */
  await phone.evaluate(function () {
    document.querySelectorAll('details').forEach(function (d) { d.open = true; });
  });
  await phone.waitForTimeout(150);
  const overflowOpen = await phone.evaluate(function () {
    return document.documentElement.scrollWidth - document.documentElement.clientWidth;
  });
  check(overflowOpen <= 1, 'still none with every panel open (' + overflowOpen + 'px)');
  const tooSmall = await phone.evaluate(function () {
    const bad = [];
    document.querySelectorAll('button, select, input[type=range]').forEach(function (b) {
      if (b.offsetParent === null) return;
      const r = b.getBoundingClientRect();
      if (r.height < 28 || r.width < 28) bad.push((b.id || b.className) + ' ' + Math.round(r.width) + 'x' + Math.round(r.height));
    });
    return bad;
  });
  check(tooSmall.length === 0, 'every control is big enough to tap' +
    (tooSmall.length ? ': ' + tooSmall.join(', ') : ''));

  console.log('\n— console —');
  check(errors.length === 0, 'no page errors' +
    (errors.length ? ':\n    ' + errors.slice(0, 8).join('\n    ') : ''));
  if (offlineFonts.length) {
    console.log('  note   web fonts unreachable here (' + offlineFonts.length +
      ' request(s)); the page fell back to its system stack');
  }

  await browser.close();
  server.close();

  console.log('\n' + (failures === 0 ? 'PASS' : failures + ' FAILURES'));
  process.exit(failures === 0 ? 0 : 1);
})().catch(function (e) {
  console.error(e);
  server.close();
  process.exit(1);
});
