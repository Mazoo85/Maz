/*
 * GOD'S EYE VIEW — logic tests.
 *
 *   node godview/tests/godview-logic.test.js
 *
 * No browser, no network, no dependencies: the feed parsers, the poller and
 * every layer are plain modules, so the promises that matter are checked here —
 *   · a broken or half-broken feed never throws past the parser, and never
 *     puts a marker at a made-up place;
 *   · a failing source backs off, says it is OFFLINE, and recovers by itself;
 *   · stopping a layer really stops it, even with a request still in flight.
 */
'use strict';

const path = require('path');
const fs = require('fs');

const F = require('../js/lib/format.js');
const P = require('../js/lib/poller.js');
const Q = require('../js/lib/quakes.js');
const QL = require('../js/layers/quakes-layer.js');
const SATLIB = require('../vendor/satellite.min.js');
const S = require('../js/lib/sats.js');
const SL = require('../js/layers/sats-layer.js');
const W = require('../js/lib/weather.js');
const WL = require('../js/layers/weather-layer.js');
const PL = require('../js/lib/planes.js');
const PLL = require('../js/layers/planes-layer.js');
const CAM = require('../js/lib/cameras.js');
const CAML = require('../js/layers/cameras-layer.js');

const ADSB = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'adsb-sample.json'), 'utf8'));
const OPENSKY = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'opensky-sample.json'), 'utf8'));
const TFL = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'tfl-jamcams-sample.json'), 'utf8'));
const DIGITRAFFIC = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'digitraffic-stations-sample.json'), 'utf8'));
const CALTRANS = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'caltrans-d4-sample.json'), 'utf8'));
const STATIONS_TLE = fs.readFileSync(path.join(__dirname, 'fixtures', 'celestrak-stations.txt'), 'utf8');
const VISUAL_TLE = fs.readFileSync(path.join(__dirname, 'fixtures', 'celestrak-visual.txt'), 'utf8');
/* Four days after the fixtures' epoch (17 Oct 2026 12:00 UTC). */
const SAT_TIME = Date.UTC(2026, 9, 21, 6, 0, 0);
const FIXTURE = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures', 'usgs-sample.json'), 'utf8'));

const tests = [];
function test(name, fn) { tests.push([name, fn]); }

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}
function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}
function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error((message || 'values differ') + ': got ' + actual + ', expected ~' + expected);
  }
}
function throws(fn, message) {
  try { fn(); } catch (e) { return; }
  throw new Error(message || 'expected an error');
}

/* Timers the test fires by hand, so a 60-second interval takes no time. */
function fakeTimers() {
  const queue = [];
  return {
    queue,
    set(fn, ms) { const h = { fn, ms }; queue.push(h); return h; },
    clear(h) { const i = queue.indexOf(h); if (i >= 0) queue.splice(i, 1); },
    fire() { const h = queue.shift(); h.fn(); return h.ms; }
  };
}
const flush = () => new Promise((r) => setImmediate(r));

/* ------------------------------------------------------------- format */
test('utcClock pads and labels UTC', () => {
  eq(F.utcClock(new Date(Date.UTC(2026, 9, 21, 4, 5, 9))), '04:05:09 UTC');
  eq(F.utcClock(new Date(Date.UTC(2026, 9, 21, 23, 59, 0))), '23:59:00 UTC');
});

test('count groups thousands', () => {
  eq(F.count(0), '0');
  eq(F.count(999), '999');
  eq(F.count(4812), '4,812');
  eq(F.count(1234567), '1,234,567');
});

test('ago reads like a person would say it', () => {
  eq(F.ago(5000), 'just now');
  eq(F.ago(-5000), 'just now');
  eq(F.ago(12 * 60000), '12 min ago');
  eq(F.ago(3 * 3600000), '3 h ago');
  eq(F.ago(72 * 3600000), '3 days ago');
});

test('rgba turns a hex colour and an alpha into css', () => {
  eq(F.rgba('#ff8a00', 0.5), 'rgba(255,138,0,0.5)');
  eq(F.rgba('#1aff8c', 1), 'rgba(26,255,140,1)');
});

/* ------------------------------------------------------------- quakes */
test('parse keeps only well-formed quakes, biggest first', () => {
  const list = Q.parse(FIXTURE);
  eq(list.length, 3, 'valid quakes');
  eq(list.map((q) => q.id).join(','), 'us0003,ak0001,ci0002');
  const top = list[0];
  eq(top.lat, -30.1);
  eq(top.lng, -71.9);
  eq(top.depthKm, 40);
  eq(top.mag, 6.1);
  eq(top.place, 'offshore Coquimbo, Chile');
  eq(top.time, 1792556400000);
  eq(top.url, 'https://earthquake.usgs.gov/earthquakes/eventpage/us0003');
});

test('parse rejects something that is not a feed', () => {
  throws(() => Q.parse({}), 'empty object');
  throws(() => Q.parse(null), 'null');
  throws(() => Q.parse({ features: 'nope' }), 'features not a list');
});

test('parse drops out-of-range coordinates and a null magnitude', () => {
  const bad = { features: [
    { id: 'a', properties: { mag: 2, time: 1 }, geometry: { coordinates: [200, 10, 1] } },
    { id: 'b', properties: { mag: 2, time: 1 }, geometry: { coordinates: [10, -95, 1] } },
    { id: 'c', properties: { mag: null, time: 1 }, geometry: { coordinates: [10, 10, 1] } },
    { id: 'd', properties: { mag: 2, time: 1 }, geometry: { coordinates: [10, 10, null] } }
  ] };
  const list = Q.parse(bad);
  eq(list.length, 1);
  eq(list[0].id, 'd');
  eq(list[0].depthKm, null, 'missing depth is null, not 0');
  eq(list[0].place, 'Unknown location');
  eq(list[0].url, null);
});

test('colour steps up with magnitude', () => {
  eq(Q.color(1.0), '#ffe680');
  eq(Q.color(2.5), '#ffc400');
  eq(Q.color(4.5), '#ff8a00');
  eq(Q.color(6.0), '#ff3b3b');
});

test('ring radius grows with magnitude and is clamped', () => {
  eq(Q.ringRadius(0.1), 0.5);
  eq(Q.ringRadius(9.5), 8);
  assert(Q.ringRadius(6) > Q.ringRadius(4), 'bigger quake, bigger ring');
});

/* ------------------------------------------------------------- poller */
function scripted(results) {
  /* fetchOnce that answers from a list: a value resolves, an Error rejects. */
  let calls = 0;
  const fn = () => {
    const r = results[Math.min(calls, results.length - 1)];
    calls++;
    return r instanceof Error ? Promise.reject(r) : Promise.resolve(r);
  };
  fn.calls = () => calls;
  return fn;
}

test('start fetches at once, goes live, and schedules the next fetch', async () => {
  const t = fakeTimers();
  const seen = [];
  const statuses = [];
  const fetchOnce = scripted(['A']);
  const p = P.createPoller({
    fetchOnce, intervalMs: 60000, onData: (d) => seen.push(d), onStatus: (s) => statuses.push(s),
    setTimer: t.set, clearTimer: t.clear
  });
  p.start();
  eq(p.status, 'loading');
  await flush();
  eq(seen.join(), 'A');
  eq(p.status, 'live');
  eq(statuses.join(), 'loading,live');
  eq(t.queue.length, 1);
  eq(t.queue[0].ms, 60000);
});

test('failures go OFFLINE, back off, cap, and recover', async () => {
  const t = fakeTimers();
  const err = new Error('down');
  const fetchOnce = scripted([err, err, err, err, 'OK']);
  const seen = [];
  const p = P.createPoller({
    fetchOnce, intervalMs: 60000, retryMs: 5000, maxBackoffMs: 15000,
    onData: (d) => seen.push(d), setTimer: t.set, clearTimer: t.clear
  });
  p.start();
  await flush();
  eq(p.status, 'offline');
  eq(t.queue[0].ms, 5000, 'first retry');
  t.fire(); await flush();
  eq(t.queue[0].ms, 10000, 'second retry doubles');
  t.fire(); await flush();
  eq(t.queue[0].ms, 15000, 'third retry is capped');
  t.fire(); await flush();
  eq(t.queue[0].ms, 15000, 'stays capped');
  t.fire(); await flush();
  eq(p.status, 'live');
  eq(p.failures, 0);
  eq(seen.join(), 'OK');
  eq(t.queue[0].ms, 60000, 'back to the normal interval');
});

test('a throwing onData counts as a failure, not a crash', async () => {
  const t = fakeTimers();
  const p = P.createPoller({
    fetchOnce: scripted(['A']), intervalMs: 1000,
    onData: () => { throw new Error('bad data'); }, setTimer: t.set, clearTimer: t.clear
  });
  p.start();
  await flush();
  eq(p.status, 'offline');
});

test('stop cancels the timer and ignores an answer still in flight', async () => {
  const t = fakeTimers();
  let release;
  const fetchOnce = () => new Promise((r) => { release = r; });
  const seen = [];
  const p = P.createPoller({
    fetchOnce, intervalMs: 1000, onData: (d) => seen.push(d), setTimer: t.set, clearTimer: t.clear
  });
  p.start();
  p.stop();
  release('LATE');
  await flush();
  eq(seen.length, 0, 'late answer ignored');
  eq(p.status, 'idle');
  eq(t.queue.length, 0, 'nothing scheduled');
});

test('refresh fetches now and leaves exactly one timer', async () => {
  const t = fakeTimers();
  const fetchOnce = scripted(['A', 'B']);
  const seen = [];
  const p = P.createPoller({
    fetchOnce, intervalMs: 1000, onData: (d) => seen.push(d), setTimer: t.set, clearTimer: t.clear
  });
  p.start();
  await flush();
  p.refresh();
  await flush();
  eq(seen.join(), 'A,B');
  eq(t.queue.length, 1);
});

/* ------------------------------------------------------------- quake layer */
function quakeLayer(fetchJson) {
  const t = fakeTimers();
  const layer = QL.create({
    fetchJson, now: () => 1792560000000, setTimer: t.set, clearTimer: t.clear
  });
  return { layer, t };
}

test('the quake layer asks USGS and turns the feed into points and rings', async () => {
  const asked = [];
  const { layer } = quakeLayer((url) => { asked.push(url); return Promise.resolve(FIXTURE); });
  let changes = 0;
  layer.onChange(() => changes++);
  eq(layer.id, 'quakes');
  eq(layer.status, 'idle');
  layer.start();
  await flush();
  eq(asked[0], Q.FEED_URL);
  eq(layer.status, 'live');
  eq(layer.count, 3);
  eq(layer.points.length, 3);
  eq(layer.rings.length, 2, 'only M2.5 and up get a ring');
  const p = layer.points[0];
  eq(p.layer, 'quakes');
  eq(p.item.id, 'us0003');
  eq(p.lat, -30.1);
  eq(p.color, '#ff3b3b');
  assert(p.radius > layer.points[2].radius, 'bigger quake, bigger dot');
  assert(changes >= 2, 'told the app about loading and data');
});

test('describe says what, where, when and how deep', async () => {
  const { layer } = quakeLayer(() => Promise.resolve(FIXTURE));
  layer.start();
  await flush();
  const d = layer.describe(layer.points[0].item);
  eq(d.title, 'M6.1 EARTHQUAKE');
  eq(d.rows[0].join('|'), 'WHERE|offshore Coquimbo, Chile');
  eq(d.rows[1].join('|'), 'WHEN|1 h ago');
  eq(d.rows[2].join('|'), 'DEPTH|40 km');
  eq(d.link, 'https://earthquake.usgs.gov/earthquakes/eventpage/us0003');
});

test('a failed feed leaves the layer OFFLINE with nothing drawn', async () => {
  const { layer } = quakeLayer(() => Promise.reject(new Error('HTTP 503')));
  layer.start();
  await flush();
  eq(layer.status, 'offline');
  eq(layer.count, 0);
  eq(layer.points.length, 0);
});

test('stop clears everything off the globe', async () => {
  const { layer, t } = quakeLayer(() => Promise.resolve(FIXTURE));
  layer.start();
  await flush();
  layer.stop();
  eq(layer.status, 'idle');
  eq(layer.count, 0);
  eq(layer.points.length, 0);
  eq(layer.rings.length, 0);
  eq(t.queue.length, 0);
});


/* ------------------------------------------------------------- satellites */
test('checksum is the TLE mod-10 rule (digits, and 1 for each minus)', () => {
  const l1 = STATIONS_TLE.split(/\r?\n/)[1];
  eq(S.checksum(l1), Number(l1[68]));
  eq(S.checksum('1 -' + ' '.repeat(65) + '0'), 2, 'one digit and one minus');
});

test('parseTle keeps whole, valid records and drops broken ones', () => {
  const stations = S.parseTle(STATIONS_TLE);
  eq(stations.length, 2);
  eq(stations[0].name, 'ISS (ZARYA)', 'name is trimmed');
  eq(stations[0].norad, 25544);
  eq(stations[1].norad, 48274);
  const visual = S.parseTle(VISUAL_TLE);
  eq(visual.map((r) => r.name).join('|'), 'ISS (ZARYA)|HST|NOAA 19', 'bad checksum and truncated record skipped');
});

test('parseTle copes with nothing useful', () => {
  eq(S.parseTle('').length, 0);
  eq(S.parseTle('<html>Too many requests</html>').length, 0);
  throws(() => S.parseTle(null), 'null is not a feed');
});

test('positionAt puts the ISS where the ISS can be', () => {
  const iss = S.parseTle(STATIONS_TLE)[0];
  const rec = SATLIB.twoline2satrec(iss.line1, iss.line2);
  const p = S.positionAt(rec, new Date(SAT_TIME), SATLIB);
  assert(p, 'got a position');
  assert(p.altKm > 370 && p.altKm < 460, 'altitude ' + p.altKm);
  assert(p.speedKmS > 7.5 && p.speedKmS < 7.8, 'speed ' + p.speedKmS);
  assert(Math.abs(p.lat) <= 51.7, 'latitude within the inclination: ' + p.lat);
  assert(p.lng >= -180 && p.lng <= 180, 'longitude in range: ' + p.lng);
  const later = S.positionAt(rec, new Date(SAT_TIME + 60000), SATLIB);
  assert(Math.abs(later.lng - p.lng) + Math.abs(later.lat - p.lat) > 1, 'a minute later it has moved');
});

test('positionAt gives null instead of a made-up place', () => {
  const iss = S.parseTle(STATIONS_TLE)[0];
  const rec = SATLIB.twoline2satrec(iss.line1, iss.line2);
  eq(S.positionAt(rec, new Date(Date.UTC(2040, 0, 1)), SATLIB), null, 'decayed long before 2040');
  eq(S.positionAt({ error: 1 }, new Date(SAT_TIME), { propagate: () => ({ position: false, velocity: false }) }), null);
});


/* ------------------------------------------------------------- sats layer */
function satsLayer(answers) {
  /* answers: { stations: text|Error, visual: text|Error } */
  const t = fakeTimers();
  let clock = SAT_TIME;
  const asked = [];
  const layer = SL.create({
    fetchText: (url) => {
      asked.push(url);
      const a = url === S.FEEDS.stations ? answers.stations : answers.visual;
      return a instanceof Error ? Promise.reject(a) : Promise.resolve(a);
    },
    satlib: SATLIB,
    now: () => clock,
    setTimer: t.set,
    clearTimer: t.clear
  });
  return { layer, t, asked, advance: (ms) => { clock += ms; } };
}

test('the sats layer merges both groups, once each, stations first', async () => {
  const { layer, asked, t } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  eq(asked.slice().sort().join(' '), [S.FEEDS.stations, S.FEEDS.visual].sort().join(' '));
  eq(layer.status, 'live');
  eq(layer.count, 4, 'ISS is in both groups but drawn once');
  eq(layer.html.map((h) => h.item.name).join('|'), 'ISS (ZARYA)|CSS (TIANHE)|HST|NOAA 19');
  eq(layer.points.length, 0);
  eq(layer.rings.length, 0);
  eq(t.queue[0].ms, 2 * 3600 * 1000, 'refetches every two hours');
});

test('stations are amber and the big two are labelled', async () => {
  const { layer } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  const [iss, css, hst] = layer.html;
  eq(iss.layer, 'sats');
  eq(iss.label, 'ISS');
  eq(css.label, 'TIANGONG');
  eq(hst.label, null);
  eq(iss.color, '#ffb300');
  eq(hst.color, '#1aff8c');
  near(iss.altitude, iss.item.altKm / 6371, 1e-9, 'altitude in Earth radii');
});

test('one group down is still live; both down is OFFLINE', async () => {
  const half = satsLayer({ stations: new Error('503'), visual: VISUAL_TLE });
  half.layer.start();
  await flush();
  eq(half.layer.status, 'live');
  eq(half.layer.count, 3);
  const none = satsLayer({ stations: new Error('503'), visual: new Error('503') });
  none.layer.start();
  await flush();
  eq(none.layer.status, 'offline');
  eq(none.layer.count, 0);
});

test('tick moves the satellites, keeping the same marker objects', async () => {
  const { layer, advance } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  const iss = layer.html[0];
  const before = [iss.lat, iss.lng];
  let changed = 0;
  layer.onChange(() => changed++);
  advance(30000);
  layer.tick();
  assert(layer.html[0] === iss, 'same object, moved in place');
  assert(iss.lat !== before[0] || iss.lng !== before[1], 'it moved');
  eq(changed, 1, 'one change per tick');
});

test('a satellite the model cannot place is left off the globe', async () => {
  const { layer, advance } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  advance(14 * 365 * 86400000);
  layer.tick();
  /* By 2040 the low, draggy stations have come down in the model; the higher
   * satellites have not. */
  const names = layer.html.map((h) => h.item.name);
  assert(names.indexOf('ISS (ZARYA)') < 0 && names.indexOf('CSS (TIANHE)') < 0, 'stations dropped: ' + names.join(', '));
  assert(names.indexOf('NOAA 19') >= 0, 'NOAA 19 still placed');
  eq(layer.count, layer.html.length, 'the count matches what is drawn');
});

test('describe gives name, height, speed, position and a link', async () => {
  const { layer } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  const d = layer.describe(layer.html[0].item);
  eq(d.title, 'ISS (ZARYA)');
  eq(d.rows[0][0], 'ALTITUDE');
  assert(/^\d{3} km \/ \d{3} mi$/.test(d.rows[0][1]), d.rows[0][1]);
  eq(d.rows[1][0], 'SPEED');
  assert(/^2\d,\d{3} km\/h \/ 1\d,\d{3} mph$/.test(d.rows[1][1]), d.rows[1][1]);
  eq(d.rows[2][0], 'POSITION');
  assert(/^\d+\.\d° [NS], \d+\.\d° [EW]$/.test(d.rows[2][1]), d.rows[2][1]);
  eq(d.rows[3].join('|'), 'NORAD ID|25544');
  eq(d.link, 'https://www.n2yo.com/satellite/?s=25544');
});

test('stop clears the satellites', async () => {
  const { layer, t } = satsLayer({ stations: STATIONS_TLE, visual: VISUAL_TLE });
  layer.start();
  await flush();
  layer.stop();
  eq(layer.status, 'idle');
  eq(layer.html.length, 0);
  eq(layer.count, 0);
  eq(t.queue.length, 0);
});


/* ------------------------------------------------------------- weather */
/* A tiny RGBA image filled by a function of (x, y). */
function image(width, height, fill) {
  const data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) data.set(fill(x, y), (y * width + x) * 4);
  }
  return { width, height, data };
}
function pixel(img, x, y) {
  const i = (y * img.width + x) * 4;
  return Array.from(img.data.slice(i, i + 4));
}

test('the clouds URL asks NASA for one whole-world image of a given day', () => {
  const url = W.cloudsUrl('2026-10-20');
  assert(url.startsWith('https://wvs.earthdata.nasa.gov/api/v1/snapshot?'), url);
  assert(url.includes('LAYERS=VIIRS_NOAA21_CorrectedReflectance_TrueColor'), 'layer');
  assert(url.includes('TIME=2026-10-20'), 'date');
  assert(url.includes('BBOX=-90,-180,90,180'), 'whole world');
  assert(url.includes('WIDTH=2048') && url.includes('HEIGHT=1024'), '2:1 size');
});

test('the clouds day is yesterday in UTC, so the mosaic is complete', () => {
  eq(W.cloudsDay(Date.UTC(2026, 9, 21, 0, 30)), '2026-10-20');
  eq(W.cloudsDay(Date.UTC(2026, 0, 1, 12)), '2025-12-31');
});

test('cloudAlpha keeps cloud and drops land, sea, desert and gaps', () => {
  near(W.cloudAlpha(240, 240, 240), 1, 0.001, 'bright white cloud');
  assert(W.cloudAlpha(170, 175, 180) > 0.4, 'grey cloud is partly kept');
  eq(W.cloudAlpha(10, 40, 90), 0, 'sea');
  eq(W.cloudAlpha(60, 110, 40), 0, 'forest');
  eq(W.cloudAlpha(210, 180, 140), 0, 'desert is bright but coloured');
  eq(W.cloudAlpha(0, 0, 0), 0, 'the black gap between orbits');
});

test('latestRainFrame picks the newest radar frame and rejects junk', () => {
  const f = W.latestRainFrame({
    host: 'https://tilecache.rainviewer.com',
    radar: { past: [{ time: 100, path: '/v2/radar/100' }, { time: 200, path: '/v2/radar/200' }] }
  });
  eq(f.host, 'https://tilecache.rainviewer.com');
  eq(f.path, '/v2/radar/200');
  eq(f.time, 200);
  throws(() => W.latestRainFrame({}), 'no radar');
  throws(() => W.latestRainFrame({ host: 'h', radar: { past: [] } }), 'no frames');
  eq(W.rainTileUrl('https://h', '/v2/radar/200', 2, 1, 3), 'https://h/v2/radar/200/256/2/1/3/2/1_1.png');
});

test('mercatorY puts the equator in the middle and the poles off the map', () => {
  eq(W.mercatorY(0, 1024), 512);
  assert(Math.abs(W.mercatorY(85.0511, 1024)) < 0.5, 'top edge');
  assert(Math.abs(W.mercatorY(-85.0511, 1024) - 1024) < 0.5, 'bottom edge');
  assert(W.mercatorY(60, 1024) < 512 - 512 / 3, 'high latitudes are stretched');
});

test('stitch lays tiles out in a grid and leaves missing ones clear', () => {
  const red = image(2, 2, () => [255, 0, 0, 255]);
  const out = W.stitch([[red, null], [null, red]], 2);
  eq(out.width, 4);
  eq(out.height, 4);
  eq(pixel(out, 0, 0).join(), '255,0,0,255');
  eq(pixel(out, 3, 0).join(), '0,0,0,0', 'missing tile is transparent');
  eq(pixel(out, 3, 3).join(), '255,0,0,255');
});

test('composite: dark base, clouds where clouds are, nothing past the poles', () => {
  /* Clouds image: the left half is white cloud, the right half is sea. */
  const clouds = image(8, 4, (x) => (x < 4 ? [245, 245, 245, 255] : [10, 40, 90, 255]));
  const out = W.composite({ width: 8, height: 4, clouds: clouds, rain: null });
  eq(pixel(out, 6, 2).join(), W.BASE.concat(255).join(), 'sea is plain dark globe');
  const c = pixel(out, 1, 2);
  assert(c[0] > 150 && c[1] > 150, 'cloud is bright: ' + c);
  eq(c[3], 255, 'the texture itself is opaque');
});

test('composite reprojects rain onto the right latitude', () => {
  /* A Mercator rain map with rain only in its top quarter (far north). */
  const rain = image(16, 16, (x, y) => (y < 4 ? [0, 120, 255, 255] : [0, 0, 0, 0]));
  /* 32 rows of 5.625°: row 0 is centred at 87.2°N (past Mercator's 85.05° edge),
   * row 2 at 75.9°N (Mercator row 2.7 of 16: rainy), row 16 at 2.8°S (dry). */
  const out = W.composite({ width: 16, height: 32, clouds: null, rain: rain });
  const north = pixel(out, 5, 2);
  assert(north[2] > 150, 'rain painted in the far north: ' + north);
  eq(pixel(out, 5, 16).join(), W.BASE.concat(255).join(), 'dry at the equator');
  eq(pixel(out, 5, 0).join(), W.BASE.concat(255).join(), 'nothing beyond the Mercator edge');
});


/* ------------------------------------------------------------- weather layer */
const RAIN_INDEX = {
  host: 'https://tilecache.rainviewer.com',
  radar: { past: [{ time: 1792563000, path: '/v2/radar/1792563000' }] }
};

/* sources: { clouds: 'ok'|'down', rain: 'ok'|'down' } */
function weatherLayer(sources) {
  const t = fakeTimers();
  const asked = [];
  const cloudImg = image(16, 8, (x) => (x < 8 ? [245, 245, 245, 255] : [10, 40, 90, 255]));
  const rainTile = image(4, 4, () => [0, 120, 255, 255]);
  let clock = Date.UTC(2026, 9, 21, 6, 12);
  const layer = WL.create({
    fetchJson: (url) => {
      asked.push(url);
      return sources.rain === 'ok' ? Promise.resolve(RAIN_INDEX) : Promise.reject(new Error('503'));
    },
    loadPixels: (url) => {
      asked.push(url);
      if (url.includes('wvs.earthdata')) {
        return sources.clouds === 'ok' ? Promise.resolve(cloudImg) : Promise.reject(new Error('503'));
      }
      return Promise.resolve(rainTile);
    },
    now: () => clock,
    setTimer: t.set,
    clearTimer: t.clear,
    size: { width: 32, height: 16 }
  });
  return { layer, t, asked, advance: (ms) => { clock += ms; } };
}

test('weather paints clouds and rain into one texture', async () => {
  const { layer, asked, t } = weatherLayer({ clouds: 'ok', rain: 'ok' });
  eq(layer.texture, null, 'nothing before it starts');
  layer.start();
  await flush();
  eq(layer.status, 'live');
  assert(layer.texture && layer.texture.width === 32 && layer.texture.height === 16, 'a texture');
  eq(layer.summary, '06:10 UTC', 'the button shows the radar time');
  eq(layer.credit, 'Clouds: NASA GIBS / VIIRS · Rain: RainViewer');
  assert(asked.includes(W.cloudsUrl('2026-10-20')), 'asked NASA for yesterday');
  eq(asked.filter((u) => u.includes('tilecache')).length, 16, 'all 16 radar tiles');
  eq(t.queue[0].ms, 600000, 'refreshes every 10 minutes');
  eq(layer.points.length + layer.rings.length + layer.html.length, 0, 'draws nothing but the texture');
});

test('weather with only clouds, or only rain, still shows', async () => {
  const clouds = weatherLayer({ clouds: 'ok', rain: 'down' });
  clouds.layer.start();
  await flush();
  eq(clouds.layer.status, 'live');
  eq(clouds.layer.summary, 'CLOUDS');
  eq(clouds.layer.credit, 'Clouds: NASA GIBS / VIIRS');
  const rain = weatherLayer({ clouds: 'down', rain: 'ok' });
  rain.layer.start();
  await flush();
  eq(rain.layer.status, 'live');
  eq(rain.layer.summary, '06:10 UTC');
  eq(rain.layer.credit, 'Rain: RainViewer');
});

test('weather with both sources down is OFFLINE with no texture', async () => {
  const { layer } = weatherLayer({ clouds: 'down', rain: 'down' });
  layer.start();
  await flush();
  eq(layer.status, 'offline');
  eq(layer.texture, null);
});

test('the clouds photo is only fetched again when it is stale', async () => {
  const { layer, asked, t, advance } = weatherLayer({ clouds: 'ok', rain: 'ok' });
  layer.start();
  await flush();
  const nasa = () => asked.filter((u) => u.includes('wvs.earthdata')).length;
  eq(nasa(), 1);
  advance(600000);
  t.fire();
  await flush();
  eq(nasa(), 1, 'ten minutes later: radar again, clouds kept');
  advance(3600000);
  t.fire();
  await flush();
  eq(nasa(), 2, 'an hour later: clouds again');
});

test('the same radar frame is not downloaded twice', async () => {
  const { layer, asked, t, advance } = weatherLayer({ clouds: 'ok', rain: 'ok' });
  layer.start();
  await flush();
  advance(600000);
  t.fire();
  await flush();
  eq(asked.filter((u) => u.includes('tilecache')).length, 16, 'still 16: the frame had not changed');
});

test('stop clears the weather off the globe', async () => {
  const { layer, t } = weatherLayer({ clouds: 'ok', rain: 'ok' });
  layer.start();
  await flush();
  layer.stop();
  eq(layer.status, 'idle');
  eq(layer.texture, null);
  eq(t.queue.length, 0);
});


/* ------------------------------------------------------------- planes */
test('the plane URLs ask for a 250 nm circle, or the same-sized box', () => {
  eq(PL.adsbUrl(40.123456, -74.5), 'https://api.adsb.lol/v2/point/40.12/-74.50/250');
  const o = PL.openskyUrl(40, -74);
  assert(o.startsWith('https://opensky-network.org/api/states/all?'), o);
  const q = Object.fromEntries(new URL(o).searchParams);
  near(Number(q.lamax) - Number(q.lamin), 2 * 250 * 1.852 / 111.32, 0.01, 'box height ~ the circle');
  assert(Number(q.lomax) - Number(q.lomin) > Number(q.lamax) - Number(q.lamin), 'wider in longitude away from the equator');
  const polar = Object.fromEntries(new URL(PL.openskyUrl(89, 0)).searchParams);
  assert(Number(polar.lamax) <= 90 && Number(polar.lomin) >= -180 && Number(polar.lomax) <= 180, 'clamped near the pole');
});

test('parseAdsb keeps placed, fresh aircraft and reads the ground', () => {
  const list = PL.parseAdsb(ADSB);
  eq(list.map((p) => p.hex).join(','), 'a1b2c3,abc001,a00f00', 'stale, unplaced and impossible rows dropped');
  const ual = list[0];
  eq(ual.callsign, 'UAL123', 'callsign trimmed');
  eq(ual.altFt, 35000);
  eq(ual.onGround, false);
  near(ual.speedKt, 480.2, 1e-9);
  eq(ual.track, 90);
  eq(ual.type, 'B738');
  eq(ual.reg, 'N12345');
  const cessna = list[2];
  eq(cessna.onGround, true, '"ground" altitude');
  eq(cessna.altFt, 0);
  eq(cessna.callsign, '', 'no callsign is an empty string, not undefined');
  throws(() => PL.parseAdsb({}), 'not a feed');
});

test('parseOpensky converts metres and m/s, and drops stale or unplaced rows', () => {
  const list = PL.parseOpensky(OPENSKY);
  eq(list.map((p) => p.hex).join(','), '3c6444,4b1805');
  const dlh = list[0];
  eq(dlh.callsign, 'DLH9LF');
  eq(Math.round(dlh.altFt), 35000, '10668 m');
  eq(Math.round(dlh.speedKt), 450, '231.5 m/s');
  eq(dlh.track, 45);
  eq(list[1].onGround, true);
  eq(list[1].altFt, 0, 'no altitude on the ground reads as 0');
  throws(() => PL.parseOpensky({ time: 1 }), 'no states list');
  eq(PL.parseOpensky({ time: 1, states: null }).length, 0, 'an empty sky is not an error');
});

test('distanceKm is the great-circle distance', () => {
  near(PL.distanceKm({ lat: 0, lng: 0 }, { lat: 0, lng: 1 }), 111.19, 0.05);
  near(PL.distanceKm({ lat: 51.47, lng: -0.45 }, { lat: 40.64, lng: -73.78 }), 5555, 15, 'London to New York');
});

test('nearest keeps the closest aircraft up to the cap', () => {
  const list = PL.parseAdsb(ADSB);
  const two = PL.nearest(list, { lat: 40.64, lng: -73.78 }, 2);
  eq(two.map((p) => p.hex).join(','), 'a00f00,a1b2c3');
});

test('advance flies a plane along its track at its speed', () => {
  const east = { lat: 40, lng: -74, speedKt: 480, track: 90, onGround: false };
  const after = PL.advance(east, 60);
  near(after.lat, 40, 0.01, 'still at 40°N heading due east');
  near(after.lng, -74 + 14.816 / (111.19 * Math.cos(40 * Math.PI / 180)), 0.002, '8 nm east');
  const north = PL.advance({ lat: 10, lng: 20, speedKt: 600, track: 0, onGround: false }, 3600);
  near(north.lat, 20, 0.05, '600 nm north is 10°');
  const parked = PL.advance({ lat: 10, lng: 20, speedKt: 0, track: 0, onGround: true }, 60);
  eq(parked.lat + ',' + parked.lng, '10,20', 'parked planes stay put');
  const dateline = PL.advance({ lat: 0, lng: 179.99, speedKt: 600, track: 90, onGround: false }, 60);
  assert(dateline.lng < -179, 'wraps across the date line: ' + dateline.lng);
});


/* ------------------------------------------------------------- planes layer */
/* sources: { adsb: 'ok'|'down', opensky: 'ok'|'down' } */
function planesLayer(sources) {
  const t = fakeTimers();
  const asked = [];
  let view = { lat: 40.64, lng: -73.78, altitude: 2 };
  let clock = 1000000;
  const layer = PLL.create({
    now: () => clock,
    fetchJson: (url) => {
      asked.push(url);
      const ok = url.includes('adsb.lol') ? sources.adsb === 'ok' : sources.opensky === 'ok';
      if (!ok) return Promise.reject(new Error('blocked'));
      return Promise.resolve(url.includes('adsb.lol') ? ADSB : OPENSKY);
    },
    getView: () => view,
    setTimer: t.set,
    clearTimer: t.clear
  });
  return { layer, t, asked, look: (v) => { view = v; }, advance: (ms) => { clock += ms; } };
}

test('planes come from adsb.lol around the view, nearest first', async () => {
  const { layer, asked, t } = planesLayer({ adsb: 'ok', opensky: 'ok' });
  eq(layer.followsView, true);
  layer.start();
  await flush();
  eq(asked.join(' '), PL.adsbUrl(40.64, -73.78), 'only the primary is asked');
  eq(layer.status, 'live');
  eq(layer.count, 3);
  eq(layer.html[0].item.hex, 'a00f00', 'nearest the centre first');
  eq(layer.credit, 'Planes: adsb.lol (ODbL)');
  eq(t.queue[0].ms, 15000, 'every 15 seconds');
  const ual = layer.html.find((h) => h.item.hex === 'a1b2c3');
  eq(ual.layer, 'planes');
  eq(ual.rotation, 90, 'pointing along its track');
  eq(ual.label, null);
  near(ual.altitude, 35000 * 0.3048 / 1000 / 6371, 1e-9, 'real altitude in Earth radii');
});

test('OpenSky answers when adsb.lol will not', async () => {
  const { layer, asked } = planesLayer({ adsb: 'down', opensky: 'ok' });
  layer.start();
  await flush();
  eq(asked.length, 2);
  assert(asked[1].includes('opensky-network.org'), 'fell back to OpenSky');
  eq(layer.status, 'live');
  eq(layer.count, 2);
  eq(layer.credit, 'Planes: OpenSky Network');
});

test('both flight sources down is OFFLINE', async () => {
  const { layer } = planesLayer({ adsb: 'down', opensky: 'down' });
  layer.start();
  await flush();
  eq(layer.status, 'offline');
  eq(layer.count, 0);
});

test('the next fetch follows the view', async () => {
  const { layer, asked, look } = planesLayer({ adsb: 'ok', opensky: 'ok' });
  layer.start();
  await flush();
  look({ lat: 51.47, lng: -0.45, altitude: 2 });
  layer.refresh();
  await flush();
  eq(asked[1], PL.adsbUrl(51.47, -0.45));
});

test('tick flies airborne planes and leaves parked ones', async () => {
  const { layer, advance } = planesLayer({ adsb: 'ok', opensky: 'ok' });
  layer.start();
  await flush();
  const ual = layer.html.find((h) => h.item.hex === 'a1b2c3');
  const cessna = layer.html.find((h) => h.item.hex === 'a00f00');
  const before = [ual.lng, cessna.lat, cessna.lng];
  advance(10000);
  layer.tick();
  assert(ual.lng > before[0], 'the eastbound airliner moved east');
  eq(cessna.lat + ',' + cessna.lng, before[1] + ',' + before[2], 'the parked Cessna did not');
});

test('describe gives callsign, height, speed, heading and type', async () => {
  const { layer } = planesLayer({ adsb: 'ok', opensky: 'ok' });
  layer.start();
  await flush();
  const ual = layer.html.find((h) => h.item.hex === 'a1b2c3').item;
  const d = layer.describe(ual);
  eq(d.title, 'UAL123');
  eq(d.rows[0].join('|'), 'ALTITUDE|35,000 ft / 10,668 m');
  eq(d.rows[1].join('|'), 'SPEED|480 kt / 553 mph');
  eq(d.rows[2].join('|'), 'HEADING|090° E');
  eq(d.rows[3].join('|'), 'AIRCRAFT|B738 · N12345');
  eq(d.link, 'https://globe.adsb.lol/?icao=a1b2c3');
  const cessna = layer.describe(layer.html.find((h) => h.item.hex === 'a00f00').item);
  eq(cessna.title, 'N9999X', 'no callsign: the registration');
  eq(cessna.rows[0].join('|'), 'ALTITUDE|ON THE GROUND');
});

test('stop clears the planes', async () => {
  const { layer, t } = planesLayer({ adsb: 'ok', opensky: 'ok' });
  layer.start();
  await flush();
  layer.stop();
  eq(layer.status, 'idle');
  eq(layer.html.length, 0);
  eq(layer.count, 0);
  eq(t.queue.length, 0);
});


/* ------------------------------------------------------------- cameras */
test('there are three camera sources, each with its lists and a credit', () => {
  eq(CAM.SOURCES.map((x) => x.id).join(','), 'tfl,digitraffic,caltrans');
  eq(CAM.SOURCES[0].urls.join(), 'https://api.tfl.gov.uk/Place/Type/JamCam');
  eq(CAM.SOURCES[1].urls.join(), 'https://tie.digitraffic.fi/api/weathercam/v1/stations');
  eq(CAM.SOURCES[2].urls.length, 4, 'four Caltrans districts');
  assert(CAM.SOURCES[2].urls.includes('https://cwwp2.dot.ca.gov/data/d4/cctv/cctvStatusD04.json'), 'district 4 URL');
  assert(CAM.SOURCES.every((x) => x.credit && x.label), 'every source is credited and named');
});

test('parseTfl keeps working cameras with safe picture links', () => {
  const list = CAM.parseTfl(TFL);
  eq(list.map((c) => c.id).join(','), 'tfl:00001.01251,tfl:00001.06514', 'unavailable and http-only dropped');
  const c = list[0];
  eq(c.name, 'Rotherhithe Tunnel');
  eq(c.lat, 51.5076);
  eq(c.lng, -0.0459);
  eq(c.image, 'https://s3-eu-west-1.amazonaws.com/jamcams.tfl.gov.uk/00001.01251.jpg');
  eq(c.source, 'tfl');
  throws(() => CAM.parseTfl({}), 'not a list');
});

test('parseDigitraffic makes one camera per working station, from a preset in collection', () => {
  const list = CAM.parseDigitraffic(DIGITRAFFIC);
  eq(list.map((c) => c.id).join(','), 'fi:C01502,fi:C04507', 'removed station and empty one dropped');
  eq(list[0].image, 'https://weathercam.digitraffic.fi/C0150201.jpg', 'first preset that is collected');
  eq(list[0].name, 'vt7 Porvoo', 'underscores read as spaces');
  eq(list[0].lat, 60.3922);
  eq(list[0].lng, 25.6633);
  throws(() => CAM.parseDigitraffic({ features: 7 }), 'not a feature list');
});

test('parseCaltrans keeps in-service cameras that have a picture', () => {
  const list = CAM.parseCaltrans(CALTRANS);
  eq(list.length, 1);
  const c = list[0];
  eq(c.id, 'ca:4:1');
  eq(c.name, 'I-80 : Bay Bridge Toll Plaza', 'the TV code is trimmed off');
  eq(c.lat, 37.8253);
  eq(c.lng, -122.3075);
  eq(c.image, 'https://cwwp2.dot.ca.gov/data/d4/cctv/image/tv101i80baybridgetollplaza/tv101i80baybridgetollplaza.jpg');
  throws(() => CAM.parseCaltrans([]), 'not a Caltrans answer');
});


/* ------------------------------------------------------------- cameras layer */
/* down: the source ids that refuse */
function camerasLayer(down) {
  const t = fakeTimers();
  const asked = [];
  let view = { lat: 51.5074, lng: -0.1278, altitude: 2 };
  let clock = 1000000;
  const layer = CAML.create({
    fetchJson: (url) => {
      asked.push(url);
      const src = url.includes('tfl.gov.uk') ? 'tfl' : url.includes('digitraffic') ? 'digitraffic' : 'caltrans';
      if ((down || []).includes(src)) return Promise.reject(new Error('blocked'));
      return Promise.resolve(src === 'tfl' ? TFL : src === 'digitraffic' ? DIGITRAFFIC : CALTRANS);
    },
    getView: () => view,
    now: () => clock,
    setTimer: t.set,
    clearTimer: t.clear
  });
  return { layer, t, asked, look: (v) => { view = v; }, advance: (ms) => { clock += ms; } };
}

test('cameras come from every source, nearest the view first', async () => {
  const { layer, asked, t } = camerasLayer();
  eq(layer.followsView, true);
  layer.start();
  await flush();
  eq(asked.length, 6, 'TfL, Digitraffic and four Caltrans districts');
  eq(layer.status, 'live');
  eq(layer.count, 5, 'the same Caltrans camera from four districts counts once');
  eq(layer.html[0].item.id, 'tfl:00001.06514', 'Trafalgar Square is under the view');
  eq(layer.html[0].layer, 'cameras');
  eq(layer.credit, 'Cameras: TfL Open Data · Cameras: Fintraffic / digitraffic.fi, CC BY 4.0 · Cameras: Caltrans');
  eq(t.queue[0].ms, 15 * 60000, 'the lists are fetched again every 15 minutes');
});

test('a source that refuses just drops out; all refusing is OFFLINE', async () => {
  const some = camerasLayer(['tfl']);
  some.layer.start();
  await flush();
  eq(some.layer.status, 'live');
  eq(some.layer.count, 3);
  assert(!some.layer.credit.includes('TfL'), 'TfL not credited when it gave nothing');
  const none = camerasLayer(['tfl', 'digitraffic', 'caltrans']);
  none.layer.start();
  await flush();
  eq(none.layer.status, 'offline');
  eq(none.layer.count, 0);
});

test('looking somewhere else re-picks cameras without downloading again', async () => {
  const { layer, asked, look } = camerasLayer();
  layer.start();
  await flush();
  const trafalgar = layer.html[0];
  look({ lat: 60.17, lng: 24.94, altitude: 2 });
  layer.refresh();
  await flush();
  eq(asked.length, 6, 'no new downloads');
  eq(layer.html[0].item.id, 'fi:C04507', 'Helsinki is now nearest');
  assert(layer.html.includes(trafalgar), 'markers keep their identity when re-picked');
});

test('lists older than 15 minutes are downloaded again on refresh', async () => {
  const { layer, asked, advance } = camerasLayer();
  layer.start();
  await flush();
  advance(16 * 60000);
  layer.refresh();
  await flush();
  eq(asked.length, 12);
});

test('describe gives the picture, the place and the source', async () => {
  const { layer } = camerasLayer();
  layer.start();
  await flush();
  const d = layer.describe(layer.html[0].item);
  eq(d.title, 'Trafalgar Sq/Cockspur St');
  eq(d.image, 'https://s3-eu-west-1.amazonaws.com/jamcams.tfl.gov.uk/00001.06514.jpg');
  eq(d.rows[0].join('|'), 'SOURCE|TfL JamCams (London)');
  eq(d.rows[1].join('|'), 'POSITION|51.507° N, 0.128° W');
  eq(d.link, d.image, 'the link opens the full picture');
});

test('stop clears the cameras', async () => {
  const { layer, t } = camerasLayer();
  layer.start();
  await flush();
  layer.stop();
  eq(layer.status, 'idle');
  eq(layer.html.length, 0);
  eq(layer.count, 0);
  eq(t.queue.length, 0);
});

/* ------------------------------------------------------------- run */
(async () => {
  let failed = 0;
  console.log("\nGOD'S EYE VIEW — logic");
  for (const [name, fn] of tests) {
    try {
      await fn();
      console.log('  ok   ' + name);
    } catch (e) {
      failed++;
      console.log('  FAIL ' + name + '\n       ' + e.message);
    }
  }
  console.log('\n' + (tests.length - failed) + '/' + tests.length + ' passed');
  process.exit(failed ? 1 : 0);
})();
