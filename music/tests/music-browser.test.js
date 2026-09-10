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
  check(await page.locator('#genreChips .chip').count() === 8, 'eight genre chips rendered');
  check(await page.locator('#moodChips .chip').count() === 5, 'five mood chips rendered');
  check(await page.locator('#songPanel').isHidden(), 'song panel hidden before generating');
  check(await page.locator('#helpModal').isHidden(), 'help modal is not covering the page');

  console.log('\n— generate —');
  await page.click('#generateBtn');
  await page.waitForTimeout(900);
  check(await page.locator('#songPanel').isVisible(), 'song panel appears');
  check((await page.locator('#songTitle').textContent()).trim().length > 2, 'song has a title');
  check(await page.locator('#mixer .track').count() === 6, 'six mixer tracks');
  check(await page.locator('.chord-cell').count() > 0, 'chord strip filled');
  check(await page.evaluate(function () {
    return document.getElementById('playIcon').textContent;
  }) === '❚❚', 'transport switched to playing');

  const moved = await playheadAdvances(page);
  check(moved.moved, 'playhead advances (' + moved.first + ' → ' + moved.last + ')');

  console.log('\n— every genre makes an audible sound —');
  const audio = await page.evaluate(async function () {
    /* Rendering whole songs for eight genres takes minutes. An eight-bar
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
