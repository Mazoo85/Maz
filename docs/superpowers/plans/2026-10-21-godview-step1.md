# GOD'S EYE VIEW — Step 1 (globe, theme, layer bar, earthquakes) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `godview/` to MAZ ARCADE: a phone-first dark "spy console" 3D globe with glowing country outlines, a top bar (title, UTC clock, LIVE light), a bottom layer bar, a details sheet, and one live layer — USGS earthquakes.

**Architecture:** A static page (no build step, no server) in the arcade's style: plain scripts that each wrap an IIFE, export with `module.exports` for Node and assign a `GV_*` global for the browser. Pure logic (`js/lib/`) and each layer (`js/layers/`) have no DOM or globe dependency and are unit-tested in Node. `js/app.js` is the only file that touches the DOM and the globe; it merges every enabled layer's `points`/`rings` into the globe. The globe is `globe.gl` loaded from a pinned CDN URL.

**Tech Stack:** HTML/CSS/vanilla JS · `globe.gl@2.46.2` (bundles three.js) via jsDelivr · Natural Earth 110m countries (via `world-atlas@2.0.2`, converted once to GeoJSON) · Node 22 tests · Playwright + Chromium for the browser test.

**Spec:** `docs/superpowers/specs/2026-10-21-godview-design.md`

## Global Constraints

- Static files only. No server of our own, no API keys anywhere in the page.
- Globe library URL, exactly: `https://cdn.jsdelivr.net/npm/globe.gl@2.46.2/dist/globe.gl.min.js`
- Earthquake feed, exactly: `https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson`, refreshed every 60 s.
- Look: black background, dark globe, country outlines `#1aff8c`, console type (`ui-monospace` stack), amber/red quake markers.
- Phone first: tap targets ≥ 44 px tall, no horizontal page scroll at 390 px wide, safe-area insets respected, `viewport-fit=cover`.
- A failing source shows its button as `OFFLINE` and retries on its own; it never breaks another layer or throws.
- If the globe library cannot load or WebGL is missing, show a readable fallback message — never an uncaught error.
- The shared MAZ ARCADE nav is a floating pill in the top-left corner (`data-mode="overlay"`); the top bar must not overlap it (it is ~75 px wide; the bar leaves 96 px).
- Every file in `js/` follows the arcade module pattern (IIFE, `module.exports` + `GV_*` global).
- Repo gates must pass before every push: `node scripts/check-links.mjs && node scripts/check-exchange.mjs`.

## File map

| File | Responsibility |
|---|---|
| `godview/index.html` | Page shell: top bar, globe mount, layer bar, details sheet, fallback, script tags |
| `godview/css/godview.css` | Spy-console theme, layout, safe areas |
| `godview/manifest.webmanifest`, `godview/icon.svg` | Add-to-home-screen |
| `godview/data/countries.geojson` | Country outlines (generated, committed) |
| `godview/tools/build-countries.cjs` | Regenerates `countries.geojson` from `world-atlas` |
| `godview/js/lib/format.js` | `GV_FORMAT`: clock, counts, "12 min ago", hex → rgba |
| `godview/js/lib/poller.js` | `GV_POLLER`: fetch on an interval, back off on failure, report status |
| `godview/js/lib/quakes.js` | `GV_QUAKES`: USGS feed URL, parser, colour and ring size by magnitude |
| `godview/js/layers/quakes-layer.js` | `GV_QUAKES_LAYER`: the earthquake layer (data → points/rings, describe) |
| `godview/js/app.js` | Boots the globe, draws layers, layer bar, clock, LIVE light, sheet, fallback |
| `godview/tests/fixtures/usgs-sample.json` | Small saved USGS response (3 valid + 2 broken features) |
| `godview/tests/godview-logic.test.js` | Node unit tests for everything in `js/lib` and `js/layers` |
| `godview/tests/godview-browser.test.js` | Chromium at phone size: boots, draws, taps, toggles, offline, fallback |
| `godview/README.md` | What it is, sources and their licences, how to test |
| Modify `shared/projects.js` | Hub card + nav entry |
| Modify `index.html` (root) | `<noscript>` list entry |
| Modify `README.md` (root) | Project table row |
| Modify `scripts/smoke-site.cjs` | Add the app to `APPS` |
| Modify `.github/workflows/site-ci.yml` | Run both new test files |

## The layer contract (used by every later step)

Every layer module exposes `create(opts)` returning an object with exactly:

```text
id        string   stable key, e.g. 'quakes'
label     string   button text, e.g. 'QUAKES'
icon      string   one emoji/glyph
status    'idle' | 'loading' | 'live' | 'offline'
count     number   items currently shown
points    array    globe points: { layer, item, lat, lng, color, radius, altitude }
rings     array    globe rings:  { lat, lng, color, maxRadius }
start()            begin fetching
stop()             stop fetching and clear points/rings/count
refresh()          fetch now
onChange(fn)       fn(layer) after any change to status/count/points/rings
describe(item)     -> { title, rows: [[label, value], ...], link: url|null }
```

`opts`: `{ fetchJson(url) -> Promise<json>, now?() -> ms, setTimer?, clearTimer? }`.

---

### Task 1: Pure logic — format, poller, quakes parser

**Files:**
- Create: `godview/js/lib/format.js`, `godview/js/lib/poller.js`, `godview/js/lib/quakes.js`
- Create: `godview/tests/fixtures/usgs-sample.json`
- Test: `godview/tests/godview-logic.test.js`

**Interfaces:**
- Produces:
  - `GV_FORMAT.utcClock(date: Date) -> 'HH:MM:SS UTC'`
  - `GV_FORMAT.count(n: number) -> '4,812'`
  - `GV_FORMAT.ago(ms: number) -> 'just now' | 'N min ago' | 'N h ago' | 'N days ago'`
  - `GV_FORMAT.rgba(hex: '#rrggbb', alpha: number) -> 'rgba(r,g,b,a)'`
  - `GV_POLLER.createPoller({ fetchOnce, onData, onStatus?, intervalMs, retryMs?=5000, maxBackoffMs?=intervalMs*5, setTimer?, clearTimer? }) -> { start(), stop(), refresh(), status, failures }`
  - `GV_QUAKES.FEED_URL`, `GV_QUAKES.parse(json) -> Quake[]` (sorted by magnitude, descending; throws if not a feed), `GV_QUAKES.color(mag) -> hex`, `GV_QUAKES.ringRadius(mag) -> degrees in [0.5, 8]`
  - `Quake = { id, lat, lng, depthKm|null, mag, place, time, url|null }`

- [ ] **Step 1: Write the fixture**

`godview/tests/fixtures/usgs-sample.json`:

```json
{
  "type": "FeatureCollection",
  "metadata": { "generated": 1792560000000, "title": "USGS All Earthquakes, Past Day (test sample)" },
  "features": [
    {
      "type": "Feature", "id": "ak0001",
      "properties": { "mag": 4.6, "place": "62 km NW of Anchorage, Alaska", "time": 1792559280000, "url": "https://earthquake.usgs.gov/earthquakes/eventpage/ak0001" },
      "geometry": { "type": "Point", "coordinates": [-150.6, 61.6, 35.2] }
    },
    {
      "type": "Feature", "id": "ci0002",
      "properties": { "mag": 1.2, "place": "5 km S of Ridgecrest, CA", "time": 1792559900000, "url": "https://earthquake.usgs.gov/earthquakes/eventpage/ci0002" },
      "geometry": { "type": "Point", "coordinates": [-117.67, 35.58, 7.1] }
    },
    {
      "type": "Feature", "id": "us0003",
      "properties": { "mag": 6.1, "place": "offshore Coquimbo, Chile", "time": 1792556400000, "url": "https://earthquake.usgs.gov/earthquakes/eventpage/us0003" },
      "geometry": { "type": "Point", "coordinates": [-71.9, -30.1, 40] }
    },
    {
      "type": "Feature", "id": "nomag",
      "properties": { "mag": null, "place": "no magnitude", "time": 1792559000000 },
      "geometry": { "type": "Point", "coordinates": [10, 10, 1] }
    },
    {
      "type": "Feature", "id": "nogeo",
      "properties": { "mag": 3.0, "place": "no geometry", "time": 1792559000000 },
      "geometry": null
    }
  ]
}
```

- [ ] **Step 2: Write the failing tests**

`godview/tests/godview-logic.test.js`:

```js
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
```

- [ ] **Step 3: Run to verify it fails**

Run: `node godview/tests/godview-logic.test.js`
Expected: exits non-zero with `Cannot find module '../js/lib/format.js'`.

- [ ] **Step 4: Implement `godview/js/lib/format.js`**

```js
/*
 * GOD'S EYE VIEW — the words and numbers on screen.
 *
 * Plain functions, no DOM: the clock, counts with thousands separators,
 * "12 min ago", and the hex → rgba conversion the fading rings need.
 */
(function (global) {
  'use strict';

  function pad(n) {
    return (n < 10 ? '0' : '') + n;
  }

  function utcClock(date) {
    return pad(date.getUTCHours()) + ':' + pad(date.getUTCMinutes()) + ':' +
      pad(date.getUTCSeconds()) + ' UTC';
  }

  function count(n) {
    return String(Math.round(n)).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  }

  function ago(ms) {
    const s = Math.max(0, Math.floor(ms / 1000));
    if (s < 60) return 'just now';
    const m = Math.floor(s / 60);
    if (m < 60) return m + ' min ago';
    const h = Math.floor(m / 60);
    if (h < 48) return h + ' h ago';
    return Math.floor(h / 24) + ' days ago';
  }

  function rgba(hex, alpha) {
    const v = parseInt(hex.slice(1), 16);
    return 'rgba(' + ((v >> 16) & 255) + ',' + ((v >> 8) & 255) + ',' + (v & 255) + ',' + alpha + ')';
  }

  const API = { utcClock: utcClock, count: count, ago: ago, rgba: rgba };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_FORMAT = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

- [ ] **Step 5: Implement `godview/js/lib/quakes.js`**

```js
/*
 * GOD'S EYE VIEW — the USGS earthquake feed.
 *
 * USGS publishes every quake of the past day as GeoJSON, keyless and open to
 * web pages. parse() turns it into plain points and drops anything it cannot
 * place for certain — a marker in the wrong place is worse than no marker.
 */
(function (global) {
  'use strict';

  const FEED_URL = 'https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson';

  function num(v) {
    return v === null || v === undefined || v === '' ? NaN : Number(v);
  }

  function parse(json) {
    if (!json || !Array.isArray(json.features)) throw new Error('not a USGS feed');
    const out = [];
    for (const f of json.features) {
      const c = f && f.geometry && f.geometry.coordinates;
      const p = f && f.properties;
      if (!Array.isArray(c) || c.length < 2 || !p) continue;
      const lng = num(c[0]);
      const lat = num(c[1]);
      const mag = num(p.mag);
      if (!isFinite(lat) || !isFinite(lng) || !isFinite(mag)) continue;
      if (lat < -90 || lat > 90 || lng < -180 || lng > 180) continue;
      const depth = num(c[2]);
      out.push({
        id: String(f.id),
        lat: lat,
        lng: lng,
        depthKm: isFinite(depth) ? depth : null,
        mag: mag,
        place: p.place ? String(p.place) : 'Unknown location',
        time: num(p.time) || 0,
        url: p.url ? String(p.url) : null
      });
    }
    out.sort((a, b) => b.mag - a.mag);
    return out;
  }

  function color(mag) {
    if (mag >= 6) return '#ff3b3b';
    if (mag >= 4.5) return '#ff8a00';
    if (mag >= 2.5) return '#ffc400';
    return '#ffe680';
  }

  /* Ring size in degrees of arc on the globe. */
  function ringRadius(mag) {
    return Math.max(0.5, Math.min(8, mag * 1.2));
  }

  const API = { FEED_URL: FEED_URL, parse: parse, color: color, ringRadius: ringRadius };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_QUAKES = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

- [ ] **Step 6: Implement `godview/js/lib/poller.js`**

```js
/*
 * GOD'S EYE VIEW — fetch something on a timer, and survive it failing.
 *
 * Every live layer is "ask a source every N seconds". When the source fails
 * the layer says OFFLINE and retries sooner, doubling the wait each time up to
 * a cap, then drops back to the normal interval the moment it answers again.
 * A stop() — or a refresh() — makes any answer still in flight be ignored.
 *
 * Timers are injectable so the tests can run a minute of polling instantly.
 */
(function (global) {
  'use strict';

  function createPoller(opts) {
    const intervalMs = opts.intervalMs;
    const retryMs = opts.retryMs || 5000;
    const maxBackoffMs = opts.maxBackoffMs || intervalMs * 5;
    const setTimer = opts.setTimer || ((fn, ms) => setTimeout(fn, ms));
    const clearTimer = opts.clearTimer || ((h) => clearTimeout(h));

    let timer = null;
    let running = false;
    let failures = 0;
    let status = 'idle';
    let generation = 0;

    function setStatus(s) {
      if (s === status) return;
      status = s;
      if (opts.onStatus) opts.onStatus(s);
    }

    function cancelTimer() {
      if (timer !== null) {
        clearTimer(timer);
        timer = null;
      }
    }

    function nextDelay() {
      if (failures === 0) return intervalMs;
      return Math.min(maxBackoffMs, retryMs * Math.pow(2, failures - 1));
    }

    async function tick() {
      timer = null;
      if (!running) return;
      const mine = ++generation;
      try {
        const data = await opts.fetchOnce();
        if (!running || mine !== generation) return;
        opts.onData(data);
        failures = 0;
        setStatus('live');
      } catch (e) {
        if (!running || mine !== generation) return;
        failures++;
        setStatus('offline');
      }
      timer = setTimer(tick, nextDelay());
    }

    return {
      start() {
        if (running) return;
        running = true;
        failures = 0;
        setStatus('loading');
        tick();
      },
      stop() {
        running = false;
        generation++;
        cancelTimer();
        setStatus('idle');
      },
      refresh() {
        if (!running) return;
        cancelTimer();
        tick();
      },
      get status() { return status; },
      get failures() { return failures; }
    };
  }

  const API = { createPoller: createPoller };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_POLLER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

- [ ] **Step 7: Run the tests and make sure they pass**

Run: `node godview/tests/godview-logic.test.js`
Expected: every line `ok`, ends `14/14 passed`, exit code 0.

- [ ] **Step 8: Commit**

```bash
git add godview/js/lib godview/tests
git commit -m "godview: feed parser, poller and formatting, with tests"
```

---

### Task 2: The earthquake layer

**Files:**
- Create: `godview/js/layers/quakes-layer.js`
- Modify: `godview/tests/godview-logic.test.js` (add a section before the `/* --- run */` block)

**Interfaces:**
- Consumes: `GV_QUAKES`, `GV_POLLER`, `GV_FORMAT` (read from globals at `create()` time — require them first in Node).
- Produces: `GV_QUAKES_LAYER.create(opts) -> Layer` per "The layer contract" above. Points carry `layer: 'quakes'` and `item: Quake`. Rings only for `mag >= 2.5`.

- [ ] **Step 1: Write the failing tests**

Add near the top, after the `const Q = ...` line:

```js
const QL = require('../js/layers/quakes-layer.js');
```

Add this section above `/* ------------------------------------------------------------- run */`:

```js
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
```

- [ ] **Step 2: Run to verify it fails**

Run: `node godview/tests/godview-logic.test.js`
Expected: exits non-zero with `Cannot find module '../js/layers/quakes-layer.js'`.

- [ ] **Step 3: Implement `godview/js/layers/quakes-layer.js`**

```js
/*
 * GOD'S EYE VIEW — earthquakes.
 *
 * Every quake of the past day from USGS, refreshed each minute: a dot sized and
 * coloured by magnitude, and a pulsing ring for anything M2.5 or bigger.
 * Follows the layer contract in docs/superpowers/plans/2026-10-21-godview-step1.md.
 */
(function (global) {
  'use strict';

  function create(opts) {
    const Q = global.GV_QUAKES;
    const P = global.GV_POLLER;
    const F = global.GV_FORMAT;
    const now = opts.now || Date.now;
    const listeners = [];

    const layer = {
      id: 'quakes',
      label: 'QUAKES',
      icon: '〰',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      start() { poller.start(); },
      stop() { poller.stop(); show([]); },
      refresh() { poller.refresh(); },
      onChange(fn) { listeners.push(fn); },
      describe: describe
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    function show(list) {
      layer.count = list.length;
      layer.points = list.map((q) => ({
        layer: 'quakes',
        item: q,
        lat: q.lat,
        lng: q.lng,
        color: Q.color(q.mag),
        radius: 0.15 + Math.max(0, q.mag) * 0.08,
        altitude: 0.01
      }));
      layer.rings = list.filter((q) => q.mag >= 2.5).map((q) => ({
        lat: q.lat,
        lng: q.lng,
        color: Q.color(q.mag),
        maxRadius: Q.ringRadius(q.mag)
      }));
      emit();
    }

    function describe(q) {
      return {
        title: 'M' + q.mag.toFixed(1) + ' EARTHQUAKE',
        rows: [
          ['WHERE', q.place],
          ['WHEN', F.ago(now() - q.time)],
          ['DEPTH', q.depthKm === null ? 'unknown' : Math.round(q.depthKm) + ' km'],
          ['TIME', new Date(q.time).toISOString().replace('T', ' ').slice(0, 19) + ' UTC']
        ],
        link: q.url
      };
    }

    const poller = P.createPoller({
      intervalMs: 60000,
      retryMs: 5000,
      maxBackoffMs: 300000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: () => opts.fetchJson(Q.FEED_URL).then(Q.parse),
      onData: show,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_QUAKES_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

Note: fixture `now` is `1792560000000`; quake `us0003` is at `1792556400000` → 3,600,000 ms → `'1 h ago'`.

- [ ] **Step 4: Run the tests and make sure they pass**

Run: `node godview/tests/godview-logic.test.js`
Expected: ends `18/18 passed`, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add godview/js/layers godview/tests/godview-logic.test.js
git commit -m "godview: earthquake layer on the shared layer contract"
```

---

### Task 3: Country outlines data

**Files:**
- Create: `godview/tools/build-countries.cjs`
- Create (generated): `godview/data/countries.geojson`

**Interfaces:**
- Produces: `godview/data/countries.geojson` — a GeoJSON `FeatureCollection` of ~177 country `Polygon`/`MultiPolygon` features, coordinates rounded to 2 decimals, each with `properties.name`.

- [ ] **Step 1: Write the converter**

`godview/tools/build-countries.cjs`:

```js
/*
 * Regenerates godview/data/countries.geojson — the glowing country outlines.
 *
 * The outlines are Natural Earth's 1:110m countries (public domain), as packed
 * by world-atlas (ISC). They are converted once and committed, so the page
 * needs no converter and no extra download at run time.
 *
 *   cd "$(mktemp -d)" && npm pack world-atlas@2.0.2 topojson-client@3.1.0
 *   mkdir wa tc && tar xzf world-atlas-2.0.2.tgz -C wa && tar xzf topojson-client-3.1.0.tgz -C tc
 *   node /path/to/Maz/godview/tools/build-countries.cjs wa/package tc/package
 */
'use strict';

const fs = require('fs');
const path = require('path');

const [worldAtlasDir, topojsonDir] = process.argv.slice(2);
if (!worldAtlasDir || !topojsonDir) {
  console.error('usage: node build-countries.cjs <world-atlas package dir> <topojson-client package dir>');
  process.exit(2);
}

const topojson = require(path.resolve(topojsonDir, 'dist', 'topojson-client.js'));
const topo = JSON.parse(fs.readFileSync(path.resolve(worldAtlasDir, 'countries-110m.json'), 'utf8'));
const geo = topojson.feature(topo, topo.objects.countries);

const round = (v) => (Array.isArray(v) ? v.map(round) : Math.round(v * 100) / 100);
const out = {
  type: 'FeatureCollection',
  features: geo.features.map((f) => ({
    type: 'Feature',
    properties: { name: f.properties.name },
    geometry: { type: f.geometry.type, coordinates: round(f.geometry.coordinates) }
  }))
};

const target = path.join(__dirname, '..', 'data', 'countries.geojson');
fs.mkdirSync(path.dirname(target), { recursive: true });
fs.writeFileSync(target, JSON.stringify(out));
console.log('wrote ' + out.features.length + ' countries, ' + fs.statSync(target).size + ' bytes → ' + target);
```

- [ ] **Step 2: Generate the file**

Run the four commands in the converter's header comment, with `/home/user/Maz` as the repo path.
Expected: `wrote 177 countries, <about 150000–250000> bytes → .../godview/data/countries.geojson`.

- [ ] **Step 3: Sanity-check it**

Run: `node -e "const g=JSON.parse(require('fs').readFileSync('godview/data/countries.geojson','utf8'));console.log(g.type,g.features.length,g.features.find(f=>f.properties.name==='Chile').geometry.type)"`
Expected: `FeatureCollection 177 MultiPolygon` (count may be 175–177).

- [ ] **Step 4: Commit**

```bash
git add godview/tools godview/data
git commit -m "godview: Natural Earth country outlines and the script that builds them"
```

---

### Task 4: The page — shell, theme, globe, layer bar, sheet, fallback

**Files:**
- Create: `godview/index.html`, `godview/css/godview.css`, `godview/js/app.js`, `godview/manifest.webmanifest`, `godview/icon.svg`
- Test: `godview/tests/godview-browser.test.js`

**Interfaces:**
- Consumes: `GV_FORMAT`, `GV_QUAKES_LAYER` (and through it `GV_QUAKES`, `GV_POLLER`); `Globe` from globe.gl.
- Produces: `window.GODVIEW = { globe, layers, enabled, openSheet(point), closeSheet() }` — test and debugging hooks. DOM ids the test relies on: `#globe`, `#clock`, `#live`, `#layerbar`, `button.layer-btn[data-layer=<id>]` with child `.val`, `#sheet`, `#sheet-title`, `#sheet-rows`, `#sheet-link`, `#sheet-close`, `#fallback`, `#fallback-msg`.

- [ ] **Step 1: Write the failing browser test**

`godview/tests/godview-browser.test.js`:

```js
/*
 * GOD'S EYE VIEW — the page, driven in a real browser at phone size.
 *
 * The logic suite proves the feeds and the layers; this proves the page: it
 * boots clean, draws a globe, shows live quakes in the layer bar, opens the
 * details sheet, toggles a layer off, survives the feed going down, and shows
 * a readable message instead of crashing when the globe cannot load.
 *
 * The USGS feed is always answered from the saved fixture, so this never
 * depends on what the Earth did today. The globe library comes from the CDN;
 * set GLOBE_JS to a local copy of globe.gl.min.js (2.46.2) to run offline.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node godview/tests/godview-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const FIXTURE = fs.readFileSync(path.join(__dirname, 'fixtures', 'usgs-sample.json'));
const GLOBE_URL = 'https://cdn.jsdelivr.net/npm/globe.gl@2.46.2/dist/globe.gl.min.js';

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

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8217;
const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.md': 'text/plain',
  '.json': 'application/json', '.geojson': 'application/json', '.svg': 'image/svg+xml',
  '.webmanifest': 'application/manifest+json'
};

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
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio'] };
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

/* A phone-sized page with the network answered by us. `quakes` and `globe` are
 * 'ok' or 'down'. Errors caused on purpose (a feed we cut) are not problems. */
async function openPage(browser, base, { quakes = 'ok', globe = 'ok' } = {}) {
  const page = await browser.newPage({
    viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true
  });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => {
    if (m.type() !== 'error') return;
    if (/Failed to load resource|net::ERR_/.test(m.text())) return; // the cuts we made on purpose
    problems.push('console: ' + m.text());
  });
  await page.route('https://earthquake.usgs.gov/**', (route) =>
    quakes === 'ok'
      ? route.fulfill({ status: 200, contentType: 'application/json', body: FIXTURE,
        headers: { 'access-control-allow-origin': '*' } })
      : route.abort('failed'));
  await page.route(GLOBE_URL, (route) => {
    if (globe !== 'ok') return route.abort('failed');
    if (process.env.GLOBE_JS) {
      return route.fulfill({ status: 200, contentType: 'text/javascript', body: fs.readFileSync(process.env.GLOBE_JS) });
    }
    return route.continue();
  });
  await page.goto(base + '/godview/', { waitUntil: 'load' });
  await page.waitForTimeout(1500);
  return { page, problems };
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}`;
  const browser = await chromium.launch(launchOptions());

  try {
    console.log('\nBOOT (phone, 390 × 844)');
    {
      const { page, problems } = await openPage(browser, base);
      check(problems.length === 0, 'boots with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));
      check((await page.title()) === "GOD'S EYE VIEW", "title is GOD'S EYE VIEW");
      check(await page.evaluate(() => document.getElementById('fallback').hidden), 'no fallback message');
      check(await page.evaluate(() => !!document.querySelector('#globe canvas')), 'the globe drew a canvas');
      check(await page.evaluate(() => window.GODVIEW.globe.polygonsData().length > 150), 'country outlines are on the globe');
      check(/^\d\d:\d\d:\d\d UTC$/.test(await page.textContent('#clock')), 'the UTC clock is ticking');

      console.log('\nEARTHQUAKES');
      const btn = 'button.layer-btn[data-layer="quakes"]';
      check((await page.getAttribute(btn, 'aria-pressed')) === 'true', 'quakes are on by default');
      check((await page.textContent(btn + ' .val')).trim() === '3', 'the button counts 3 quakes');
      check(await page.evaluate(() => window.GODVIEW.globe.pointsData().length === 3), '3 quake dots on the globe');
      check(await page.evaluate(() => window.GODVIEW.globe.ringsData().length === 2), '2 pulsing rings (M2.5+)');
      check(await page.evaluate(() => document.getElementById('live').classList.contains('is-on')), 'the LIVE light is on');

      console.log('\nDETAILS SHEET');
      await page.evaluate(() => window.GODVIEW.openSheet(window.GODVIEW.globe.pointsData()[0]));
      check(await page.evaluate(() => !document.getElementById('sheet').hidden), 'tapping a quake opens the sheet');
      check((await page.textContent('#sheet-title')) === 'M6.1 EARTHQUAKE', 'the sheet names the biggest quake');
      check((await page.textContent('#sheet-rows')).includes('offshore Coquimbo, Chile'), 'and says where it was');
      check((await page.getAttribute('#sheet-link', 'href')) === 'https://earthquake.usgs.gov/earthquakes/eventpage/us0003', 'and links to USGS');
      await page.tap('#sheet-close');
      check(await page.evaluate(() => document.getElementById('sheet').hidden), 'CLOSE hides the sheet');

      console.log('\nTOGGLE');
      await page.tap(btn);
      check((await page.getAttribute(btn, 'aria-pressed')) === 'false', 'tapping turns quakes off');
      check((await page.textContent(btn + ' .val')).trim() === 'OFF', 'the button says OFF');
      check(await page.evaluate(() => window.GODVIEW.globe.pointsData().length === 0), 'the dots are gone');
      await page.tap(btn);
      await page.waitForTimeout(500);
      check((await page.textContent(btn + ' .val')).trim() === '3', 'tapping again brings them back');

      console.log('\nPHONE LAYOUT');
      const box = await page.$eval(btn, (e) => { const r = e.getBoundingClientRect(); return { h: r.height, right: r.right }; });
      check(box.h >= 44, 'the layer button is at least 44 px tall (' + Math.round(box.h) + ')');
      check(box.right <= 390, 'the layer button fits on screen');
      check(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), 'no sideways scrolling');
      const title = await page.$eval('#topbar h1', (e) => e.getBoundingClientRect().left);
      check(title >= 56, 'the title leaves room for the MAZ nav pill');
      check(problems.length === 0, 'still no errors after all that' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nFEED DOWN');
    {
      const { page, problems } = await openPage(browser, base, { quakes: 'down' });
      const btn = 'button.layer-btn[data-layer="quakes"]';
      check((await page.textContent(btn + ' .val')).trim() === 'OFFLINE', 'the quake button says OFFLINE');
      check(await page.evaluate(() => document.getElementById('fallback').hidden), 'the globe still shows');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }

    console.log('\nGLOBE LIBRARY UNREACHABLE');
    {
      const { page, problems } = await openPage(browser, base, { globe: 'down' });
      check(await page.evaluate(() => !document.getElementById('fallback').hidden), 'a fallback message shows');
      check((await page.textContent('#fallback-msg')).includes('could not be downloaded'), 'and says what went wrong');
      check(problems.length === 0, 'and nothing throws' + (problems.length ? ' — ' + problems.join('; ') : ''));
      await page.close();
    }
  } finally {
    await browser.close();
    server.close();
  }

  console.log('\n' + (failures ? failures + ' check(s) FAILED' : 'all checks passed'));
  process.exit(failures ? 1 : 0);
})();
```

- [ ] **Step 2: Run to verify it fails**

Run (offline-capable): `GLOBE_JS=<path to globe.gl.min.js 2.46.2> node godview/tests/godview-browser.test.js`
(Get the file with `npm pack globe.gl@2.46.2 && tar xzf globe.gl-2.46.2.tgz` in a scratch dir → `package/dist/globe.gl.min.js`.)
Expected: `FAIL` lines starting at `title is GOD'S EYE VIEW` (the page 404s); exit code 1.

- [ ] **Step 3: Write `godview/index.html`**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover" />
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
  <meta name="theme-color" content="#000000" />
  <meta name="description" content="GOD'S EYE VIEW — a live globe of earthquakes, satellites, weather and more, in your browser." />
  <title>GOD'S EYE VIEW</title>
  <link rel="manifest" href="manifest.webmanifest" />
  <link rel="icon" href="icon.svg" type="image/svg+xml" />
  <link rel="stylesheet" href="css/godview.css" />
</head>
<body>
  <div id="globe" aria-label="Live globe"></div>

  <header id="topbar">
    <h1>GOD'S EYE VIEW</h1>
    <div id="status">
      <span id="live" title="Lights up when live data arrives">LIVE</span>
      <span id="clock">--:--:-- UTC</span>
    </div>
  </header>

  <nav id="layerbar" aria-label="Layers"></nav>

  <section id="sheet" hidden aria-live="polite">
    <div class="sheet-head">
      <h2 id="sheet-title"></h2>
      <button id="sheet-close" type="button">CLOSE</button>
    </div>
    <dl id="sheet-rows"></dl>
    <a id="sheet-link" target="_blank" rel="noopener" hidden>SOURCE ↗</a>
  </section>

  <div id="fallback" hidden>
    <p class="fallback-title">NO SIGNAL</p>
    <p id="fallback-msg"></p>
  </div>

  <noscript>
    <div class="noscript">GOD'S EYE VIEW needs JavaScript switched on.</div>
  </noscript>

  <script src="https://cdn.jsdelivr.net/npm/globe.gl@2.46.2/dist/globe.gl.min.js"></script>
  <script src="js/lib/format.js"></script>
  <script src="js/lib/poller.js"></script>
  <script src="js/lib/quakes.js"></script>
  <script src="js/layers/quakes-layer.js"></script>
  <script src="js/app.js"></script>

  <!-- shared MAZ ARCADE nav: back to the hub + jump to any other project -->
  <script src="../shared/maz-nav.js" data-current="godview" data-mode="overlay" defer></script>
</body>
</html>
```

- [ ] **Step 4: Write `godview/css/godview.css`**

```css
/* GOD'S EYE VIEW — the spy-console look. Phone first. */
:root {
  --bg: #000000;
  --green: #1aff8c;
  --green-dim: rgba(26, 255, 140, 0.35);
  --amber: #ffb300;
  --red: #ff3b3b;
  --panel: rgba(2, 14, 8, 0.86);
  --text: #c8ffe0;
  --muted: #5f9c7c;
  --mono: ui-monospace, 'SF Mono', 'Cascadia Mono', 'Roboto Mono', Menlo, Consolas, monospace;
  --safe-top: env(safe-area-inset-top, 0px);
  --safe-bottom: env(safe-area-inset-bottom, 0px);
  --safe-left: env(safe-area-inset-left, 0px);
  --safe-right: env(safe-area-inset-right, 0px);
}

* { box-sizing: border-box; }

html, body {
  margin: 0;
  height: 100%;
  overflow: hidden;
  background: var(--bg);
  color: var(--text);
  font-family: var(--mono);
  -webkit-tap-highlight-color: transparent;
  -webkit-text-size-adjust: 100%;
}

#globe {
  position: fixed;
  inset: 0;
  touch-action: none;
}

/* Faint scan lines over everything, for the console feel. */
body::after {
  content: '';
  position: fixed;
  inset: 0;
  pointer-events: none;
  background: repeating-linear-gradient(to bottom, rgba(26, 255, 140, 0.035) 0 1px, transparent 1px 3px);
  z-index: 5;
}

/* ---------------------------------------------------------------- top bar */
#topbar {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  z-index: 10;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  padding: calc(var(--safe-top) + 10px) calc(var(--safe-right) + 12px) 10px calc(var(--safe-left) + 56px);
  background: linear-gradient(to bottom, rgba(0, 0, 0, 0.85), transparent);
  pointer-events: none;
}

#topbar h1 {
  margin: 0;
  font-size: 15px;
  letter-spacing: 0.18em;
  color: var(--green);
  text-shadow: 0 0 8px var(--green-dim);
  white-space: nowrap;
}

#status {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 12px;
  color: var(--muted);
  white-space: nowrap;
}

#live {
  padding: 2px 6px;
  border: 1px solid var(--muted);
  border-radius: 3px;
  letter-spacing: 0.1em;
}
#live.is-on { color: var(--red); border-color: var(--red); }
#live.is-blink { background: var(--red); color: #000; }

#clock { color: var(--text); }

/* -------------------------------------------------------------- layer bar */
#layerbar {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 10;
  display: flex;
  gap: 8px;
  overflow-x: auto;
  padding: 10px calc(var(--safe-right) + 10px) calc(var(--safe-bottom) + 10px) calc(var(--safe-left) + 10px);
  background: linear-gradient(to top, rgba(0, 0, 0, 0.9), transparent);
  scrollbar-width: none;
}
#layerbar::-webkit-scrollbar { display: none; }

.layer-btn {
  flex: 0 0 auto;
  min-width: 96px;
  min-height: 52px;
  display: grid;
  grid-template-columns: auto 1fr;
  grid-template-rows: auto auto;
  column-gap: 6px;
  align-items: center;
  padding: 6px 10px;
  border: 1px solid var(--green-dim);
  border-radius: 6px;
  background: var(--panel);
  color: var(--text);
  font: inherit;
  text-align: left;
  cursor: pointer;
}
.layer-btn .icon { grid-row: 1 / span 2; font-size: 20px; }
.layer-btn .name { font-size: 11px; letter-spacing: 0.12em; color: var(--green); }
.layer-btn .val { font-size: 14px; }

.layer-btn[aria-pressed='false'] { opacity: 0.55; border-color: rgba(95, 156, 124, 0.4); }
.layer-btn[aria-pressed='false'] .name { color: var(--muted); }
.layer-btn.is-offline { border-color: var(--red); }
.layer-btn.is-offline .val { color: var(--red); }
.layer-btn.is-loading .val { color: var(--amber); }

/* ---------------------------------------------------------- details sheet */
#sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 20;
  max-height: 60%;
  overflow-y: auto;
  padding: 14px calc(var(--safe-right) + 16px) calc(var(--safe-bottom) + 16px) calc(var(--safe-left) + 16px);
  background: var(--panel);
  border-top: 1px solid var(--green);
  box-shadow: 0 -6px 24px rgba(26, 255, 140, 0.15);
}
#sheet[hidden] { display: none; }

.sheet-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}
#sheet-title {
  margin: 0;
  font-size: 16px;
  letter-spacing: 0.1em;
  color: var(--amber);
}
#sheet-close {
  min-height: 44px;
  min-width: 72px;
  border: 1px solid var(--green-dim);
  border-radius: 6px;
  background: transparent;
  color: var(--green);
  font: inherit;
  letter-spacing: 0.1em;
}

#sheet-rows {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 6px 14px;
  margin: 12px 0;
  font-size: 13px;
}
#sheet-rows dt { color: var(--muted); letter-spacing: 0.1em; }
#sheet-rows dd { margin: 0; overflow-wrap: anywhere; }

#sheet-link {
  display: inline-block;
  padding: 10px 0;
  color: var(--green);
  font-size: 13px;
  letter-spacing: 0.1em;
}

/* -------------------------------------------------------------- fallback */
#fallback {
  position: fixed;
  inset: 0;
  z-index: 30;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 24px;
  text-align: center;
  background: var(--bg);
}
#fallback[hidden] { display: none; }
.fallback-title {
  margin: 0 0 10px;
  font-size: 22px;
  letter-spacing: 0.2em;
  color: var(--red);
}
#fallback-msg { max-width: 30em; line-height: 1.5; color: var(--text); }

.noscript { padding: 80px 24px; text-align: center; }
```

- [ ] **Step 5: Write `godview/js/app.js`**

```js
/*
 * GOD'S EYE VIEW — the page.
 *
 * The only file that touches the DOM or the globe. It boots the globe, draws
 * every enabled layer's points and rings onto it, keeps the layer bar, the
 * clock and the LIVE light up to date, and opens the details sheet on a tap.
 * Layers know nothing about the page: they follow the layer contract in
 * docs/superpowers/plans/2026-10-21-godview-step1.md.
 */
(function () {
  'use strict';

  const F = window.GV_FORMAT;
  const $ = (id) => document.getElementById(id);

  /* Which layers start switched on. */
  const DEFAULT_ON = { quakes: true };

  function fail(message) {
    $('fallback-msg').textContent = message;
    $('fallback').hidden = false;
  }

  function hasWebGL() {
    try {
      const c = document.createElement('canvas');
      return !!(c.getContext('webgl2') || c.getContext('webgl'));
    } catch (e) {
      return false;
    }
  }

  async function fetchJson(url) {
    const res = await fetch(url, { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return res.json();
  }

  /* ------------------------------------------------------------ clock */
  function startClock() {
    const tick = () => { $('clock').textContent = F.utcClock(new Date()); };
    tick();
    setInterval(tick, 1000);
  }

  /* ------------------------------------------------------------ globe */
  function makeGlobe(el) {
    const globe = new Globe(el, { animateIn: false })
      .width(window.innerWidth)
      .height(window.innerHeight)
      .backgroundColor('#000000')
      .showAtmosphere(true)
      .atmosphereColor('#1aff8c')
      .atmosphereAltitude(0.16)
      .showGraticules(true)
      .polygonsData([])
      .polygonCapColor(() => 'rgba(0,0,0,0)')
      .polygonSideColor(() => 'rgba(0,0,0,0)')
      .polygonStrokeColor(() => '#1aff8c')
      .polygonAltitude(0.004)
      .pointsData([])
      .pointLat('lat')
      .pointLng('lng')
      .pointColor('color')
      .pointRadius('radius')
      .pointAltitude('altitude')
      .pointsMerge(false)
      .ringsData([])
      .ringLat('lat')
      .ringLng('lng')
      .ringColor((r) => (t) => F.rgba(r.color, 1 - t))
      .ringMaxRadius('maxRadius')
      .ringPropagationSpeed(2)
      .ringRepeatPeriod(1400);

    const material = globe.globeMaterial();
    material.color.set('#03140c');
    if (material.emissive) material.emissive.set('#010a05');

    globe.renderer().setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
    globe.pointOfView({ lat: 20, lng: -40, altitude: 2.4 });

    const controls = globe.controls();
    controls.autoRotate = true;
    controls.autoRotateSpeed = 0.35;
    controls.addEventListener('start', () => { controls.autoRotate = false; });

    window.addEventListener('resize', () => {
      globe.width(window.innerWidth).height(window.innerHeight);
    });
    return globe;
  }

  /* ------------------------------------------------------------ sheet */
  function openSheet(point) {
    const layer = state.layers.find((l) => l.id === point.layer);
    if (!layer) return;
    const d = layer.describe(point.item);
    $('sheet-title').textContent = d.title;
    const rows = $('sheet-rows');
    rows.textContent = '';
    for (const [k, v] of d.rows) {
      const dt = document.createElement('dt');
      dt.textContent = k;
      const dd = document.createElement('dd');
      dd.textContent = v;
      rows.append(dt, dd);
    }
    const link = $('sheet-link');
    if (d.link) {
      link.href = d.link;
      link.hidden = false;
    } else {
      link.removeAttribute('href');
      link.hidden = true;
    }
    $('sheet').hidden = false;
  }

  function closeSheet() {
    $('sheet').hidden = true;
  }

  /* ------------------------------------------------------------ layers */
  const state = { globe: null, layers: [], enabled: {}, buttons: {} };

  function redraw() {
    const on = state.layers.filter((l) => state.enabled[l.id]);
    state.globe.pointsData(on.flatMap((l) => l.points));
    state.globe.ringsData(on.flatMap((l) => l.rings));
  }

  function paintButton(layer) {
    const btn = state.buttons[layer.id];
    const enabled = !!state.enabled[layer.id];
    btn.setAttribute('aria-pressed', String(enabled));
    btn.classList.toggle('is-offline', enabled && layer.status === 'offline');
    btn.classList.toggle('is-loading', enabled && layer.status === 'loading');
    let val;
    if (!enabled) val = 'OFF';
    else if (layer.status === 'offline') val = 'OFFLINE';
    else if (layer.status === 'loading') val = '…';
    else val = F.count(layer.count);
    btn.querySelector('.val').textContent = val;
  }

  let blinkTimer = null;
  function paintLive(blink) {
    const anyLive = state.layers.some((l) => state.enabled[l.id] && l.status === 'live');
    const live = $('live');
    live.classList.toggle('is-on', anyLive);
    if (blink && anyLive) {
      live.classList.add('is-blink');
      clearTimeout(blinkTimer);
      blinkTimer = setTimeout(() => live.classList.remove('is-blink'), 600);
    }
  }

  function addLayer(layer) {
    state.layers.push(layer);
    state.enabled[layer.id] = !!DEFAULT_ON[layer.id];

    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'layer-btn';
    btn.dataset.layer = layer.id;
    const icon = document.createElement('span');
    icon.className = 'icon';
    icon.textContent = layer.icon;
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = layer.label;
    const val = document.createElement('span');
    val.className = 'val';
    btn.append(icon, name, val);
    btn.addEventListener('click', () => {
      const on = !state.enabled[layer.id];
      state.enabled[layer.id] = on;
      if (on) layer.start();
      else layer.stop();
      paintButton(layer);
      paintLive(false);
      redraw();
    });
    $('layerbar').append(btn);
    state.buttons[layer.id] = btn;

    layer.onChange(() => {
      paintButton(layer);
      paintLive(layer.status === 'live');
      redraw();
    });
    paintButton(layer);
    if (state.enabled[layer.id]) layer.start();
  }

  /* ------------------------------------------------------------ boot */
  function boot() {
    startClock();
    $('sheet-close').addEventListener('click', closeSheet);

    if (typeof window.Globe !== 'function') {
      fail('The globe could not be downloaded. Check your internet connection and reload the page.');
      return;
    }
    if (!hasWebGL()) {
      fail('This browser cannot draw 3D graphics (WebGL is switched off or not supported).');
      return;
    }
    try {
      state.globe = makeGlobe($('globe'));
    } catch (e) {
      fail('The globe could not start: ' + e.message);
      return;
    }
    state.globe.onPointClick(openSheet).onGlobeClick(closeSheet);

    fetchJson('data/countries.geojson')
      .then((geo) => state.globe.polygonsData(geo.features))
      .catch(() => { /* outlines are decoration; the live layers still work */ });

    addLayer(window.GV_QUAKES_LAYER.create({ fetchJson: fetchJson }));

    window.GODVIEW = {
      globe: state.globe,
      layers: state.layers,
      enabled: state.enabled,
      openSheet: openSheet,
      closeSheet: closeSheet
    };
  }

  boot();
})();
```

- [ ] **Step 6: Write `godview/manifest.webmanifest` and `godview/icon.svg`**

`godview/manifest.webmanifest`:

```json
{
  "name": "GOD'S EYE VIEW",
  "short_name": "God View",
  "description": "A live globe of earthquakes, satellites, weather and more.",
  "start_url": ".",
  "scope": ".",
  "display": "standalone",
  "orientation": "any",
  "background_color": "#000000",
  "theme_color": "#000000",
  "icons": [
    { "src": "icon.svg", "sizes": "any", "type": "image/svg+xml", "purpose": "any" }
  ]
}
```

`godview/icon.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" rx="12" fill="#000"/>
  <circle cx="32" cy="32" r="20" fill="#03140c" stroke="#1aff8c" stroke-width="2"/>
  <ellipse cx="32" cy="32" rx="9" ry="20" fill="none" stroke="#1aff8c" stroke-width="1.5"/>
  <line x1="12" y1="32" x2="52" y2="32" stroke="#1aff8c" stroke-width="1.5"/>
  <circle cx="40" cy="24" r="3" fill="#ff3b3b"/>
</svg>
```

- [ ] **Step 7: Run the browser test and make sure it passes**

Run: `GLOBE_JS=<path to globe.gl.min.js 2.46.2> node godview/tests/godview-browser.test.js`
Expected: every line `ok`, ends `all checks passed`, exit code 0.
Also run `node godview/tests/godview-logic.test.js` → `18/18 passed`.

- [ ] **Step 8: Look at it**

Take a phone-size screenshot (add `await page.screenshot({ path: '<scratchpad>/godview-phone.png' })` temporarily, or a one-off Playwright script) and check by eye: dark globe, green outlines, amber/red quake dots, top bar not under the nav pill, layer bar at the bottom. Remove any temporary screenshot code before committing.

- [ ] **Step 9: Commit**

```bash
git add godview/index.html godview/css godview/js/app.js godview/manifest.webmanifest godview/icon.svg godview/tests/godview-browser.test.js
git commit -m "godview: the page — spy-console globe, layer bar, details sheet, fallback"
```

---

### Task 5: Wire it into MAZ ARCADE, CI and docs

**Files:**
- Create: `godview/README.md`
- Modify: `shared/projects.js` (new entry after `coda-pics`, before `engine`)
- Modify: `index.html` (root `<noscript>` list, after the `coda-pics/` line)
- Modify: `README.md` (root project table, after the CODA PICS row)
- Modify: `scripts/smoke-site.cjs` (`APPS` array, after `coda-pics`)
- Modify: `.github/workflows/site-ci.yml` (logic step at end of `links` job; browser step at end of `browser` job)

**Interfaces:**
- Consumes: the finished `godview/` from Tasks 1–4.
- Produces: project id `godview` in the shared manifest; CI runs both godview test files.

- [ ] **Step 1: Add the project to `shared/projects.js`**

Insert after the `coda-pics` object (before `{ id: 'engine', ...`):

```js
    {
      id: 'godview',
      name: "GOD'S EYE VIEW",
      kind: 'app',
      path: 'godview/',
      tag: 'live globe of the planet',
      accent: '#1aff8c',
      blurb:
        'A spy-console 3D globe you spin with a finger, showing what the planet is doing ' +
        'right now: every earthquake of the past day from USGS, live, with more layers on ' +
        'the way. Free public data, no account, no key.',
      badges: ['Phone + desktop', '3D globe', 'Live data'],
      docs: 'godview/README.md'
    },
```

- [ ] **Step 2: Add the hub's `<noscript>` link**

In root `index.html`, after `<li><a href="coda-pics/">CODA PICS</a></li>`, add:

```html
          <li><a href="godview/">GOD'S EYE VIEW</a></li>
```

- [ ] **Step 3: Add the root README row**

In root `README.md`, after the `| 🖼️ | **CODA PICS** ...` row, add:

```markdown
| 🌍 | **GOD'S EYE VIEW** — [open](godview/) · [docs](godview/README.md) | A live 3D globe in the spy-console style: spin it with a finger and watch the planet's earthquakes as they happen, from free public data with no account and no key. Satellites, weather, planes, cameras and ships are on the way. |
```

- [ ] **Step 4: Add the app to the smoke test**

In `scripts/smoke-site.cjs`, in `APPS`, after the `coda-pics` line, add:

```js
  { id: 'godview', url: '/godview/', name: "GOD'S EYE VIEW", mode: 'overlay' },
```

- [ ] **Step 5: Run both godview suites in CI**

In `.github/workflows/site-ci.yml`, at the end of the `links` job's steps (after "NEON CELLS logic tests"), add:

```yaml
      # GOD'S EYE VIEW's feed parsers, poller and layers are plain modules:
      # a broken feed, a source going down and coming back, and a layer
      # stopped mid-request are all proved here without a browser.
      - name: GOD'S EYE VIEW logic tests
        run: node godview/tests/godview-logic.test.js
```

At the end of the `browser` job's steps (after "NEON CELLS end to end"), add:

```yaml
      # The globe drawn at phone size, live quakes from a saved feed, the
      # details sheet, a layer toggled, the feed cut, and the globe library
      # unreachable — none of which may throw.
      - name: GOD'S EYE VIEW end to end (phone)
        run: node godview/tests/godview-browser.test.js
```

- [ ] **Step 6: Write `godview/README.md`**

```markdown
# GOD'S EYE VIEW

A live 3D globe in the spy-console style, made for phones. Spin the Earth with a
finger, pinch to zoom, tap anything for its details.

**Open it:** [godview/](./) — on a phone, use your browser's *Add to Home Screen*
and it opens full screen like an app.

## Layers

| Layer | Source | Refresh | Licence |
|---|---|---|---|
| 〰 Earthquakes | [USGS](https://earthquake.usgs.gov/earthquakes/feed/v1.0/geojson.php), every quake of the past day | 60 s | U.S. public domain |

Satellites, weather, planes, cameras and ships are planned — see the
[design](../docs/superpowers/specs/2026-10-21-godview-design.md).

If a source stops answering, its button says **OFFLINE** and it retries on its
own; the rest of the globe keeps working.

## How it is built

A static page — no server, no account, no API key. The globe is
[globe.gl](https://github.com/vasturiano/globe.gl) 2.46.2 (MIT) from jsDelivr.
Country outlines are Natural Earth 1:110m (public domain) via
[world-atlas](https://github.com/topojson/world-atlas) (ISC), converted once by
`tools/build-countries.cjs` into `data/countries.geojson`.

- `js/lib/` — pure logic: feed parsing, the poller, formatting
- `js/layers/` — one file per layer, all on the same small contract
- `js/app.js` — the only file that touches the page or the globe

## Tests

    node godview/tests/godview-logic.test.js        # no dependencies
    npm --prefix music/tests install                # once, for Playwright
    node godview/tests/godview-browser.test.js      # phone-size Chromium

The browser test answers the USGS feed from `tests/fixtures/usgs-sample.json`,
so it never depends on today's earthquakes. Set `GLOBE_JS` to a local copy of
`globe.gl.min.js` to run it with no internet.

Inspired by [God's Eye View](https://github.com/bilawalsidhu/gods-eye-view) by
Bilawal Sidhu (MIT); no code is shared with it.
```

- [ ] **Step 7: Run every gate**

Run:
```bash
node scripts/check-links.mjs && node scripts/check-exchange.mjs
node godview/tests/godview-logic.test.js
GLOBE_JS=<path> node godview/tests/godview-browser.test.js
node scripts/smoke-site.cjs
```
Expected: links and exchange report no problems; `18/18 passed`; `all checks passed`; smoke test passes — but note `smoke-site.cjs` loads godview with the real network: on a box that cannot reach jsDelivr/USGS the godview entry will show the fallback and console network errors. That is expected here and is verified for real by CI (which has internet). Every *other* app in the smoke test must still pass.

- [ ] **Step 8: Commit and push**

```bash
git add godview/README.md shared/projects.js index.html README.md scripts/smoke-site.cjs .github/workflows/site-ci.yml
git commit -m "godview: on the MAZ ARCADE hub, in the nav, and in Site CI"
git push -u origin claude/gods-eye-view-godview
```

- [ ] **Step 9: Cody's phone check**

Cody opens the branch preview or, once merged to `main`, `https://mazoo85.github.io/Maz/godview/` on a phone, and confirms: the globe spins, the QUAKES button shows a number, tapping a dot opens its details. This is the only check that proves USGS answers a real phone browser.

---

## Later steps (each gets its own plan, written when its step starts)

Each later layer is one new file in `godview/js/layers/` on the layer contract above, one new `<script>` tag, one `addLayer(...)` line in `app.js`, tests in both suites, and a README row. Its plan is written only after the previous step passed Cody's phone check, because each depends on a live source this build box cannot reach.

2. **Satellites** — CelesTrak GP JSON (`stations` + `visual` groups), propagated in the page with `satellite.js@7.1.0`; likely needs a contract extension for moving objects (redrawn each frame rather than per fetch).
3. **Weather** — RainViewer radar + NASA GIBS clouds as a semi-transparent globe overlay; needs a tiles/texture channel in `app.js`.
4. **Planes** — adsb.lol point API for the area in view, OpenSky fallback, capped for phone speed.
5. **Cameras** — public camera feeds that permit browser access; still images in the sheet.
6. **Ships** — AISStream; expected to be blocked from browsers. If so, stop and bring Cody the options.
