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
