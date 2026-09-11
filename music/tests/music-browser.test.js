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
      const depth = song.genre.fx.sidechain;
      if (forceOff) {
        song.genre = Object.assign({}, song.genre, {
          fx: Object.assign({}, song.genre.fx, { sidechain: 0 })
        });
      }
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
       stereo width into the reading, which makes the number depend on which
       instrument the arrangement happened to pick. */
    const echoSong = function (ping) {
      return song(function (s) {
        s.pingpong = ping;
        s.presetOverride = { lead: 'pluck' };
      });
    };
    const echoWindow = function (buf, s) {
      let lastEnd = 0;
      s.tracks.lead.forEach(function (e) { lastEnd = Math.max(lastEnd, e.t + e.d); });
      const spb = 60 / s.bpm;
      const total = buf.length / buf.sampleRate;
      return [Math.min(0.95, (lastEnd * spb + 0.35) / total),
              Math.min(0.99, (lastEnd * spb + 2.2) / total)];
    };
    const offSong = echoSong(false), onSong = echoSong(true);
    const centred = await render(offSong, mixWith('lead', { rev: 0, del: 2, cho: 0 }));
    const bouncing = await render(onSong, mixWith('lead', { rev: 0, del: 2, cho: 0 }));
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

  check(fx.pingOff.rms > 1e-5, 'the echo test has echoes to move (rms ' +
    fx.pingOff.rms.toExponential(1) + ')');
  check(fx.pingOn.stereo > fx.pingOff.stereo * 1.5,
    'ping-pong throws the echoes to the sides (stereo ' +
    fx.pingOff.stereo.toFixed(3) + ' → ' + fx.pingOn.stereo.toFixed(3) + ')');

  check(fx.crushOff.rms > 1e-4, 'the crush test has a part to wreck');
  check(fx.crushOn.bright > fx.crushOff.bright * 1.5,
    'crushing adds grit that was not there (brightness ' +
    fx.crushOff.bright.toFixed(4) + ' → ' + fx.crushOn.bright.toFixed(4) + ')');

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
