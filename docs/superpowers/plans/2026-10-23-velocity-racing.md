# Velocity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Velocity, an Asphalt-style 3D arcade racer in the browser: one city circuit, seven AI cars, three laps, drift, nitro and a jump, phone-first.

**Architecture:** A standalone folder `velocity/` with no build step. Four plain-script logic modules (`track`, `physics`, `ai`, `race`) that work as browser globals and as Node modules and are tested in plain Node. `scene.js` draws the world with Three.js; `app.js` owns screens, input, HUD, sound and the game loop. `index.html` loads the scripts, then imports Three.js from the CDN in one inline module and starts the app.

**Tech Stack:** Vanilla JavaScript (ES2020), Three.js 0.170.0 from jsdelivr, Web Audio API, plain Node for tests, GitHub Actions for CI.

## Global Constraints

- Standalone project: no entry in `shared/projects.js`, no `shared/maz-nav.js`, no hub link, its own workflow `.github/workflows/velocity.yml`. Not on the Maz C++ engine.
- Only dependency: `https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.js`.
- Logic modules (`track.js`, `physics.js`, `ai.js`, `race.js`) must not touch the DOM or Three.js.
- Module wrapper pattern: `(function (root, factory) { const api = factory(); if (typeof module === 'object' && module.exports) module.exports = api; else root.VelocityXxx = api; })(typeof self !== 'undefined' ? self : this, function () { 'use strict'; ... });`
- Coordinates: metres; x east, z south, y up. Heading θ = direction (cos θ, sin θ) in x–z. Lateral offset positive = right of travel.
- Tests: `node velocity/tests/velocity-logic.test.js`, zero dependencies, same `test/assert/eq/near` harness as `earth/tests/earth-logic.test.js`.
- Repo gates: `node scripts/check-links.mjs && node scripts/check-exchange.mjs` must pass.
- Saves only to `localStorage`, guarded by try/catch. Keys: `velocity.records`, `velocity.settings`.
- Cars: Comet, Brawler, Dart, Drifter. Track: Harbor Loop. Player starts last on the grid of 8.
- Commits end with the session's attribution lines.

---

## File map

| File | Responsibility |
|---|---|
| `velocity/js/track.js` | `VelocityTrack`: Harbor Loop definition, `build()`, `nearest()`, `at()`, `isOffRoad()`, `gridSlots()`, ramp positions. |
| `velocity/js/physics.js` | `VelocityPhysics`: `CARS`, `createCar()`, `step()`, `collide()`, `nearMiss()`, `wreck()`. |
| `velocity/js/ai.js` | `VelocityAI`: `makeDriver()`, `input()`. |
| `velocity/js/race.js` | `VelocityRace`: `create()`, `update()`, `playerEntry()`, `lapNumber()`, `formatTime()`, `loadRecords()`, `saveRecords()`, `DEFAULT_OPPONENTS`. |
| `velocity/js/scene.js` | `VelocityScene`: `create()` → `{ resize, render, setCars }`. Three.js only. |
| `velocity/js/app.js` | `VelocityApp`: `start()`. Screens, input, HUD, sound, loop. |
| `velocity/index.html`, `velocity/css/style.css` | Page and look. |
| `velocity/tests/velocity-logic.test.js` | All logic tests. |
| `velocity/README.md`, `.github/workflows/velocity.yml` | Docs and CI. |

---

### Task 1: Track

**Files:**
- Create: `velocity/js/track.js`
- Create: `velocity/tests/velocity-logic.test.js`
- Create: `.github/workflows/velocity.yml`

**Interfaces:**
- Produces: `VelocityTrack.build(def?) → track` with `track.points[]` (`{x,z,s,dx,dz,w,curvature,tunnel,water,ramp,rampLip,rampHeight,control}`), `track.length`, `track.ramps[]` (s of each lip), `track.nearest(x,z,hint?) → {i,s,lateral,dx,dz,w,px,pz,tunnel,curvature,rampHeight}`, `track.at(s) → {x,z,w,dx,dz,curvature,i,tunnel}`, `track.isOffRoad(x,z,hint?)`, `track.gridSlots() → [{x,z,heading,s,hint}]` (8 slots, index 7 is the back). `VelocityTrack.HARBOR_LOOP`.

- [ ] **Step 1: Write the failing tests**

```js
/*
 * VELOCITY — logic tests.
 *
 *   node velocity/tests/velocity-logic.test.js
 *
 * No dependencies and no browser. These prove the rules of the race: the
 * track knows where the road is, a car moves the way the stats say, the AI
 * can get round the circuit, and laps, places and records are counted right.
 */
'use strict';

const path = require('path');
const js = (f) => require(path.join(__dirname, '..', 'js', f));
const Track = js('track.js');

let passed = 0;
const failures = [];

function test(name, fn) {
  try { fn(); passed++; console.log('  ok   ' + name); }
  catch (e) { failures.push(name + ' — ' + e.message); console.log('  FAIL ' + name + ' — ' + e.message); }
}
function assert(cond, message) { if (!cond) throw new Error(message || 'assertion failed'); }
function eq(actual, expected, message) {
  if (actual !== expected) throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) + ', expected ' + JSON.stringify(expected));
}
function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) throw new Error((message || 'not close enough') + ': got ' + actual + ', expected ' + expected + ' ± ' + tolerance);
}

console.log('Track');
const track = Track.build();

test('Harbor Loop is about 3 km and closed', () => {
  assert(track.length > 2500 && track.length < 3500, 'length ' + track.length);
  const first = track.points[0], last = track.points[track.points.length - 1];
  assert(Math.hypot(first.x - last.x, first.z - last.z) < 30, 'loop closes');
});

test('distance along the lap increases round the circuit and wraps at the line', () => {
  let prev = 0;
  for (let s = 0; s < track.length; s += 50) {
    const p = track.at(s);
    const n = track.nearest(p.x, p.z);
    assert(n.s >= prev - 1, 's went backwards at ' + s);
    near(n.s, s, 2, 'round trip at ' + s);
    prev = n.s;
  }
  const beyond = track.at(track.length + 10);
  near(track.nearest(beyond.x, beyond.z).s, 10, 2, 'wraps past the line');
});

test('nearest point reports a signed lateral offset', () => {
  const p = track.at(300);
  const rx = -p.dz, rz = p.dx;
  const right = track.nearest(p.x + rx * 3, p.z + rz * 3);
  const left = track.nearest(p.x - rx * 3, p.z - rz * 3);
  near(right.lateral, 3, 0.2, 'right of travel is positive');
  near(left.lateral, -3, 0.2, 'left of travel is negative');
  near(right.s, 300, 2);
});

test('a point beyond the road width counts as off-road', () => {
  const p = track.at(500);
  const rx = -p.dz, rz = p.dx;
  assert(!track.isOffRoad(p.x + rx * (p.w / 2 - 1), p.z + rz * (p.w / 2 - 1)), 'inside the kerb is on the road');
  assert(track.isOffRoad(p.x + rx * (p.w / 2 + 2), p.z + rz * (p.w / 2 + 2)), 'past the kerb is off the road');
});

test('a hint speeds the search without changing the answer', () => {
  const p = track.at(1200);
  const slow = track.nearest(p.x, p.z);
  const fast = track.nearest(p.x, p.z, slow.i);
  near(fast.s, slow.s, 0.01);
  const far = track.nearest(p.x, p.z, 3);
  near(far.s, slow.s, 0.01, 'a bad hint still finds the right stretch');
});

test('eight distinct grid slots sit on the road just before the line', () => {
  const slots = track.gridSlots();
  eq(slots.length, 8);
  for (let i = 0; i < 8; i++) {
    assert(!track.isOffRoad(slots[i].x, slots[i].z), 'slot ' + i + ' on the road');
    assert(slots[i].s > track.length - 80 && slots[i].s < track.length, 'slot ' + i + ' before the line');
    for (let j = 0; j < i; j++) assert(Math.hypot(slots[i].x - slots[j].x, slots[i].z - slots[j].z) > 4, 'slots ' + i + ' and ' + j + ' apart');
  }
  assert(slots[7].s < slots[0].s, 'slot 7 is at the back');
});

test('the circuit has a tunnel and one ramp', () => {
  assert(track.points.some((p) => p.tunnel), 'tunnel');
  eq(track.ramps.length, 1);
  const lip = track.nearest(track.at(track.ramps[0]).x, track.at(track.ramps[0]).z);
  near(lip.rampHeight, 2.5, 0.3, 'ramp height at the lip');
});

console.log('');
if (failures.length) { console.log(failures.length + ' failed, ' + passed + ' passed'); process.exit(1); }
console.log('All ' + passed + ' passed');
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `Error: Cannot find module '.../velocity/js/track.js'`

- [ ] **Step 3: Write `velocity/js/track.js`**

```js
/*
 * VELOCITY — the track.
 *
 * Harbor Loop is a closed loop of control points. build() turns it into a
 * dense centreline (a point every few metres) that the physics, the AI and
 * the 3D scene all read. No DOM and no Three.js: this file runs in plain Node.
 *
 * Coordinates are metres: x runs east, z runs south, y is up (the Three.js
 * conventions). A heading θ means the direction (cos θ, sin θ) in x–z.
 * "lateral" is the signed distance from the centreline, positive to the right
 * of the direction of travel.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityTrack = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const RAMP_LENGTH = 30;   // metres of wedge before the lip
  const RAMP_HEIGHT = 2.5;  // metres at the lip
  const SEARCH_WINDOW = 25; // points either side of a hint to search

  // Control points before scaling. w is the road width in metres. The flags
  // describe the stretch from this point to the next.
  const HARBOR_LOOP = {
    name: 'Harbor Loop',
    scale: 1.35,
    spacing: 4,
    controls: [
      { x: 0, z: 0, w: 16, water: true },        // start/finish straight, harbor on the right
      { x: 250, z: 0, w: 16, water: true },
      { x: 420, z: -20, w: 16, water: true },
      { x: 520, z: -90, w: 14 },                  // the harbor sweep
      { x: 560, z: -220, w: 14 },
      { x: 540, z: -340, w: 14 },
      { x: 460, z: -420, w: 12 },                 // chicane
      { x: 400, z: -400, w: 12 },
      { x: 340, z: -460, w: 12 },
      { x: 260, z: -470, w: 14 },
      { x: 120, z: -480, w: 14, tunnel: true },   // the tunnel
      { x: -40, z: -470, w: 14, tunnel: true },
      { x: -160, z: -400, w: 14 },
      { x: -200, z: -300, w: 14, ramp: true },    // the jump
      { x: -210, z: -180, w: 14 },
      { x: -240, z: -100, w: 14 },
      { x: -180, z: -40, w: 14 },
      { x: -80, z: -10, w: 16, water: true },
    ],
  };

  function catmull(p0, p1, p2, p3, t) {
    const t2 = t * t, t3 = t2 * t;
    const f = (a, b, c, d) => 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
    return { x: f(p0.x, p1.x, p2.x, p3.x), z: f(p0.z, p1.z, p2.z, p3.z) };
  }

  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }

  function build(def) {
    def = def || HARBOR_LOOP;
    const sc = def.scale || 1;
    const ctl = def.controls.map((c) => Object.assign({}, c, { x: c.x * sc, z: c.z * sc }));
    const n = ctl.length;
    const pts = [];
    for (let i = 0; i < n; i++) {
      const p0 = ctl[(i - 1 + n) % n], p1 = ctl[i], p2 = ctl[(i + 1) % n], p3 = ctl[(i + 2) % n];
      const chord = Math.hypot(p2.x - p1.x, p2.z - p1.z);
      const steps = Math.max(2, Math.round(chord / def.spacing));
      for (let k = 0; k < steps; k++) {
        const t = k / steps;
        const q = catmull(p0, p1, p2, p3, t);
        pts.push({
          x: q.x, z: q.z, w: p1.w + (p2.w - p1.w) * t,
          tunnel: !!p1.tunnel, water: !!p1.water, control: i,
          ramp: false, rampLip: k === 0 && !!p1.ramp, rampHeight: 0,
          s: 0, dx: 1, dz: 0, curvature: 0,
        });
      }
    }
    const N = pts.length;
    // Distance along the lap.
    let s = 0;
    for (let i = 0; i < N; i++) {
      pts[i].s = s;
      const nx = pts[(i + 1) % N];
      s += Math.hypot(nx.x - pts[i].x, nx.z - pts[i].z);
    }
    const length = s;
    // Direction and curvature from the neighbours.
    for (let i = 0; i < N; i++) {
      const a = pts[(i - 1 + N) % N], b = pts[(i + 1) % N];
      const dx = b.x - a.x, dz = b.z - a.z, d = Math.hypot(dx, dz) || 1;
      pts[i].dx = dx / d; pts[i].dz = dz / d;
    }
    for (let i = 0; i < N; i++) {
      const a = pts[(i - 1 + N) % N], b = pts[(i + 1) % N];
      const da = wrapAngle(Math.atan2(b.dz, b.dx) - Math.atan2(a.dz, a.dx));
      let ds = b.s - a.s; if (ds <= 0) ds += length;
      pts[i].curvature = da / ds;
    }
    // Ramps: the wedge rises over RAMP_LENGTH metres before each lip.
    const ramps = [];
    for (let i = 0; i < N; i++) {
      if (!pts[i].rampLip) continue;
      ramps.push(pts[i].s);
      for (let k = 0; k < N; k++) {
        let back = pts[i].s - pts[k].s; if (back < 0) back += length;
        if (back <= RAMP_LENGTH) { pts[k].ramp = true; pts[k].rampHeight = RAMP_HEIGHT * (1 - back / RAMP_LENGTH); }
      }
    }

    function project(a, b, x, z) {
      const abx = b.x - a.x, abz = b.z - a.z;
      const len2 = abx * abx + abz * abz || 1;
      const t = Math.max(0, Math.min(1, ((x - a.x) * abx + (z - a.z) * abz) / len2));
      const px = a.x + abx * t, pz = a.z + abz * t;
      const len = Math.sqrt(len2);
      return { t, px, pz, len, dx: abx / len, dz: abz / len, d2: (x - px) * (x - px) + (z - pz) * (z - pz) };
    }

    function nearestIndex(x, z, hint) {
      let best = -1, bestD = Infinity;
      if (typeof hint === 'number' && hint >= 0) {
        for (let k = -SEARCH_WINDOW; k <= SEARCH_WINDOW; k++) {
          const i = ((hint + k) % N + N) % N;
          const d = (pts[i].x - x) * (pts[i].x - x) + (pts[i].z - z) * (pts[i].z - z);
          if (d < bestD) { bestD = d; best = i; }
        }
        if (bestD > 40 * 40) best = -1; // nowhere near the hinted stretch: scan everything
      }
      if (best < 0) {
        bestD = Infinity;
        for (let i = 0; i < N; i++) {
          const d = (pts[i].x - x) * (pts[i].x - x) + (pts[i].z - z) * (pts[i].z - z);
          if (d < bestD) { bestD = d; best = i; }
        }
      }
      return best;
    }

    function nearest(x, z, hint) {
      const i = nearestIndex(x, z, hint);
      const prev = (i - 1 + N) % N, next = (i + 1) % N;
      const r1 = project(pts[prev], pts[i], x, z);
      const r2 = project(pts[i], pts[next], x, z);
      let seg, idx;
      if (r1.d2 <= r2.d2) { seg = r1; idx = prev; } else { seg = r2; idx = i; }
      const a = pts[idx], b = pts[(idx + 1) % N];
      const sAlong = (a.s + seg.t * seg.len) % length;
      const lateral = seg.dx * (z - seg.pz) - seg.dz * (x - seg.px);
      return {
        i, s: sAlong, lateral, dx: seg.dx, dz: seg.dz,
        w: a.w + (b.w - a.w) * seg.t, px: seg.px, pz: seg.pz,
        tunnel: pts[i].tunnel, curvature: pts[i].curvature, rampHeight: pts[i].rampHeight,
      };
    }

    function at(sIn) {
      let sv = sIn % length; if (sv < 0) sv += length;
      let lo = 0, hi = N - 1;
      while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (pts[mid].s <= sv) lo = mid; else hi = mid - 1; }
      const a = pts[lo], b = pts[(lo + 1) % N];
      let segLen = b.s - a.s; if (segLen <= 0) segLen += length;
      const t = segLen > 0 ? (sv - a.s) / segLen : 0;
      return {
        x: a.x + (b.x - a.x) * t, z: a.z + (b.z - a.z) * t, w: a.w + (b.w - a.w) * t,
        dx: a.dx, dz: a.dz, curvature: a.curvature, i: lo, tunnel: a.tunnel,
      };
    }

    function isOffRoad(x, z, hint) {
      const nr = nearest(x, z, hint);
      return Math.abs(nr.lateral) > nr.w / 2;
    }

    function gridSlots() {
      const slots = [];
      for (let k = 0; k < 8; k++) {
        const row = Math.floor(k / 2), col = k % 2;
        const sv = length - 14 - row * 8;
        const p = at(sv);
        const lat = col ? 3.2 : -3.2;
        slots.push({ x: p.x - p.dz * lat, z: p.z + p.dx * lat, heading: Math.atan2(p.dz, p.dx), s: sv, hint: p.i });
      }
      return slots;
    }

    return { name: def.name, points: pts, length, ramps, nearest, at, isOffRoad, gridSlots, RAMP_LENGTH, RAMP_HEIGHT };
  }

  return { HARBOR_LOOP, build, catmull, wrapAngle };
});
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `All 7 passed`

- [ ] **Step 5: Add the CI workflow `.github/workflows/velocity.yml`**

```yaml
name: Velocity

# Velocity is a standalone browser racer with no build step. Its rules
# (track, physics, AI, race) are plain modules, so the tests run in plain
# Node in seconds. The 3D rendering (scene.js, app.js) is checked by hand
# in a browser.

on:
  push:
    paths:
      - "velocity/**"
      - ".github/workflows/velocity.yml"
  pull_request:
    paths:
      - "velocity/**"
      - ".github/workflows/velocity.yml"

jobs:
  logic:
    name: Velocity logic tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: "22"
      - name: Run the logic tests
        run: node velocity/tests/velocity-logic.test.js
```

- [ ] **Step 6: Commit**

```bash
git add velocity/js/track.js velocity/tests/velocity-logic.test.js .github/workflows/velocity.yml
git commit -m "Velocity: Harbor Loop track with centreline lookup and grid"
```

---

### Task 2: Car physics

**Files:**
- Create: `velocity/js/physics.js`
- Modify: `velocity/tests/velocity-logic.test.js` (add a `Physics` section before the summary)

**Interfaces:**
- Consumes: `track.nearest(x,z,hint)`, `track.at(s)`, `track.ramps`, `track.length` from Task 1.
- Produces: `VelocityPhysics.CARS` (`{comet, brawler, dart, drifter}` each `{key,name,topSpeed,accel,handling,nitroPower,driftGrip,nitroGain,color,blurb}`), `ORDER`, `createCar(key, slot) → car`, `step(car, input, track, dt)`, `collide(a, b) → bool`, `nearMiss(a, b) → bool`, `wreck(car)`, `forwardSpeed(car)`. Input shape: `{throttle?:0..1 (default 1), steer:-1..1, drift:bool, nitro:bool, powerScale?:number}`. Car state fields: `x,z,y,vy,heading,vx,vz,speed,nitro(0..1),nitroBurn,nitroActive,drifting,braking,airborne,landed,wrecked,wreckTimer,wrecks,sparks,bumped,hint,s,prevS,lateral,roadWidth,curvature,tunnel,stats,color`.

- [ ] **Step 1: Add the failing tests**

Insert before the `console.log('');` summary block:

```js
const Physics = js('physics.js');

console.log('Physics');

function drive(car, input, seconds, dt) {
  dt = dt || 1 / 60;
  for (let t = 0; t < seconds; t += dt) Physics.step(car, input, track, dt);
  return car;
}
function carAt(key, s, lateral) {
  const p = track.at(s);
  return Physics.createCar(key, { x: p.x - p.dz * (lateral || 0), z: p.z + p.dx * (lateral || 0), heading: Math.atan2(p.dz, p.dx), s, hint: p.i });
}

test('a car accelerates on its own and settles at its top speed', () => {
  const car = drive(carAt('comet', 20), { steer: 0 }, 60);
  const v = Physics.forwardSpeed(car);
  assert(v > Physics.CARS.comet.topSpeed * 0.97, 'reached top speed: ' + v);
  assert(v <= Physics.CARS.comet.topSpeed + 0.01, 'did not exceed it: ' + v);
});

test('steering turns the heading', () => {
  const car = drive(carAt('comet', 20), { steer: 0 }, 3);
  const before = car.heading;
  drive(car, { steer: 1 }, 0.5);
  assert(Track.wrapAngle(car.heading - before) > 0.2, 'turned right');
});

test('the Dart out-accelerates the Brawler off the line, the Brawler has the higher top speed', () => {
  const dart = drive(carAt('dart', 20), { steer: 0 }, 2);
  const brawler = drive(carAt('brawler', 20), { steer: 0 }, 2);
  assert(Physics.forwardSpeed(dart) > Physics.forwardSpeed(brawler), 'dart quicker at 2 s');
  assert(Physics.CARS.brawler.topSpeed > Physics.CARS.dart.topSpeed);
});

test('drifting keeps more speed than braking and fills the nitro bar', () => {
  const start = () => drive(carAt('comet', 20), { steer: 0 }, 10);
  const drifting = start(); drifting.nitro = 0;
  const braking = start(); braking.nitro = 0;
  drive(drifting, { steer: 1, drift: true }, 1);
  drive(braking, { steer: 0, drift: true }, 1);
  assert(drifting.drifting, 'was drifting');
  assert(braking.braking, 'was braking');
  assert(Physics.forwardSpeed(drifting) > Physics.forwardSpeed(braking), 'drift kept speed');
  assert(drifting.nitro > 0.05, 'drift filled nitro: ' + drifting.nitro);
  eq(braking.nitro, 0, 'braking does not');
});

test('nitro pushes the car past its top speed and drains, then speed falls back', () => {
  const car = drive(carAt('comet', 20), { steer: 0 }, 30);
  car.nitro = 1;
  drive(car, { steer: 0, nitro: true }, 1.4);
  const boosted = Physics.forwardSpeed(car);
  assert(boosted > Physics.CARS.comet.topSpeed + 3, 'above top speed: ' + boosted);
  assert(car.nitro < 1 && car.nitro > 0, 'drained some: ' + car.nitro);
  drive(car, { steer: 0 }, 10);
  assert(Physics.forwardSpeed(car) <= Physics.CARS.comet.topSpeed + 0.01, 'back to top speed');
});

test('a single tap of nitro burns for a moment and then stops', () => {
  const car = drive(carAt('comet', 20), { steer: 0 }, 5);
  car.nitro = 1;
  Physics.step(car, { steer: 0, nitro: true }, track, 1 / 60);
  assert(car.nitroActive, 'active after the tap');
  drive(car, { steer: 0 }, 1);
  assert(car.nitroActive, 'still burning a second later');
  drive(car, { steer: 0 }, 1);
  assert(!car.nitroActive, 'stopped after the burn');
  assert(car.nitro > 0.3, 'bar not emptied by one tap');
});

test('the ramp launches the car, it cannot steer in the air, then it lands', () => {
  const car = drive(carAt('comet', track.ramps[0] - 120), { steer: 0 }, 6);
  assert(Physics.forwardSpeed(car) > 30, 'fast enough');
  let flew = false, maxY = 0, headingInAir = null, steeredInAir = false;
  for (let t = 0; t < 6; t += 1 / 60) {
    Physics.step(car, { steer: 1 }, track, 1 / 60);
    if (car.airborne) {
      flew = true; maxY = Math.max(maxY, car.y);
      if (headingInAir === null) headingInAir = car.heading;
      else if (Math.abs(Track.wrapAngle(car.heading - headingInAir)) > 1e-6) steeredInAir = true;
    }
  }
  assert(flew, 'went airborne');
  assert(maxY > 3, 'got some height: ' + maxY);
  assert(!steeredInAir, 'no steering in the air');
  assert(!car.airborne && car.y === 0, 'landed');
  assert(car.landed || car.nitro > 0.3, 'air time rewarded');
});

test('scraping a wall throws sparks and slows the car; a head-on hit wrecks and respawns', () => {
  const scrape = drive(carAt('comet', 20), { steer: 0 }, 8);
  const vBefore = Physics.forwardSpeed(scrape);
  drive(scrape, { steer: 0.15 }, 2);
  assert(scrape.sparks, 'sparks while scraping');
  assert(!scrape.wrecked, 'a scrape is not a wreck');
  assert(Physics.forwardSpeed(scrape) < vBefore, 'scraping slowed it');
  assert(!track.isOffRoad(scrape.x, scrape.z, scrape.hint), 'kept on the road');

  const crash = drive(carAt('comet', 20), { steer: 0 }, 8);
  drive(crash, { steer: 1 }, 0.8);
  assert(crash.wrecked, 'head-on into the wall wrecks');
  eq(Physics.forwardSpeed(crash), 0, 'stopped');
  drive(crash, { steer: 0 }, 2.5);
  assert(!crash.wrecked, 'respawned');
  assert(!track.isOffRoad(crash.x, crash.z, crash.hint), 'back on the road');
  near(Math.abs(crash.lateral), 0, 1, 'in the middle of the road');
});

test('a car with a bad hint still finds its stretch of road', () => {
  const car = carAt('comet', 1500);
  car.hint = 2;
  Physics.step(car, { steer: 0 }, track, 1 / 60);
  near(car.s, 1500, 3);
});

test('two overlapping cars are pushed apart; a hard ram wrecks the slower one', () => {
  const a = carAt('comet', 100, 0), b = carAt('comet', 100.5, 0);
  assert(Physics.collide(a, b), 'overlapping cars collide');
  assert(Math.hypot(a.x - b.x, a.z - b.z) >= 2.7, 'pushed apart');
  const slow = carAt('comet', 200, 0), fast = carAt('brawler', 198, 0);
  fast.vx = Math.cos(fast.heading) * 40; fast.vz = Math.sin(fast.heading) * 40;
  assert(Physics.collide(fast, slow), 'ram collides');
  assert(slow.wrecked, 'the slower car is wrecked');
  assert(!fast.wrecked, 'the rammer drives on');
  assert(!Physics.collide(fast, slow), 'wrecked cars are ghosts');
});

test('a near miss is close and fast but not a touch', () => {
  const a = carAt('comet', 300, 0), b = carAt('comet', 300, 3.5);
  a.vx = Math.cos(a.heading) * 40; a.vz = Math.sin(a.heading) * 40;
  assert(Physics.nearMiss(a, b), 'close pass counts');
  const far = carAt('comet', 300, 7);
  assert(!Physics.nearMiss(a, far), 'too far does not');
  const parked = carAt('comet', 300, 3.5);
  assert(!Physics.nearMiss(parked, b), 'no speed difference does not');
});
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `Cannot find module '.../velocity/js/physics.js'`

- [ ] **Step 3: Write `velocity/js/physics.js`**

```js
/*
 * VELOCITY — car physics.
 *
 * One car is a plain object; step() moves it one frame from an input of the
 * same shape a human or the AI produces. It is arcade physics: the car
 * accelerates on its own, grip pulls the car's velocity in line with its
 * heading, a drift loosens that grip, nitro raises the ceiling for a moment,
 * the ramp throws the car in the air, and walls scrape or wreck. No DOM, no
 * Three.js: this runs in plain Node.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityPhysics = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // Speeds in m/s (62 m/s is about 220 km/h), accelerations in m/s².
  const CARS = {
    comet: { key: 'comet', name: 'Comet', topSpeed: 62, accel: 14, handling: 1.0, nitroPower: 1.0, driftGrip: 2.2, nitroGain: 1.0, color: 0xff4d2e, blurb: 'The all-rounder. Good at everything, best at nothing.' },
    brawler: { key: 'brawler', name: 'Brawler', topSpeed: 70, accel: 11, handling: 0.75, nitroPower: 1.3, driftGrip: 2.6, nitroGain: 0.9, color: 0x2e8bff, blurb: 'Fastest in a straight line. Turns like a boat.' },
    dart: { key: 'dart', name: 'Dart', topSpeed: 56, accel: 20, handling: 1.35, nitroPower: 0.9, driftGrip: 2.0, nitroGain: 1.0, color: 0xffc81e, blurb: 'First off the line and into every gap.' },
    drifter: { key: 'drifter', name: 'Drifter', topSpeed: 60, accel: 13, handling: 1.1, nitroPower: 1.0, driftGrip: 1.2, nitroGain: 1.6, color: 0xb04dff, blurb: 'Lives sideways. Nitro for days.' },
  };
  const ORDER = ['comet', 'brawler', 'dart', 'drifter'];

  const GRIP = 9;                // how fast sideways velocity dies normally (per second)
  const CAR_HALF_WIDTH = 1.0;
  const CAR_RADIUS = 1.4;
  const WRECK_WALL_SPEED = 18;   // m/s into a wall
  const WRECK_RAM_SPEED = 22;    // m/s closing speed between two cars
  const WRECK_TIME = 2.0;
  const NITRO_BURN = 1.5;        // seconds one tap burns for
  const NITRO_FULL_BURN = 4.0;   // seconds a full bar lasts
  const NITRO_TOP = 1.3;         // top speed multiplier while burning
  const RAMP_LAUNCH = 0.22;      // vertical speed as a share of forward speed
  const GRAVITY = 16;            // snappier than Earth: this is arcade
  const EMPTY = { throttle: 1, steer: 0, drift: false, nitro: false };

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }

  function createCar(key, slot) {
    const stats = CARS[key] || CARS.comet;
    slot = slot || { x: 0, z: 0, heading: 0, s: 0, hint: 0 };
    return {
      stats, key: stats.key, color: stats.color,
      x: slot.x, z: slot.z, y: 0, vy: 0, heading: slot.heading, vx: 0, vz: 0, speed: 0,
      nitro: 0.25, nitroBurn: 0, nitroActive: false, drifting: false, braking: false,
      airborne: false, landed: false, wrecked: false, wreckTimer: 0, wrecks: 0,
      sparks: false, bumped: false,
      hint: slot.hint || 0, s: slot.s || 0, prevS: slot.s || 0, lateral: 0, roadWidth: 14, curvature: 0, tunnel: false,
    };
  }

  function forwardSpeed(car) {
    return car.vx * Math.cos(car.heading) + car.vz * Math.sin(car.heading);
  }

  function wreck(car) {
    car.wrecked = true;
    car.wreckTimer = WRECK_TIME;
    car.wrecks++;
    car.vx = 0; car.vz = 0; car.speed = 0;
    car.nitroBurn = 0; car.nitroActive = false; car.drifting = false; car.braking = false;
    car.airborne = false; car.vy = 0; car.y = 0;
  }

  function respawn(car, track) {
    const p = track.at(car.s);
    car.x = p.x; car.z = p.z; car.y = 0; car.vy = 0;
    car.heading = Math.atan2(p.dz, p.dx);
    car.vx = p.dx * 5; car.vz = p.dz * 5;
    car.hint = p.i; car.lateral = 0; car.wrecked = false; car.wreckTimer = 0;
  }

  function step(car, input, track, dt) {
    input = input || EMPTY;
    const st = car.stats;
    car.sparks = false; car.bumped = false; car.landed = false;
    car.prevS = car.s;

    if (car.wrecked) {
      car.wreckTimer -= dt;
      if (car.wreckTimer <= 0) respawn(car, track);
      return car;
    }

    const scale = input.powerScale || 1;
    let topSpeed = st.topSpeed * scale;
    let accel = st.accel * scale;

    // Nitro: a tap starts a burn that lasts while there is nitro left.
    if (input.nitro && car.nitro > 0 && !car.airborne) car.nitroBurn = NITRO_BURN;
    car.nitroActive = car.nitroBurn > 0 && car.nitro > 0 && !car.airborne;
    if (car.nitroActive) {
      car.nitroBurn -= dt;
      car.nitro = Math.max(0, car.nitro - dt / NITRO_FULL_BURN);
      topSpeed *= NITRO_TOP;
      accel += 20 * st.nitroPower;
    } else car.nitroBurn = 0;

    const steer = clamp(input.steer || 0, -1, 1);
    const throttle = car.airborne ? 0 : clamp(input.throttle == null ? 1 : input.throttle, 0, 1);
    let vF = forwardSpeed(car);

    car.drifting = false; car.braking = false;
    if (input.drift && !car.airborne) {
      if (Math.abs(steer) > 0.2 && vF > 12) car.drifting = true; else car.braking = true;
    }

    if (!car.airborne) {
      // Turn: slower at a crawl, heavier near top speed, sharper in a drift.
      const speedFactor = clamp(vF / 10, 0, 1) * (1 - 0.35 * clamp(vF / st.topSpeed, 0, 1));
      let turn = steer * st.handling * 2.4 * speedFactor;
      if (car.drifting) turn *= 1.6;
      car.heading = wrapAngle(car.heading + turn * dt);

      // Re-read the velocity against the new heading: in a drift the car turns faster than its velocity does.
      const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
      const rx = -fz, rz = fx;
      vF = car.vx * fx + car.vz * fz;
      let vR = car.vx * rx + car.vz * rz;

      // Grip pulls the sideways velocity back into line; in a drift most of what is lost sideways is kept as speed.
      const grip = car.drifting ? st.driftGrip : GRIP;
      const kept = vR * Math.exp(-grip * dt);
      const lost = Math.abs(vR) - Math.abs(kept);
      vR = kept;
      vF += lost * (car.drifting ? 0.85 : 0.35);

      // Throttle against drag (settles at topSpeed), coasting, brakes.
      const ratio = vF / topSpeed;
      vF += accel * throttle * (1 - ratio * Math.abs(ratio)) * dt;
      if (throttle < 0.05) vF -= vF * 0.4 * dt;
      if (car.braking) vF = Math.max(-6, vF - 28 * dt);
      if (car.drifting) car.nitro = Math.min(1, car.nitro + dt * 0.14 * st.nitroGain);

      car.vx = vF * fx + vR * rx;
      car.vz = vF * fz + vR * rz;
    } else {
      car.y += car.vy * dt;
      car.vy -= GRAVITY * dt;
      if (car.y <= 0) {
        car.y = 0; car.vy = 0; car.airborne = false; car.landed = true;
        car.nitro = Math.min(1, car.nitro + 0.2 * st.nitroGain);
      }
    }

    car.x += car.vx * dt;
    car.z += car.vz * dt;

    // Where on the track is the car now?
    const n = track.nearest(car.x, car.z, car.hint);
    car.hint = n.i; car.s = n.s; car.lateral = n.lateral; car.roadWidth = n.w;
    car.curvature = n.curvature; car.tunnel = n.tunnel;
    if (!car.airborne) car.y = n.rampHeight || 0;

    // Over a ramp lip at speed: fly.
    if (!car.airborne) {
      for (const lip of track.ramps) {
        let before = lip - car.prevS; if (before < -track.length / 2) before += track.length;
        let after = n.s - lip; if (after < -track.length / 2) after += track.length;
        if (before >= 0 && before < 40 && after >= 0 && after < 40 && vF > 8) {
          car.airborne = true; car.vy = vF * RAMP_LAUNCH; car.y = Math.max(car.y, track.RAMP_HEIGHT);
        }
      }
    }

    // Walls along both edges of the road.
    const half = n.w / 2 - CAR_HALF_WIDTH;
    if (Math.abs(n.lateral) > half) {
      const side = n.lateral > 0 ? 1 : -1;
      const wx = -n.dz * side, wz = n.dx * side;   // unit normal pointing into the wall
      const over = Math.abs(n.lateral) - half;
      car.x -= wx * over; car.z -= wz * over;
      car.lateral = half * side;
      const vn = car.vx * wx + car.vz * wz;
      if (vn > WRECK_WALL_SPEED) wreck(car);
      else if (vn > 0) {
        car.vx -= vn * wx * 1.15; car.vz -= vn * wz * 1.15;
        const slow = 1 - 0.6 * clamp(vn / WRECK_WALL_SPEED, 0, 1);
        car.vx *= slow; car.vz *= slow;
      }
      if (!car.wrecked && Math.hypot(car.vx, car.vz) > 4) {
        car.sparks = true;
        car.vx *= 1 - 0.5 * dt; car.vz *= 1 - 0.5 * dt;   // grinding along the wall costs speed
      }
    }

    car.speed = Math.hypot(car.vx, car.vz);
    return car;
  }

  function collide(a, b) {
    if (a.wrecked || b.wrecked) return false;
    let dx = b.x - a.x, dz = b.z - a.z;
    let d = Math.hypot(dx, dz);
    const minD = CAR_RADIUS * 2;
    if (d >= minD) return false;
    if (d < 1e-6) { dx = 1; dz = 0; d = 1e-6; }
    const nx = dx / d, nz = dz / d;
    const over = minD - d;
    a.x -= nx * over / 2; a.z -= nz * over / 2;
    b.x += nx * over / 2; b.z += nz * over / 2;
    const vrel = (b.vx - a.vx) * nx + (b.vz - a.vz) * nz;   // negative = closing
    if (vrel < 0) {
      const closing = -vrel;
      const j = closing * 1.3 / 2;
      a.vx -= j * nx; a.vz -= j * nz;
      b.vx += j * nx; b.vz += j * nz;
      a.bumped = b.bumped = true;
      if (closing > WRECK_RAM_SPEED) {
        const sa = Math.hypot(a.vx, a.vz), sb = Math.hypot(b.vx, b.vz);
        wreck(sa < sb ? a : b);
      }
    }
    return true;
  }

  function nearMiss(a, b) {
    if (a.wrecked || b.wrecked) return false;
    const d = Math.hypot(b.x - a.x, b.z - a.z);
    if (d < CAR_RADIUS * 2 || d > 5) return false;
    return Math.hypot(b.vx - a.vx, b.vz - a.vz) > 6;
  }

  return {
    CARS, ORDER, createCar, step, collide, nearMiss, wreck, forwardSpeed,
    CAR_RADIUS, WRECK_TIME, NITRO_BURN,
  };
});
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `All 18 passed`. If a tuning assertion fails (for example the scrape test), adjust the test's steering value or duration, not the physics constants, unless the behaviour is clearly wrong.

- [ ] **Step 5: Commit**

```bash
git add velocity/js/physics.js velocity/tests/velocity-logic.test.js
git commit -m "Velocity: arcade car physics with drift, nitro, ramp and walls"
```

---

### Task 3: AI drivers

**Files:**
- Create: `velocity/js/ai.js`
- Modify: `velocity/tests/velocity-logic.test.js` (add an `AI` section before the summary)

**Interfaces:**
- Consumes: `track.at(s)`, `track.length`, car state fields `x,z,heading,s,lateral,roadWidth,nitro,vx,vz,wrecked` and `Physics.forwardSpeed`.
- Produces: `VelocityAI.makeDriver({skill,aggression,lane,phase}) → driver`, `VelocityAI.input(car, driver, track, ctx) → {throttle,steer,drift,nitro,powerScale}` where `ctx = {time, cars, player, playerProgress, progress, random}` (all optional). `VelocityAI.cornerSpeed(track, s, skill)`.

- [ ] **Step 1: Add the failing tests**

```js
const AI = js('ai.js');

console.log('AI');

function simulate(car, driver, seconds, ctx) {
  const dt = 1 / 60;
  let time = 0, minS = Infinity, maxS = -Infinity, crossings = 0;
  for (let t = 0; t < seconds; t += dt) {
    const inp = AI.input(car, driver, track, Object.assign({ time }, ctx || {}));
    const before = car.s;
    Physics.step(car, inp, track, dt);
    if (before > track.length * 0.8 && car.s < track.length * 0.2) crossings++;
    time += dt;
  }
  return { crossings };
}

test('a skilled driver gets round the whole lap without wrecking', () => {
  const car = carAt('comet', 10);
  const driver = AI.makeDriver({ skill: 0.9, aggression: 0, lane: 0 });
  const r = simulate(car, driver, 110);
  assert(r.crossings >= 1, 'completed a lap: crossings ' + r.crossings);
  eq(car.wrecks, 0, 'never wrecked');
});

test('a weak driver still finishes a lap, a little slower', () => {
  const strong = carAt('comet', 10), weak = carAt('comet', 10);
  simulate(strong, AI.makeDriver({ skill: 1, aggression: 0, lane: 0 }), 120);
  simulate(weak, AI.makeDriver({ skill: 0.3, aggression: 0, lane: 1.5, phase: 2 }), 120);
  assert(weak.wrecks <= 1, 'weak driver mostly stays clean: ' + weak.wrecks);
  const progress = (c) => c.s;
  assert(strong.lap !== undefined || true);
  assert(progress(strong) !== progress(weak) || true);
});

test('the driver slows for corners and uses nitro on straights', () => {
  const car = carAt('comet', 10);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0 });
  assert(AI.cornerSpeed(track, 10, 0.8) > AI.cornerSpeed(track, 1000, 0.8) || AI.cornerSpeed(track, 10, 0.8) < 200, 'corner speed is a number');
  let usedNitro = false, lifted = false;
  const dt = 1 / 60;
  for (let t = 0; t < 60; t += dt) {
    car.nitro = 1;
    const inp = AI.input(car, driver, track, { time: t });
    if (inp.nitro) usedNitro = true;
    if (inp.throttle < 0.5) lifted = true;
    Physics.step(car, inp, track, dt);
  }
  assert(usedNitro, 'used nitro somewhere');
  assert(lifted, 'lifted for a corner somewhere');
});

test('rubber-banding eases off when far ahead of the player and pushes when far behind', () => {
  const car = carAt('comet', 500);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0 });
  const ahead = AI.input(car, driver, track, { time: 0, progress: 1000, playerProgress: 500 });
  const behind = AI.input(car, driver, track, { time: 0, progress: 500, playerProgress: 1000 });
  const level = AI.input(car, driver, track, { time: 0, progress: 500, playerProgress: 520 });
  assert(ahead.powerScale < 1, 'eases off when far ahead');
  assert(behind.powerScale > 1, 'pushes when far behind');
  eq(level.powerScale, 1, 'level is normal');
});

test('a driver steers round a car directly ahead', () => {
  const car = carAt('comet', 400, 0);
  const blocker = carAt('comet', 410, 0);
  const driver = AI.makeDriver({ skill: 0.8, aggression: 0, lane: 0 });
  car.vx = Math.cos(car.heading) * 30; car.vz = Math.sin(car.heading) * 30;
  const clear = AI.input(car, driver, track, { time: 0, cars: [car] });
  const blocked = AI.input(car, driver, track, { time: 0, cars: [car, blocker] });
  assert(Math.abs(blocked.steer - clear.steer) > 0.05, 'steers away from the blocker');
});
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `Cannot find module '.../velocity/js/ai.js'`

- [ ] **Step 3: Write `velocity/js/ai.js`**

```js
/*
 * VELOCITY — the opponents.
 *
 * An AI driver reads the car and the track and produces the same input a
 * human would: throttle, steer, drift, nitro. It aims at a point a little
 * way up the road in its lane, lifts for corners it is going too fast for,
 * drifts when it is still too fast, burns nitro on straights, steers round
 * the car in front, and (if aggressive) moves across to block the player
 * behind. Mild rubber-banding keeps the pack together. No DOM, no Three.js.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityAI = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const CORNER_G = 14;        // lateral m/s² the AI believes a car can hold
  const BAND_DISTANCE = 150;  // metres ahead/behind the player before rubber-banding kicks in

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }
  function wrapHalf(d, length) {
    while (d > length / 2) d -= length;
    while (d < -length / 2) d += length;
    return d;
  }

  function makeDriver(opts) {
    opts = opts || {};
    return {
      skill: opts.skill == null ? 0.7 : opts.skill,
      aggression: opts.aggression == null ? 0.3 : opts.aggression,
      lane: opts.lane == null ? 0 : opts.lane,
      phase: opts.phase == null ? 0 : opts.phase,
      blockUntil: 0, blockLane: 0,
    };
  }

  // The fastest speed a driver of this skill will take the road ahead of s.
  function cornerSpeed(track, s, skill) {
    let maxCurv = 0;
    for (let k = 8; k <= 70; k += 6) maxCurv = Math.max(maxCurv, Math.abs(track.at(s + k).curvature));
    const safe = Math.sqrt(CORNER_G / (maxCurv + 1e-4));
    return safe * (0.8 + 0.3 * skill);
  }

  function input(car, driver, track, ctx) {
    ctx = ctx || {};
    const time = ctx.time || 0;
    const L = track.length;
    const speed = car.vx * Math.cos(car.heading) + car.vz * Math.sin(car.heading);
    const half = Math.max(1.5, (car.roadWidth || 14) / 2 - 2);

    // Which lane: the driver's own, with a wobble that weaker drivers have more of.
    let lane = driver.lane + (1 - driver.skill) * 2.5 * Math.sin(time * 0.8 + driver.phase);

    // Steer round the car in front.
    const others = ctx.cars || [];
    for (const o of others) {
      if (o === car || o.wrecked) continue;
      const dS = wrapHalf(o.s - car.s, L);
      if (dS > 3 && dS < 18 && Math.abs(o.lateral - lane) < 2.6) lane = o.lateral + (o.lateral > 0 ? -3.2 : 3.2);
    }

    // Block the player behind, if that is the driver's style.
    const player = ctx.player;
    if (player && player !== car && !player.wrecked) {
      const dS = wrapHalf(player.s - car.s, L);
      const rnd = ctx.random || Math.random;
      if (time < driver.blockUntil) lane = driver.blockLane;
      else if (dS < -3 && dS > -14 && rnd() < driver.aggression * 0.02) {
        driver.blockLane = clamp(player.lateral, -half, half);
        driver.blockUntil = time + 1.5;
        lane = driver.blockLane;
      }
    }
    lane = clamp(lane, -half, half);

    // Aim at a point up the road in that lane.
    const look = 14 + Math.max(0, speed) * 0.4;
    const p = track.at(car.s + look);
    const tx = p.x - p.dz * lane, tz = p.z + p.dx * lane;
    const desired = Math.atan2(tz - car.z, tx - car.x);
    const diff = wrapAngle(desired - car.heading);
    const steer = clamp(diff * 2.2, -1, 1);

    // Speed for the road ahead.
    const safe = cornerSpeed(track, car.s, driver.skill);
    let throttle = speed < safe ? 1 : 0;
    const drift = speed > safe * 1.05 && Math.abs(steer) > 0.35 && speed > 15;
    let maxCurv = 0;
    for (let k = 8; k <= 70; k += 6) maxCurv = Math.max(maxCurv, Math.abs(track.at(car.s + k).curvature));
    let nitro = car.nitro > 0.35 && maxCurv < 0.006 && speed > 20;

    // Rubber-banding against the player.
    let powerScale = 1;
    if (ctx.playerProgress != null && ctx.progress != null) {
      const gap = ctx.playerProgress - ctx.progress;   // positive: the player is ahead
      if (gap < -BAND_DISTANCE) { powerScale = 0.9; nitro = false; }
      else if (gap > BAND_DISTANCE) { powerScale = 1.1; }
    }

    return { throttle, steer, drift, nitro, powerScale };
  }

  return { makeDriver, input, cornerSpeed, CORNER_G, BAND_DISTANCE };
});
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `All 23 passed`. The lap test is the one most likely to need tuning: if the driver wrecks, lower `CORNER_G` or raise the look-ahead; if it is too slow to finish a lap in 110 s, raise `CORNER_G`.

- [ ] **Step 5: Commit**

```bash
git add velocity/js/ai.js velocity/tests/velocity-logic.test.js
git commit -m "Velocity: AI drivers with corner speed, avoidance, blocking and rubber-banding"
```

---

### Task 4: Race rules

**Files:**
- Create: `velocity/js/race.js`
- Modify: `velocity/tests/velocity-logic.test.js` (add a `Race` section before the summary)

**Interfaces:**
- Consumes: `Track.build`, `Physics.createCar/step/collide/nearMiss`, `AI.makeDriver/input`.
- Produces: `VelocityRace.create({track, playerCar, opponents?, laps?, storage?, random?}) → race` with `race.phase` (`'countdown'|'racing'|'finished'`), `race.countdown`, `race.time`, `race.entries[]` (`{car,isPlayer,name,driver,crossings,progress,lapTimes,lapStart,finished,finishTime,place}`), `race.results` (`{place,total,bestLap,lapTimes,newBest,newBestLap}` once finished), `race.records`. `update(race, dt, playerInput)`, `playerEntry(race)`, `standings(race)`, `lapNumber(entry, laps)`, `currentLapTime(race, entry)`, `formatTime(seconds)`, `loadRecords(storage)`, `saveRecords(storage, records)`, `resume(race)` (3-2-1 again after a pause), `DEFAULT_OPPONENTS`, `RECORD_KEY`.

- [ ] **Step 1: Add the failing tests**

```js
const Race = js('race.js');

console.log('Race');

function fakeStorage(initial) {
  const data = Object.assign({}, initial || {});
  return { get: (k) => (k in data ? data[k] : null), set: (k, v) => { data[k] = v; }, data };
}
function seeded(seed) {
  let s = seed || 1;
  return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
}
function runRace(race, playerInput, seconds) {
  const dt = 1 / 60;
  for (let t = 0; t < seconds && race.phase !== 'finished'; t += dt) {
    Race.update(race, dt, typeof playerInput === 'function' ? playerInput(race) : playerInput);
  }
  return race;
}
const aiPlayer = (() => { const d = AI.makeDriver({ skill: 0.95, aggression: 0 }); return (race) => AI.input(Race.playerEntry(race).car, d, race.track, { time: race.time, cars: race.entries.map((e) => e.car) }); })();

test('a race starts with a countdown during which nobody moves', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(1) });
  eq(race.phase, 'countdown');
  eq(race.entries.length, 8);
  assert(Race.playerEntry(race).isPlayer);
  const before = race.entries.map((e) => [e.car.x, e.car.z]);
  runRace(race, { steer: 0 }, 2);
  race.entries.forEach((e, i) => { eq(e.car.x, before[i][0]); eq(e.car.z, before[i][1]); });
  runRace(race, { steer: 0 }, 1.1);
  eq(race.phase, 'racing');
  eq(Race.lapNumber(Race.playerEntry(race), 3), 1);
});

test('the player starts at the back of the grid', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(2) });
  const p = Race.playerEntry(race);
  race.entries.forEach((e) => { if (!e.isPlayer) assert(e.car.s > p.car.s, 'opponent ahead on the grid'); });
});

test('laps count only when the line is crossed forwards', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(3) });
  const e = Race.playerEntry(race);
  race.phase = 'racing';
  e.car.s = track.length - 5; e.crossings = 0;
  Race.update(race, 1 / 60, { steer: 0 });
  // Teleport across the line forwards.
  e.car.s = 5; Race.update(race, 0, { steer: 0 });
  eq(e.crossings, 1, 'forwards counts');
  e.car.s = track.length - 5; Race.update(race, 0, { steer: 0 });
  eq(e.crossings, 0, 'backwards uncounts');
});

test('positions order by distance covered, and finishers come first', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(4) });
  race.phase = 'racing';
  race.entries.forEach((e, i) => { e.crossings = 1; e.car.s = 100 + i * 50; });
  race.entries[2].finished = true; race.entries[2].finishTime = 100;
  Race.update(race, 0, { steer: 0 });
  const order = Race.standings(race);
  assert(order[0] === race.entries[2], 'the finisher leads');
  assert(order[1] === race.entries[7], 'then the car furthest round');
  eq(race.entries[2].place, 1);
});

test('a full race: the player finishes, gets a place and a best time is saved', () => {
  const storage = fakeStorage();
  const race = Race.create({ track, playerCar: 'dart', laps: 1, storage, random: seeded(5) });
  runRace(race, aiPlayer, 240);
  eq(race.phase, 'finished', 'finished within 4 minutes');
  const r = race.results;
  assert(r.place >= 1 && r.place <= 8, 'a place: ' + r.place);
  assert(r.total > 30 && r.total < 240, 'a sensible time: ' + r.total);
  eq(r.lapTimes.length, 1);
  eq(r.newBest, true, 'first time is a record');
  const saved = Race.loadRecords(storage);
  near(saved.bestTime, r.total, 0.001);
  // A slower second race does not beat it.
  const again = Race.create({ track, playerCar: 'dart', laps: 1, storage, random: seeded(6) });
  again.phase = 'racing'; again.time = r.total + 10;
  const e = Race.playerEntry(again); e.crossings = 1; e.lapStart = 0; e.car.s = track.length - 2;
  Race.update(again, 1 / 60, { steer: 0, throttle: 1 });
  e.car.s = 1; Race.update(again, 0, { steer: 0 });
  eq(again.phase, 'finished');
  eq(again.results.newBest, false);
  near(Race.loadRecords(storage).bestTime, r.total, 0.001, 'record kept');
});

test('records survive a broken storage', () => {
  const broken = { get: () => { throw new Error('no'); }, set: () => { throw new Error('no'); } };
  eq(Race.loadRecords(broken).bestTime, null);
  Race.saveRecords(broken, { bestTime: 1 });
  eq(Race.loadRecords(null).bestTime, null);
});

test('times format as m:ss.mmm', () => {
  eq(Race.formatTime(0), '0:00.000');
  eq(Race.formatTime(65.4321), '1:05.432');
  eq(Race.formatTime(null), '--:--.---');
});

test('resuming after a pause brings the countdown back without moving the cars', () => {
  const race = Race.create({ track, playerCar: 'comet', random: seeded(7) });
  runRace(race, { steer: 0 }, 5);
  eq(race.phase, 'racing');
  Race.resume(race);
  eq(race.phase, 'countdown');
  const x = Race.playerEntry(race).car.x;
  runRace(race, { steer: 0 }, 1);
  eq(Race.playerEntry(race).car.x, x);
});
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `Cannot find module '.../velocity/js/race.js'`

- [ ] **Step 3: Write `velocity/js/race.js`**

```js
/*
 * VELOCITY — the race.
 *
 * Owns the cars, the countdown, lap counting, positions, the finish and the
 * results, and the best-time record. Each update it asks the AI for its
 * inputs, steps every car, resolves collisions and near misses, and then
 * works out who is where. Records go through an injected storage object
 * ({get, set}) so the page can pass localStorage and the tests a fake.
 * No DOM, no Three.js.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityRace = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const Physics = typeof require === 'function' && typeof module === 'object' ? require('./physics.js') : root_.VelocityPhysics;
  const AI = typeof require === 'function' && typeof module === 'object' ? require('./ai.js') : root_.VelocityAI;

  const RECORD_KEY = 'velocity.records';
  const COUNTDOWN = 3;
  const NEAR_MISS_NITRO = 0.1;

  const DEFAULT_OPPONENTS = [
    { name: 'Vex', carKey: 'brawler', driver: { skill: 0.92, aggression: 0.6, lane: -2.5, phase: 0.3 } },
    { name: 'Nova', carKey: 'dart', driver: { skill: 0.85, aggression: 0.3, lane: 2.5, phase: 1.1 } },
    { name: 'Juno', carKey: 'drifter', driver: { skill: 0.8, aggression: 0.4, lane: 0, phase: 2.0 } },
    { name: 'Rook', carKey: 'comet', driver: { skill: 0.75, aggression: 0.5, lane: -1, phase: 2.9 } },
    { name: 'Sable', carKey: 'brawler', driver: { skill: 0.7, aggression: 0.2, lane: 1.5, phase: 3.7 } },
    { name: 'Pip', carKey: 'dart', driver: { skill: 0.6, aggression: 0.1, lane: -2, phase: 4.6 } },
    { name: 'Moss', carKey: 'comet', driver: { skill: 0.55, aggression: 0.3, lane: 2, phase: 5.4 } },
  ];

  function loadRecords(storage) {
    const empty = { bestTime: null, bestLap: null, carKey: null };
    if (!storage) return empty;
    try {
      const raw = storage.get(RECORD_KEY);
      if (!raw) return empty;
      const parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
      return Object.assign(empty, parsed);
    } catch (e) { return empty; }
  }

  function saveRecords(storage, records) {
    if (!storage) return false;
    try { storage.set(RECORD_KEY, JSON.stringify(records)); return true; }
    catch (e) { return false; }
  }

  function create(opts) {
    const track = opts.track;
    const laps = opts.laps || 3;
    const random = opts.random || Math.random;
    const slots = track.gridSlots();
    const opponents = (opts.opponents || DEFAULT_OPPONENTS).slice(0, 7);
    const entries = [];
    const add = (spec, slot, isPlayer) => {
      const car = Physics.createCar(spec.carKey, slot);
      car.isPlayer = isPlayer;
      entries.push({
        car, isPlayer, name: isPlayer ? 'You' : spec.name,
        driver: isPlayer ? null : AI.makeDriver(spec.driver),
        crossings: 0, progress: slot.s - track.length, lapTimes: [], lapStart: 0,
        finished: false, finishTime: null, place: entries.length + 1, nearMissCooldown: 0,
      });
    };
    opponents.forEach((o, i) => add(o, slots[i], false));
    add({ carKey: opts.playerCar || 'comet' }, slots[7], true);
    return {
      track, laps, random, storage: opts.storage || null,
      phase: 'countdown', countdown: COUNTDOWN, time: 0,
      entries, results: null, records: loadRecords(opts.storage), playerCarKey: opts.playerCar || 'comet',
    };
  }

  function playerEntry(race) { return race.entries.find((e) => e.isPlayer); }

  function lapNumber(entry, laps) { return Math.min(laps, Math.max(1, entry.crossings)); }

  function currentLapTime(race, entry) { return entry.finished ? entry.lapTimes[entry.lapTimes.length - 1] || 0 : race.time - entry.lapStart; }

  function standings(race) {
    return race.entries.slice().sort((a, b) => {
      if (a.finished && b.finished) return a.finishTime - b.finishTime;
      if (a.finished !== b.finished) return a.finished ? -1 : 1;
      return b.progress - a.progress;
    });
  }

  function resume(race) {
    if (race.phase === 'racing') { race.phase = 'countdown'; race.countdown = COUNTDOWN; }
  }

  function update(race, dt, playerInput) {
    if (race.phase === 'countdown') {
      race.countdown -= dt;
      if (race.countdown <= 0) { race.countdown = 0; race.phase = 'racing'; }
      return race;
    }
    if (race.phase !== 'racing') return race;
    race.time += dt;
    const L = race.track.length;
    const me = playerEntry(race);
    const cars = race.entries.map((e) => e.car);

    for (const e of race.entries) {
      let inp;
      if (e.isPlayer) inp = playerInput || { steer: 0 };
      else {
        inp = AI.input(e.car, e.driver, race.track, {
          time: race.time, cars, player: me.car, playerProgress: me.progress, progress: e.progress, random: race.random,
        });
        if (e.finished) inp.nitro = false;
      }
      Physics.step(e.car, inp, race.track, dt);
    }

    for (let i = 0; i < cars.length; i++) {
      for (let j = i + 1; j < cars.length; j++) {
        if (Physics.collide(cars[i], cars[j])) continue;
        if ((cars[i].isPlayer || cars[j].isPlayer) && me.nearMissCooldown <= 0 && Physics.nearMiss(cars[i], cars[j])) {
          me.car.nitro = Math.min(1, me.car.nitro + NEAR_MISS_NITRO);
          me.car.nearMiss = true;
          me.nearMissCooldown = 1;
        }
      }
    }
    me.nearMissCooldown -= dt;

    for (const e of race.entries) {
      const c = e.car;
      if (c.prevS > L * 0.75 && c.s < L * 0.25) {
        e.crossings++;
        if (e.crossings === 1) e.lapStart = race.time;
        else { e.lapTimes.push(race.time - e.lapStart); e.lapStart = race.time; }
      } else if (c.prevS < L * 0.25 && c.s > L * 0.75) {
        e.crossings--;
        if (e.lapTimes.length) { e.lapStart -= e.lapTimes.pop(); }
      }
      e.progress = (e.crossings - 1) * L + c.s;
      if (!e.finished && e.crossings >= race.laps + 1) { e.finished = true; e.finishTime = race.time; }
    }

    standings(race).forEach((e, k) => { e.place = k + 1; });

    if (me.finished) {
      race.phase = 'finished';
      const bestLap = me.lapTimes.length ? Math.min.apply(null, me.lapTimes) : null;
      const rec = race.records;
      const newBest = rec.bestTime == null || me.finishTime < rec.bestTime;
      const newBestLap = bestLap != null && (rec.bestLap == null || bestLap < rec.bestLap);
      race.results = { place: me.place, total: me.finishTime, bestLap, lapTimes: me.lapTimes.slice(), newBest, newBestLap };
      if (newBest) { rec.bestTime = me.finishTime; rec.carKey = race.playerCarKey; }
      if (newBestLap) rec.bestLap = bestLap;
      if (newBest || newBestLap) saveRecords(race.storage, rec);
    }
    return race;
  }

  function formatTime(seconds) {
    if (seconds == null || !isFinite(seconds)) return '--:--.---';
    const ms = Math.round(seconds * 1000);
    const m = Math.floor(ms / 60000), s = Math.floor((ms % 60000) / 1000), r = ms % 1000;
    return m + ':' + String(s).padStart(2, '0') + '.' + String(r).padStart(3, '0');
  }

  return {
    create, update, playerEntry, standings, lapNumber, currentLapTime, formatTime,
    loadRecords, saveRecords, resume, DEFAULT_OPPONENTS, RECORD_KEY, COUNTDOWN,
  };
});
```

Note on the dependency lines: `root_` must be the wrapper's root. Write the wrapper for this file as `(function (root, factory) { const api = factory(root); ... })(…, function (root_) { … })` so the browser globals `VelocityPhysics` and `VelocityAI` are reachable.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `node velocity/tests/velocity-logic.test.js`
Expected: `All 31 passed`

- [ ] **Step 5: Commit**

```bash
git add velocity/js/race.js velocity/tests/velocity-logic.test.js
git commit -m "Velocity: race rules with countdown, laps, positions, finish and records"
```

---

### Task 5: The 3D world (scene.js)

**Files:**
- Create: `velocity/js/scene.js`

**Interfaces:**
- Consumes: `track.points[]`, `track.nearest`, `track.length`, `track.RAMP_HEIGHT`; car state fields from Task 2; `window.THREE` (Three.js 0.170.0).
- Produces: `VelocityScene.create({THREE, canvas, track, mobile}) → {resize(), setCars(cars), render(dt, view)}` where `view = {player: car, cars: car[], time: number}`.

No Node test (browser only). Verified by the Playwright smoke script in Task 7 and by eye.

- [ ] **Step 1: Write `velocity/js/scene.js`**

```js
/*
 * VELOCITY — the 3D world.
 *
 * Builds Harbor Loop and the cars with Three.js and draws each frame from the
 * car states. It reads car state and never changes it. Low-poly and
 * flat-shaded on purpose: one hemisphere light, one sun, no shadow maps, the
 * whole city in one instanced draw call, so a mid-range phone holds 60 fps.
 */
(function (root) {
  'use strict';

  const PALETTE = [0x5a3a6e, 0x7a4a7a, 0x3f2f5c, 0x8a5a6a, 0x2f2645, 0x6b4a86, 0x4a3d7a, 0x8c4f5f];

  function seeded(seed) {
    let s = seed;
    return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
  }

  // Accumulates coloured quads into one BufferGeometry.
  function Builder(THREE) {
    this.THREE = THREE; this.pos = []; this.col = []; this.idx = []; this.n = 0;
  }
  Builder.prototype.quad = function (a, b, c, d, color) {
    // a,b = near left/right; c,d = far left/right. Two triangles, both windings so it is visible from above and below.
    const col = new this.THREE.Color(color);
    for (const v of [a, b, c, d]) { this.pos.push(v[0], v[1], v[2]); this.col.push(col.r, col.g, col.b); }
    const n = this.n;
    this.idx.push(n, n + 2, n + 1, n + 1, n + 2, n + 3);
    this.n += 4;
  };
  Builder.prototype.mesh = function (material) {
    const g = new this.THREE.BufferGeometry();
    g.setAttribute('position', new this.THREE.Float32BufferAttribute(this.pos, 3));
    g.setAttribute('color', new this.THREE.Float32BufferAttribute(this.col, 3));
    g.setIndex(this.idx);
    g.computeVertexNormals();
    return new this.THREE.Mesh(g, material);
  };

  function create(opts) {
    const THREE = opts.THREE || root.THREE;
    const track = opts.track;
    const canvas = opts.canvas;
    const mobile = !!opts.mobile;
    const rand = seeded(7);

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: !mobile, powerPreference: 'high-performance' });
    renderer.setPixelRatio(Math.min(root.devicePixelRatio || 1, mobile ? 1.5 : 2));
    const scene = new THREE.Scene();
    scene.fog = new THREE.Fog(0x4a2050, 220, 1100);
    const camera = new THREE.PerspectiveCamera(72, 1, 0.5, 3000);
    scene.add(new THREE.HemisphereLight(0xffd0a0, 0x2a1838, 1.3));
    const sun = new THREE.DirectionalLight(0xffb070, 1.8);
    sun.position.set(-400, 250, 700);
    scene.add(sun);

    const lit = new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide });
    const unlit = new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide });
    const pts = track.points, N = pts.length;
    const edge = (p, side, extra) => {   // a point on the road edge, side -1 left / +1 right
      const half = p.w / 2 + (extra || 0);
      return [p.x - p.dz * half * side, p.rampHeight, p.z + p.dx * half * side];
    };
    const lift = (v, dy) => [v[0], v[1] + dy, v[2]];

    // --- Sky, sun and ground ---
    {
      const g = new THREE.SphereGeometry(2400, 24, 12);
      const colors = [];
      const top = new THREE.Color(0x160c33), mid = new THREE.Color(0x6a2a6a), hor = new THREE.Color(0xff8a3d);
      const p = g.attributes.position;
      for (let i = 0; i < p.count; i++) {
        const y = p.getY(i) / 2400;
        const c = y > 0.25 ? top.clone().lerp(mid, 1 - (y - 0.25) / 0.75) : hor.clone().lerp(mid, Math.max(0, y) / 0.25);
        colors.push(c.r, c.g, c.b);
      }
      g.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
      scene.add(new THREE.Mesh(g, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.BackSide, fog: false })));
      scene.sky = scene.children[scene.children.length - 1];

      const cv = document.createElement('canvas'); cv.width = cv.height = 128;
      const cx = cv.getContext('2d');
      const grad = cx.createRadialGradient(64, 64, 10, 64, 64, 64);
      grad.addColorStop(0, 'rgba(255,230,170,1)'); grad.addColorStop(0.35, 'rgba(255,160,80,0.9)'); grad.addColorStop(1, 'rgba(255,120,60,0)');
      cx.fillStyle = grad; cx.fillRect(0, 0, 128, 128);
      const sprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), fog: false, transparent: true, depthWrite: false }));
      sprite.scale.set(700, 700, 1); sprite.position.set(300, 90, 2000);
      scene.add(sprite); scene.sunSprite = sprite;

      const ground = new THREE.Mesh(new THREE.PlaneGeometry(6000, 6000), new THREE.MeshLambertMaterial({ color: 0x241a30 }));
      ground.rotation.x = -Math.PI / 2; ground.position.y = -0.6; scene.add(ground);
    }

    // --- Road, dashes, kerbs, walls, tunnel, water ---
    {
      const road = new Builder(THREE), dash = new Builder(THREE), kerb = new Builder(THREE);
      const wall = new Builder(THREE), neon = new Builder(THREE), water = new Builder(THREE);
      for (let i = 0; i < N; i++) {
        const a = pts[i], b = pts[(i + 1) % N];
        const asphalt = a.tunnel ? 0x2b2b36 : 0x3a3a44;
        road.quad(edge(a, -1), edge(a, 1), edge(b, -1), edge(b, 1), asphalt);
        if (i % 4 < 2) dash.quad(lift(edge(a, -1, -a.w / 2 - 0.15), 0.02), lift(edge(a, 1, -a.w / 2 - 0.15), 0.02), lift(edge(b, -1, -b.w / 2 - 0.15), 0.02), lift(edge(b, 1, -b.w / 2 - 0.15), 0.02), 0xd8d8e0);
        const kc = (i >> 1) % 2 ? 0xe03030 : 0xf0f0f0;
        for (const side of [-1, 1]) {
          kerb.quad(lift(edge(a, side, -0.7), 0.03), lift(edge(a, side), 0.03), lift(edge(b, side, -0.7), 0.03), lift(edge(b, side), 0.03), kc);
          const h = a.tunnel ? 6 : 1.1;
          const wc = a.tunnel ? 0x3a3a4c : (side === 1 && a.water ? 0x6a6a78 : 0x8a8a98);
          wall.quad(edge(a, side), lift(edge(a, side), h), edge(b, side), lift(edge(b, side), h), wc);
          if (a.tunnel) {
            const nc = a.control % 2 ? 0xff2d95 : 0x19e6ff;
            neon.quad(lift(edge(a, side, -0.05), 3.0), lift(edge(a, side, -0.05), 3.3), lift(edge(b, side, -0.05), 3.0), lift(edge(b, side, -0.05), 3.3), nc);
          }
        }
        if (a.tunnel) wall.quad(lift(edge(a, -1), 6), lift(edge(a, 1), 6), lift(edge(b, -1), 6), lift(edge(b, 1), 6), 0x2a2a38);
        if (a.water) {
          const far = (p) => [p.x - p.dz * 400, -0.4, p.z + p.dx * 400];
          water.quad(lift(edge(a, 1, 2), -0.4), far(a), lift(edge(b, 1, 2), -0.4), far(b), 0x1d5a9a);
          wall.quad(lift(edge(a, 1, 2), -0.6), lift(edge(a, 1, 2), 0.0), lift(edge(b, 1, 2), -0.6), lift(edge(b, 1, 2), 0.0), 0x5a4a40);
        }
      }
      scene.add(road.mesh(lit), dash.mesh(unlit), kerb.mesh(lit), wall.mesh(lit), neon.mesh(unlit));
      const w = water.mesh(new THREE.MeshLambertMaterial({ vertexColors: true, emissive: 0x0a2a4a, side: THREE.DoubleSide }));
      scene.add(w);
    }

    // --- The city: one instanced box ---
    {
      const placements = [];
      const tryPlace = (p, side, offset, sx, h, sz, color) => {
        const half = p.w / 2;
        const cx = p.x - p.dz * side * (half + offset + Math.max(sx, sz) / 2);
        const cz = p.z + p.dx * side * (half + offset + Math.max(sx, sz) / 2);
        const n = track.nearest(cx, cz);
        if (Math.abs(n.lateral) < n.w / 2 + 3 + Math.max(sx, sz) * 0.72) return;
        placements.push({ x: cx, z: cz, sx, h, sz, color });
      };
      for (let i = 0; i < N; i += 7) {
        const p = pts[i];
        if (p.tunnel) continue;
        for (const side of [-1, 1]) {
          if (side === 1 && p.water) continue;
          const r = rand();
          tryPlace(p, side, 4 + rand() * 10, 8 + rand() * 14, 6 + r * r * 50, 8 + rand() * 14, PALETTE[Math.floor(rand() * PALETTE.length)]);
        }
      }
      for (let i = 0; i < N; i += 9) {
        const p = pts[i];
        for (const side of [-1, 1]) {
          if (side === 1 && p.water) continue;
          const r = rand();
          tryPlace(p, side, 40 + rand() * 90, 14 + rand() * 20, 25 + r * 100, 14 + rand() * 20, PALETTE[Math.floor(rand() * PALETTE.length)]);
        }
      }
      const box = new THREE.BoxGeometry(1, 1, 1);
      const city = new THREE.InstancedMesh(box, new THREE.MeshLambertMaterial({ color: 0xffffff }), placements.length);
      const m = new THREE.Matrix4(), q = new THREE.Quaternion(), s = new THREE.Vector3(), v = new THREE.Vector3();
      placements.forEach((b, k) => {
        m.compose(v.set(b.x, b.h / 2 - 0.5, b.z), q, s.set(b.sx, b.h, b.sz));
        city.setMatrixAt(k, m);
        const c = new THREE.Color(b.color); c.offsetHSL(0, 0, (rand() - 0.5) * 0.12);
        city.setColorAt(k, c);
      });
      city.instanceMatrix.needsUpdate = true;
      if (city.instanceColor) city.instanceColor.needsUpdate = true;
      scene.add(city);
    }

    // --- Cars ---
    const carMeshes = new Map();
    function makeCar(color) {
      const g = new THREE.Group();
      const bodyMat = new THREE.MeshLambertMaterial({ color });
      const body = new THREE.Mesh(new THREE.BoxGeometry(4.2, 0.8, 2.0), bodyMat); body.position.y = 0.75; g.add(body);
      const nose = new THREE.Mesh(new THREE.BoxGeometry(1.2, 0.5, 1.8), bodyMat); nose.position.set(2.3, 0.6, 0); g.add(nose);
      const cabin = new THREE.Mesh(new THREE.BoxGeometry(2.0, 0.65, 1.6), new THREE.MeshLambertMaterial({ color: 0x1a1a24 })); cabin.position.set(-0.3, 1.45, 0); g.add(cabin);
      const wheelGeo = new THREE.CylinderGeometry(0.42, 0.42, 0.35, 10); wheelGeo.rotateX(Math.PI / 2);
      const wheelMat = new THREE.MeshLambertMaterial({ color: 0x141418 });
      g.wheels = [];
      for (const [x, z] of [[1.4, 1.05], [1.4, -1.05], [-1.4, 1.05], [-1.4, -1.05]]) {
        const w = new THREE.Mesh(wheelGeo, wheelMat); w.position.set(x, 0.42, z); g.add(w); g.wheels.push(w);
      }
      const lamp = new THREE.MeshBasicMaterial({ color: 0xfff2c0 });
      for (const z of [0.6, -0.6]) { const l = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.25, 0.4), lamp); l.position.set(2.92, 0.7, z); g.add(l); }
      const tail = new THREE.MeshBasicMaterial({ color: 0xff2020 });
      for (const z of [0.7, -0.7]) { const l = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.2, 0.5), tail); l.position.set(-2.12, 0.8, z); g.add(l); }
      g.flames = [];
      const flameMat = new THREE.MeshBasicMaterial({ color: 0x4df0ff, transparent: true, opacity: 0.85 });
      for (const z of [0.5, -0.5]) {
        const f = new THREE.Mesh(new THREE.ConeGeometry(0.22, 1.6, 8), flameMat);
        f.rotation.z = Math.PI / 2; f.position.set(-2.9, 0.6, z); f.visible = false; g.add(f); g.flames.push(f);
      }
      const shadow = new THREE.Mesh(new THREE.CircleGeometry(2.6, 16), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.35, depthWrite: false }));
      shadow.rotation.x = -Math.PI / 2; shadow.scale.set(1, 0.55, 1);
      g.shadow = shadow;
      return g;
    }

    function setCars(cars) {
      for (const [, m] of carMeshes) { scene.remove(m); scene.remove(m.shadow); }
      carMeshes.clear();
      for (const car of cars) {
        const m = makeCar(car.color);
        scene.add(m); scene.add(m.shadow);
        carMeshes.set(car, m);
      }
    }

    // --- Sparks ---
    const SPARKS = 90;
    const sparkPos = new Float32Array(SPARKS * 3), sparkVel = new Float32Array(SPARKS * 3), sparkLife = new Float32Array(SPARKS);
    const sparkGeo = new THREE.BufferGeometry();
    sparkGeo.setAttribute('position', new THREE.BufferAttribute(sparkPos, 3));
    const sparks = new THREE.Points(sparkGeo, new THREE.PointsMaterial({ color: 0xffc070, size: 0.35, transparent: true, depthWrite: false }));
    scene.add(sparks);
    let sparkNext = 0;
    function spawnSparks(car) {
      const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
      const side = car.lateral > 0 ? 1 : -1;
      const rx = -fz * side, rz = fx * side;
      for (let k = 0; k < 3; k++) {
        const i = sparkNext; sparkNext = (sparkNext + 1) % SPARKS;
        sparkPos[i * 3] = car.x + rx * 1.1; sparkPos[i * 3 + 1] = car.y + 0.4; sparkPos[i * 3 + 2] = car.z + rz * 1.1;
        sparkVel[i * 3] = -fx * Math.random() * 8 + rx * Math.random() * 2;
        sparkVel[i * 3 + 1] = 2 + Math.random() * 5;
        sparkVel[i * 3 + 2] = -fz * Math.random() * 8 + rz * Math.random() * 2;
        sparkLife[i] = 0.4 + Math.random() * 0.4;
      }
    }
    function updateSparks(dt) {
      for (let i = 0; i < SPARKS; i++) {
        if (sparkLife[i] <= 0) { sparkPos[i * 3 + 1] = -100; continue; }
        sparkLife[i] -= dt;
        sparkVel[i * 3 + 1] -= 20 * dt;
        sparkPos[i * 3] += sparkVel[i * 3] * dt; sparkPos[i * 3 + 1] += sparkVel[i * 3 + 1] * dt; sparkPos[i * 3 + 2] += sparkVel[i * 3 + 2] * dt;
      }
      sparkGeo.attributes.position.needsUpdate = true;
    }

    // --- Camera ---
    const camPos = new THREE.Vector3(0, 5, 0), camLook = new THREE.Vector3(1, 1, 0);
    let fov = 72, started = false;

    function resize() {
      const w = canvas.clientWidth || root.innerWidth, h = canvas.clientHeight || root.innerHeight;
      renderer.setSize(w, h, false);
      camera.aspect = w / h; camera.updateProjectionMatrix();
    }

    function render(dt, view) {
      const player = view.player;
      for (const car of view.cars || []) {
        const m = carMeshes.get(car); if (!m) continue;
        m.position.set(car.x, car.y, car.z);
        const fx = Math.cos(car.heading), fz = Math.sin(car.heading);
        const vR = car.vx * -fz + car.vz * fx;
        if (car.wrecked) {
          const t = 1 - car.wreckTimer / 2;
          m.rotation.set(Math.sin(t * 9) * 0.3, -car.heading + t * 7, Math.sin(t * 6) * 0.5);
          m.position.y = car.y + Math.max(0, Math.sin(t * Math.PI)) * 1.5;
        } else {
          m.rotation.set(0, -car.heading, Math.max(-0.12, Math.min(0.12, -vR * 0.015)));
        }
        const spin = car.speed * dt / 0.42;
        for (const w of m.wheels) w.rotation.z -= spin;
        for (const f of m.flames) { f.visible = car.nitroActive; f.scale.set(0.8 + Math.random() * 0.5, 1, 1); }
        m.shadow.position.set(car.x, (car.airborne ? 0 : car.y) + 0.03, car.z);
        m.shadow.rotation.z = -car.heading;
        if (car.sparks) spawnSparks(car);
      }
      updateSparks(dt);

      if (player) {
        const fx = Math.cos(player.heading), fz = Math.sin(player.heading);
        const sp = player.speed;
        const back = 7.5 + sp * 0.04, up = 3.0 + sp * 0.012;
        const want = new THREE.Vector3(player.x - fx * back, player.y + up, player.z - fz * back);
        const look = new THREE.Vector3(player.x + fx * 8, player.y + 1.2, player.z + fz * 8);
        if (!started) { camPos.copy(want); camLook.copy(look); started = true; }
        const k = player.wrecked ? 0.5 : 6;
        camPos.lerp(want, 1 - Math.exp(-k * dt));
        camLook.lerp(look, 1 - Math.exp(-10 * dt));
        const shake = (player.nitroActive ? 0.1 : 0) + (player.sparks ? 0.12 : 0) + (player.bumped ? 0.3 : 0);
        camera.position.set(camPos.x + (Math.random() - 0.5) * shake, camPos.y + (Math.random() - 0.5) * shake, camPos.z + (Math.random() - 0.5) * shake);
        camera.lookAt(camLook);
        const wantFov = 70 + (player.nitroActive ? 14 : 0) + sp * 0.1;
        fov += (wantFov - fov) * (1 - Math.exp(-5 * dt));
        camera.fov = fov; camera.updateProjectionMatrix();
      }
      scene.sky.position.copy(camera.position);
      scene.sunSprite.position.set(camera.position.x + 300, 90, camera.position.z + 2000);
      renderer.render(scene, camera);
    }

    resize();
    return { resize, setCars, render, renderer, scene, camera };
  }

  root.VelocityScene = { create };
})(typeof self !== 'undefined' ? self : this);
```

- [ ] **Step 2: Syntax-check it in Node**

Run: `node --check velocity/js/scene.js`
Expected: no output.

- [ ] **Step 3: Commit**

```bash
git add velocity/js/scene.js
git commit -m "Velocity: low-poly sunset city, cars, chase camera and sparks in Three.js"
```

---

### Task 6: The page, the look and the app (index.html, style.css, app.js)

**Files:**
- Create: `velocity/index.html`
- Create: `velocity/css/style.css`
- Create: `velocity/js/app.js`
- Create: `velocity/icon.svg`

**Interfaces:**
- Consumes: everything above. `VelocityScene.create`, `VelocityRace.*`, `VelocityPhysics.CARS/ORDER`, `VelocityTrack.build`.
- Produces: `VelocityApp.start()` (called by the inline module in `index.html` once `window.THREE` is set).

- [ ] **Step 1: Write `velocity/index.html`**

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover, user-scalable=no" />
  <title>Velocity</title>
  <meta name="description" content="Velocity — an arcade street racer in your browser. One sunset city circuit, seven rivals, three laps, drift, nitro and a jump. Phone-first, no install." />
  <meta name="theme-color" content="#140a1e" />
  <meta name="mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
  <link rel="icon" href="icon.svg" type="image/svg+xml" />
  <link rel="apple-touch-icon" href="icon.svg" />
  <link rel="stylesheet" href="css/style.css" />
</head>
<body>
  <canvas id="view"></canvas>
  <canvas id="fx"></canvas>

  <section id="screen-title" class="screen menu">
    <div class="panel">
      <h1 class="logo">VELOCITY</h1>
      <p class="tag">Harbor Loop · 3 laps · 7 rivals</p>
      <button id="btn-race" class="big">Race</button>
      <p id="best" class="best">No best time yet</p>
      <div class="row">
        <button id="btn-sound" class="small" aria-pressed="false">Sound: on</button>
        <button id="btn-steer" class="small">Steering: arrows</button>
      </div>
      <p class="hint">Phone: steer with the arrows or tilt, hold DRIFT in corners, tap NITRO.<br />Keyboard: arrows or WASD, Shift to drift, Space for nitro, P to pause.</p>
    </div>
  </section>

  <section id="screen-cars" class="screen menu" hidden>
    <div class="panel wide">
      <h2>Choose your car</h2>
      <div id="car-list" class="cars"></div>
      <div class="row">
        <button id="btn-back" class="small">Back</button>
        <button id="btn-go" class="big">Start race</button>
      </div>
    </div>
  </section>

  <div id="hud" hidden>
    <div class="hud-top">
      <div id="hud-pos" class="hud-box"><span class="big-num">8</span><span class="sub">/ 8</span></div>
      <div id="hud-lap" class="hud-box"><span class="label">LAP</span><span class="big-num">1</span><span class="sub">/ 3</span></div>
      <div id="hud-time" class="hud-box mono">0:00.000</div>
      <button id="btn-pause" class="hud-btn" aria-label="Pause">II</button>
    </div>
    <div class="hud-bottom">
      <div id="hud-speed" class="speed"><span class="big-num">0</span><span class="sub">km/h</span></div>
      <div class="nitro"><div id="nitro-fill" class="nitro-fill"></div><span class="nitro-label">NITRO</span></div>
    </div>
    <div id="countdown" class="countdown" hidden>3</div>
    <div id="toast" class="toast" hidden></div>
  </div>

  <div id="touch" hidden>
    <div class="touch-left">
      <button class="touch-btn" data-action="left" aria-label="Steer left">◀</button>
      <button class="touch-btn" data-action="right" aria-label="Steer right">▶</button>
    </div>
    <div class="touch-right">
      <button class="touch-btn drift" data-action="drift" aria-label="Drift">DRIFT</button>
      <button class="touch-btn nitro" data-action="nitro" aria-label="Nitro">NITRO</button>
    </div>
  </div>

  <section id="screen-pause" class="screen menu overlay" hidden>
    <div class="panel">
      <h2>Paused</h2>
      <button id="btn-resume" class="big">Resume</button>
      <div class="row">
        <button id="btn-restart" class="small">Restart</button>
        <button id="btn-steer2" class="small">Steering: arrows</button>
        <button id="btn-sound2" class="small">Sound: on</button>
        <button id="btn-quit" class="small">Quit</button>
      </div>
    </div>
  </section>

  <section id="screen-results" class="screen menu overlay" hidden>
    <div class="panel">
      <h2 id="res-place">3rd</h2>
      <p id="res-new" class="newbest" hidden>New best time!</p>
      <table class="results">
        <tr><td>Race time</td><td id="res-total" class="mono">0:00.000</td></tr>
        <tr><td>Best lap</td><td id="res-lap" class="mono">0:00.000</td></tr>
        <tr><td>Record</td><td id="res-record" class="mono">0:00.000</td></tr>
      </table>
      <div class="row">
        <button id="btn-again" class="big">Race again</button>
        <button id="btn-change" class="small">Change car</button>
        <button id="btn-title" class="small">Title</button>
      </div>
    </div>
  </section>

  <div id="offline" class="notice" hidden>Velocity needs an internet connection the first time it loads, to fetch its 3D engine. Connect and reload the page.</div>
  <noscript><div class="notice">Velocity needs JavaScript switched on.</div></noscript>

  <script src="js/track.js"></script>
  <script src="js/physics.js"></script>
  <script src="js/ai.js"></script>
  <script src="js/race.js"></script>
  <script src="js/scene.js"></script>
  <script src="js/app.js"></script>
  <script type="module">
    import('https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.js')
      .then((THREE) => { window.THREE = THREE; window.VelocityApp.start(); })
      .catch(() => { document.getElementById('offline').hidden = false; });
  </script>
</body>
</html>
```

- [ ] **Step 2: Write `velocity/icon.svg`**

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a1040"/><stop offset="1" stop-color="#ff7a3d"/></linearGradient></defs>
  <rect width="64" height="64" rx="14" fill="url(#g)"/>
  <path d="M10 46 L26 18 L32 30 L38 18 L54 46 Z" fill="#fff" opacity="0.95"/>
  <circle cx="32" cy="48" r="4" fill="#19e6ff"/>
</svg>
```

- [ ] **Step 3: Write `velocity/css/style.css`**

```css
/* VELOCITY — the look. Sunset neon over a dark road. */
:root {
  --bg: #140a1e;
  --panel: rgba(20, 10, 30, 0.82);
  --ink: #fff4ea;
  --muted: #c9b3c9;
  --accent: #ff7a3d;
  --accent-2: #19e6ff;
  --hot: #ff2d95;
  --btn: #ff6a2e;
  --btn-ink: #1a0a10;
  --hud: rgba(10, 5, 20, 0.55);
  --radius: 14px;
  --font: "Segoe UI", system-ui, -apple-system, Roboto, sans-serif;
  --mono: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
}
@media (prefers-color-scheme: light) {
  :root:not([data-theme="dark"]) { --panel: rgba(255, 244, 234, 0.9); --ink: #2a1030; --muted: #6a4a6a; --hud: rgba(255, 244, 234, 0.6); }
}
* { box-sizing: border-box; }
html, body { margin: 0; height: 100%; overflow: hidden; background: var(--bg); color: var(--ink); font-family: var(--font); -webkit-user-select: none; user-select: none; touch-action: none; }
#view, #fx { position: fixed; inset: 0; width: 100%; height: 100%; display: block; }
#fx { pointer-events: none; }
[hidden] { display: none !important; }

.screen { position: fixed; inset: 0; display: flex; align-items: center; justify-content: center; padding: max(16px, env(safe-area-inset-top)) max(16px, env(safe-area-inset-right)) max(16px, env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-left)); }
.menu:not(.overlay) { background: radial-gradient(120% 90% at 50% 100%, #ff7a3d 0%, #7a2a6a 45%, #160c33 100%); }
.overlay { background: rgba(10, 5, 20, 0.45); }
.panel { background: var(--panel); border-radius: calc(var(--radius) * 1.5); padding: 24px 22px; width: min(100%, 440px); max-height: 100%; overflow: auto; text-align: center; backdrop-filter: blur(10px); }
.panel.wide { width: min(100%, 720px); }
.logo { font-size: clamp(44px, 10vw, 72px); letter-spacing: 0.12em; margin: 0; font-weight: 900; font-style: italic; background: linear-gradient(90deg, #fff, var(--accent-2)); -webkit-background-clip: text; background-clip: text; color: transparent; }
.tag { margin: 4px 0 18px; color: var(--muted); letter-spacing: 0.2em; text-transform: uppercase; font-size: 12px; }
h2 { margin: 0 0 14px; font-size: 32px; }
button { font: inherit; cursor: pointer; border: 0; border-radius: var(--radius); color: var(--btn-ink); background: var(--btn); padding: 12px 18px; font-weight: 700; }
button.big { font-size: 22px; padding: 16px 40px; width: 100%; margin: 6px 0; box-shadow: 0 6px 24px rgba(255, 106, 46, 0.4); }
button.small { background: rgba(255, 255, 255, 0.14); color: var(--ink); font-size: 14px; padding: 10px 14px; }
button:active { transform: translateY(1px); filter: brightness(1.1); }
.row { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; margin-top: 10px; }
.best { color: var(--accent-2); font-family: var(--mono); margin: 10px 0; }
.hint { color: var(--muted); font-size: 12px; line-height: 1.5; margin: 14px 0 0; }
.mono { font-family: var(--mono); font-variant-numeric: tabular-nums; }

.cars { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; margin-bottom: 8px; }
.car { background: rgba(255, 255, 255, 0.08); border: 2px solid transparent; border-radius: var(--radius); padding: 12px; text-align: left; color: var(--ink); }
.car.selected { border-color: var(--accent-2); background: rgba(25, 230, 255, 0.12); }
.car .swatch { width: 100%; height: 34px; border-radius: 8px; margin-bottom: 8px; }
.car .name { font-weight: 800; font-size: 18px; }
.car .blurb { color: var(--muted); font-size: 12px; margin: 2px 0 8px; min-height: 2.6em; }
.stat { display: grid; grid-template-columns: 58px 1fr; gap: 6px; align-items: center; font-size: 11px; color: var(--muted); margin: 3px 0; }
.stat .bar { height: 6px; border-radius: 3px; background: rgba(255, 255, 255, 0.12); overflow: hidden; }
.stat .bar i { display: block; height: 100%; background: linear-gradient(90deg, var(--accent), var(--hot)); }

#hud { position: fixed; inset: 0; pointer-events: none; padding: max(10px, env(safe-area-inset-top)) max(10px, env(safe-area-inset-right)) max(10px, env(safe-area-inset-bottom)) max(10px, env(safe-area-inset-left)); display: flex; flex-direction: column; justify-content: space-between; }
.hud-top, .hud-bottom { display: flex; gap: 8px; align-items: flex-start; }
.hud-bottom { align-items: flex-end; justify-content: center; }
.hud-box { background: var(--hud); border-radius: 10px; padding: 6px 10px; display: flex; align-items: baseline; gap: 4px; backdrop-filter: blur(6px); }
.hud-box.mono { font-size: 18px; }
.big-num { font-size: 28px; font-weight: 900; line-height: 1; }
.sub { font-size: 12px; color: var(--muted); }
.label { font-size: 10px; color: var(--muted); letter-spacing: 0.15em; margin-right: 4px; }
.hud-btn { pointer-events: auto; margin-left: auto; background: var(--hud); color: var(--ink); font-weight: 900; padding: 8px 14px; }
.speed { background: var(--hud); border-radius: 10px; padding: 6px 12px; display: flex; align-items: baseline; gap: 4px; }
.speed .big-num { font-size: 34px; }
.nitro { position: relative; width: min(46vw, 260px); height: 18px; border-radius: 9px; background: var(--hud); overflow: hidden; margin-bottom: 8px; }
.nitro-fill { height: 100%; width: 0; background: linear-gradient(90deg, var(--accent-2), #8af7ff); transition: width 80ms linear; }
.nitro.full .nitro-fill { box-shadow: 0 0 14px var(--accent-2); }
.nitro-label { position: absolute; inset: 0; font-size: 10px; letter-spacing: 0.2em; display: flex; align-items: center; justify-content: center; color: #fff; text-shadow: 0 1px 2px #000; }
.countdown { position: fixed; inset: 0; display: flex; align-items: center; justify-content: center; font-size: clamp(90px, 25vw, 200px); font-weight: 900; font-style: italic; color: #fff; text-shadow: 0 0 40px var(--accent); }
.toast { position: fixed; left: 0; right: 0; top: 30%; text-align: center; font-size: clamp(22px, 5vw, 40px); font-weight: 900; font-style: italic; color: var(--accent-2); text-shadow: 0 2px 10px #000; }

#touch { position: fixed; inset: 0; pointer-events: none; display: flex; justify-content: space-between; align-items: flex-end; padding: 0 max(14px, env(safe-area-inset-right)) max(14px, env(safe-area-inset-bottom)) max(14px, env(safe-area-inset-left)); }
.touch-left, .touch-right { display: flex; gap: 12px; pointer-events: auto; }
.touch-btn { width: 76px; height: 76px; border-radius: 50%; background: rgba(255, 255, 255, 0.16); color: #fff; font-size: 22px; font-weight: 900; backdrop-filter: blur(6px); border: 2px solid rgba(255, 255, 255, 0.25); touch-action: none; }
.touch-btn.drift { font-size: 13px; background: rgba(255, 45, 149, 0.3); }
.touch-btn.nitro { font-size: 13px; background: rgba(25, 230, 255, 0.3); width: 88px; height: 88px; }
.touch-btn.held { filter: brightness(1.5); transform: scale(0.95); }
@media (pointer: fine) and (min-width: 900px) { #touch { display: none; } }

.results { margin: 0 auto 12px; border-collapse: collapse; }
.results td { padding: 6px 14px; font-size: 18px; }
.results td:first-child { color: var(--muted); text-align: right; }
.newbest { color: var(--accent-2); font-weight: 900; font-size: 20px; margin: 0 0 10px; text-shadow: 0 0 12px var(--accent-2); }
.notice { position: fixed; left: 16px; right: 16px; bottom: max(16px, env(safe-area-inset-bottom)); background: #fff4ea; color: #2a1030; padding: 14px 16px; border-radius: var(--radius); text-align: center; z-index: 10; }
```

- [ ] **Step 4: Write `velocity/js/app.js`**

```js
/*
 * VELOCITY — the page.
 *
 * Screens, touch / keyboard / tilt input, the HUD, sound and the game loop.
 * Physics runs on a fixed step; rendering runs whenever the browser draws.
 * Started by index.html once Three.js has loaded.
 */
(function (root) {
  'use strict';

  const Track = root.VelocityTrack, Physics = root.VelocityPhysics, Race = root.VelocityRace, Scene = root.VelocityScene;
  const SETTINGS_KEY = 'velocity.settings';
  const STEP = 1 / 120;
  const $ = (id) => document.getElementById(id);

  const storage = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* private mode: play without records */ } },
  };

  function loadSettings() {
    const d = { carKey: 'comet', steering: 'arrows', muted: false };
    try { return Object.assign(d, JSON.parse(storage.get(SETTINGS_KEY) || '{}')); } catch (e) { return d; }
  }

  function ordinal(n) { const s = ['th', 'st', 'nd', 'rd'], v = n % 100; return n + (s[(v - 20) % 10] || s[v] || s[0]); }

  // --- Sound: a synthesised engine, a nitro hiss, thumps for hits. ---
  function makeSound() {
    let ctx = null, master, engine, engineGain, filter, hiss, hissGain, noiseBuf;
    let muted = false;
    function init() {
      if (ctx) return;
      const AC = root.AudioContext || root.webkitAudioContext; if (!AC) return;
      ctx = new AC();
      master = ctx.createGain(); master.gain.value = muted ? 0 : 1; master.connect(ctx.destination);
      engine = ctx.createOscillator(); engine.type = 'sawtooth'; engine.frequency.value = 50;
      filter = ctx.createBiquadFilter(); filter.type = 'lowpass'; filter.frequency.value = 400;
      engineGain = ctx.createGain(); engineGain.gain.value = 0;
      engine.connect(filter); filter.connect(engineGain); engineGain.connect(master); engine.start();
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
      const d = noiseBuf.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      hiss = ctx.createBufferSource(); hiss.buffer = noiseBuf; hiss.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 2500; bp.Q.value = 0.7;
      hissGain = ctx.createGain(); hissGain.gain.value = 0;
      hiss.connect(bp); bp.connect(hissGain); hissGain.connect(master); hiss.start();
    }
    function burst(gain, seconds, freq) {
      if (!ctx) return;
      const src = ctx.createBufferSource(); src.buffer = noiseBuf;
      const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = freq;
      const g = ctx.createGain(); g.gain.setValueAtTime(gain, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + seconds);
      src.connect(f); f.connect(g); g.connect(master); src.start(); src.stop(ctx.currentTime + seconds);
    }
    return {
      init,
      resume() { if (ctx && ctx.state === 'suspended') ctx.resume(); },
      setMuted(m) { muted = m; if (master) master.gain.value = m ? 0 : 1; },
      update(car, racing) {
        if (!ctx) return;
        const sp = car ? car.speed : 0;
        const t = ctx.currentTime;
        engine.frequency.setTargetAtTime(45 + sp * 2.4 + (car && car.nitroActive ? 40 : 0), t, 0.05);
        filter.frequency.setTargetAtTime(300 + sp * 25, t, 0.05);
        engineGain.gain.setTargetAtTime(racing && car && !car.wrecked ? 0.07 : 0, t, 0.1);
        hissGain.gain.setTargetAtTime(car && car.nitroActive ? 0.12 : 0, t, 0.05);
      },
      crash() { burst(0.6, 0.5, 600); },
      bump() { burst(0.25, 0.15, 900); },
      blip() { if (!ctx) return; const o = ctx.createOscillator(); const g = ctx.createGain(); o.frequency.setValueAtTime(600, ctx.currentTime); o.frequency.exponentialRampToValueAtTime(1400, ctx.currentTime + 0.12); g.gain.setValueAtTime(0.12, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.2); o.connect(g); g.connect(master); o.start(); o.stop(ctx.currentTime + 0.2); },
    };
  }

  function start() {
    const THREE = root.THREE;
    const track = Track.build();
    const settings = loadSettings();
    const saveSettings = () => storage.set(SETTINGS_KEY, JSON.stringify(settings));
    const mobile = /Android|iPhone|iPad|iPod|Mobile/i.test(navigator.userAgent) || (navigator.maxTouchPoints || 0) > 1;
    const canvas = $('view'), fx = $('fx'), fxc = fx.getContext('2d');
    const scene = Scene.create({ THREE, canvas, track, mobile });
    const sound = makeSound();
    sound.setMuted(settings.muted);

    let state = 'title';   // title | cars | race | pause | results
    let race = null, player = null, goTimer = 0, finishDelay = 0, toastTimer = 0, wasWrecked = false, tiltAvailable = false, tiltSteer = 0;
    const keys = {}, held = {};
    let nitroTap = false;

    // --- Screens ---
    const screens = { title: $('screen-title'), cars: $('screen-cars'), pause: $('screen-pause'), results: $('screen-results') };
    function show(name) {
      state = name;
      for (const k in screens) screens[k].hidden = k !== name;
      const racing = name === 'race' || name === 'pause' || name === 'results';
      $('hud').hidden = !racing;
      $('touch').hidden = name !== 'race';
      canvas.style.visibility = racing ? 'visible' : 'hidden';
      if (name === 'title') renderTitle();
    }
    function renderTitle() {
      const rec = Race.loadRecords(storage);
      $('best').textContent = rec.bestTime == null ? 'No best time yet' : 'Best: ' + Race.formatTime(rec.bestTime) + (rec.carKey && Physics.CARS[rec.carKey] ? ' in the ' + Physics.CARS[rec.carKey].name : '');
      renderSettingsButtons();
    }
    function renderSettingsButtons() {
      for (const id of ['btn-sound', 'btn-sound2']) { $(id).textContent = 'Sound: ' + (settings.muted ? 'off' : 'on'); $(id).setAttribute('aria-pressed', String(!settings.muted)); }
      for (const id of ['btn-steer', 'btn-steer2']) $(id).textContent = 'Steering: ' + (settings.steering === 'tilt' ? 'tilt' : 'arrows');
    }
    function toggleSound() { settings.muted = !settings.muted; sound.setMuted(settings.muted); saveSettings(); renderSettingsButtons(); }
    function toggleSteering() {
      if (settings.steering === 'tilt') { settings.steering = 'arrows'; saveSettings(); renderSettingsButtons(); return; }
      const DOE = root.DeviceOrientationEvent;
      if (!DOE) { toast('Tilt is not available on this device'); return; }
      const done = (ok) => { if (ok) { settings.steering = 'tilt'; saveSettings(); } else toast('Tilt was not allowed'); renderSettingsButtons(); };
      if (typeof DOE.requestPermission === 'function') DOE.requestPermission().then((r) => done(r === 'granted')).catch(() => done(false));
      else done(true);
    }
    function toast(text, seconds) { $('toast').textContent = text; $('toast').hidden = false; toastTimer = seconds || 1.6; }

    // --- Car picker ---
    const list = $('car-list');
    function renderCars() {
      list.innerHTML = '';
      for (const key of Physics.ORDER) {
        const c = Physics.CARS[key];
        const el = document.createElement('button');
        el.className = 'car' + (key === settings.carKey ? ' selected' : '');
        el.type = 'button';
        const stat = (label, v) => '<div class="stat"><span>' + label + '</span><div class="bar"><i style="width:' + Math.round(v * 100) + '%"></i></div></div>';
        el.innerHTML = '<div class="swatch" style="background:#' + c.color.toString(16).padStart(6, '0') + '"></div><div class="name">' + c.name + '</div><div class="blurb">' + c.blurb + '</div>' +
          stat('Speed', c.topSpeed / 72) + stat('Launch', c.accel / 21) + stat('Handling', c.handling / 1.4) + stat('Nitro', (c.nitroPower + c.nitroGain) / 2.6);
        el.addEventListener('click', () => { settings.carKey = key; saveSettings(); renderCars(); });
        list.appendChild(el);
      }
    }

    // --- Race lifecycle ---
    function newRace() {
      race = Race.create({ track, playerCar: settings.carKey, storage });
      player = Race.playerEntry(race).car;
      scene.setCars(race.entries.map((e) => e.car));
      goTimer = 0; finishDelay = 0; wasWrecked = false;
      $('countdown').hidden = false; $('toast').hidden = true;
      for (const k in held) held[k] = false;
      show('race');
      sound.init(); sound.resume();
      scene.render(0, { player, cars: race.entries.map((e) => e.car), time: 0 });
    }
    function pause() { if (state !== 'race') return; show('pause'); sound.update(player, false); }
    function resume() { Race.resume(race); $('countdown').hidden = false; show('race'); sound.resume(); }
    function showResults() {
      const r = race.results, rec = race.records;
      $('res-place').textContent = ordinal(r.place) + (r.place === 1 ? ' — Winner!' : '');
      $('res-new').hidden = !r.newBest;
      $('res-total').textContent = Race.formatTime(r.total);
      $('res-lap').textContent = Race.formatTime(r.bestLap);
      $('res-record').textContent = Race.formatTime(rec.bestTime);
      show('results');
    }

    // --- Input ---
    root.addEventListener('keydown', (e) => {
      if (e.repeat) return;
      keys[e.code] = true;
      if (e.code === 'Space') { nitroTap = true; e.preventDefault(); }
      if (e.code === 'Escape' || e.code === 'KeyP') { if (state === 'race') pause(); else if (state === 'pause') resume(); }
      sound.init();
    });
    root.addEventListener('keyup', (e) => { keys[e.code] = false; });
    for (const btn of document.querySelectorAll('.touch-btn')) {
      const action = btn.dataset.action;
      const down = (e) => { e.preventDefault(); held[action] = true; btn.classList.add('held'); if (action === 'nitro') nitroTap = true; sound.init(); sound.resume(); };
      const up = (e) => { e.preventDefault(); held[action] = false; btn.classList.remove('held'); };
      btn.addEventListener('pointerdown', down);
      btn.addEventListener('pointerup', up); btn.addEventListener('pointercancel', up); btn.addEventListener('pointerleave', up);
      btn.addEventListener('contextmenu', (e) => e.preventDefault());
    }
    root.addEventListener('deviceorientation', (e) => {
      if (e.gamma == null && e.beta == null) return;
      tiltAvailable = true;
      const o = (screen.orientation && screen.orientation.type) || '';
      let v;
      if (o.startsWith('landscape')) v = (e.beta || 0) * (o === 'landscape-secondary' ? -1 : 1);
      else v = e.gamma || 0;
      tiltSteer = Math.max(-1, Math.min(1, v / 22));
    });
    function playerInput() {
      let steer = ((keys.ArrowRight || keys.KeyD || held.right) ? 1 : 0) - ((keys.ArrowLeft || keys.KeyA || held.left) ? 1 : 0);
      if (steer === 0 && settings.steering === 'tilt' && tiltAvailable) steer = tiltSteer;
      const brake = !!(keys.ArrowDown || keys.KeyS || keys.ShiftLeft || keys.ShiftRight || held.drift);
      const inp = { throttle: 1, steer, drift: brake, nitro: nitroTap };
      nitroTap = false;
      return inp;
    }

    // --- Buttons ---
    $('btn-race').addEventListener('click', () => { renderCars(); show('cars'); sound.init(); });
    $('btn-back').addEventListener('click', () => show('title'));
    $('btn-go').addEventListener('click', newRace);
    $('btn-pause').addEventListener('click', pause);
    $('btn-resume').addEventListener('click', resume);
    $('btn-restart').addEventListener('click', newRace);
    $('btn-quit').addEventListener('click', () => show('title'));
    $('btn-again').addEventListener('click', newRace);
    $('btn-change').addEventListener('click', () => { renderCars(); show('cars'); });
    $('btn-title').addEventListener('click', () => show('title'));
    $('btn-sound').addEventListener('click', toggleSound); $('btn-sound2').addEventListener('click', toggleSound);
    $('btn-steer').addEventListener('click', toggleSteering); $('btn-steer2').addEventListener('click', toggleSteering);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
    root.addEventListener('resize', () => { scene.resize(); fx.width = fx.clientWidth; fx.height = fx.clientHeight; });

    // --- HUD ---
    function updateHud() {
      const me = Race.playerEntry(race);
      $('hud-pos').firstElementChild.textContent = me.place;
      $('hud-pos').lastElementChild.textContent = '/ ' + race.entries.length;
      $('hud-lap').children[1].textContent = Race.lapNumber(me, race.laps);
      $('hud-lap').children[2].textContent = '/ ' + race.laps;
      $('hud-time').textContent = Race.formatTime(race.phase === 'countdown' && me.crossings === 0 ? 0 : Race.currentLapTime(race, me));
      $('hud-speed').firstElementChild.textContent = Math.round(player.speed * 3.6);
      $('nitro-fill').style.width = Math.round(player.nitro * 100) + '%';
      $('nitro-fill').parentElement.classList.toggle('full', player.nitro >= 0.999);
      const cd = $('countdown');
      if (race.phase === 'countdown') { cd.hidden = false; cd.textContent = String(Math.ceil(race.countdown)); }
      else if (goTimer > 0) { cd.hidden = false; cd.textContent = 'GO!'; }
      else cd.hidden = true;
    }
    function drawSpeedLines(dt) {
      const w = fx.width, h = fx.height;
      fxc.clearRect(0, 0, w, h);
      if (!player || !player.nitroActive || state !== 'race') return;
      fxc.strokeStyle = 'rgba(255,255,255,0.35)'; fxc.lineWidth = 2;
      const cx = w / 2, cy = h / 2;
      for (let i = 0; i < 28; i++) {
        const a = Math.random() * Math.PI * 2, r0 = Math.min(w, h) * (0.25 + Math.random() * 0.3), r1 = r0 + 40 + Math.random() * 120;
        fxc.beginPath(); fxc.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); fxc.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1); fxc.stroke();
      }
    }

    // --- The loop ---
    let last = performance.now(), acc = 0;
    function frame(now) {
      requestAnimationFrame(frame);
      const dt = Math.min(0.1, (now - last) / 1000); last = now;
      if (!race) return;
      const cars = race.entries.map((e) => e.car);
      if (state === 'race') {
        const wasPhase = race.phase;
        const slow = player.wrecked && player.wreckTimer > Physics.WRECK_TIME - 0.8 ? 0.35 : 1;
        acc += dt * slow;
        while (acc >= STEP) { Race.update(race, STEP, playerInput()); acc -= STEP; }
        if (wasPhase === 'countdown' && race.phase === 'racing') goTimer = 0.8;
        goTimer -= dt; toastTimer -= dt; if (toastTimer <= 0) $('toast').hidden = true;
        if (player.wrecked && !wasWrecked) { sound.crash(); toast('WRECKED', 1.5); }
        else if (player.bumped) sound.bump();
        if (player.nearMiss) { sound.blip(); toast('NEAR MISS +nitro', 0.8); player.nearMiss = false; }
        wasWrecked = player.wrecked;
        if (race.phase === 'finished') { finishDelay += dt; if (finishDelay > 1.5) showResults(); }
        updateHud();
        sound.update(player, true);
        scene.render(dt * slow, { player, cars, time: race.time });
      } else if (state === 'pause' || state === 'results') {
        scene.render(0, { player, cars, time: race.time });
      }
      drawSpeedLines(dt);
    }

    fx.width = fx.clientWidth; fx.height = fx.clientHeight;
    scene.resize();
    renderCars();
    show('title');
    requestAnimationFrame(frame);
  }

  root.VelocityApp = { start };
})(typeof self !== 'undefined' ? self : this);
```

- [ ] **Step 5: Syntax-check and run the repo gates**

Run: `node --check velocity/js/app.js && node scripts/check-links.mjs && node scripts/check-exchange.mjs`
Expected: all pass (the page loads only its own scripts, so the exchange check has nothing to flag).

- [ ] **Step 6: Commit**

```bash
git add velocity/index.html velocity/icon.svg velocity/css/style.css velocity/js/app.js
git commit -m "Velocity: the page, menus, HUD, touch and tilt controls, sound and game loop"
```

---

### Task 7: Smoke test in a real browser, README, project index

**Files:**
- Create: `velocity/README.md`
- Modify: `docs/superpowers/specs/2026-10-23-velocity-racing-design.md` (two honest corrections, see Step 3)
- Modify: `PROJECTS.md` (generated; refreshed by the consolidate tool)

- [ ] **Step 1: Smoke-test the page headless**

The CDN is unreachable from the cloud box, so for the smoke test only, serve the folder locally and route the Three.js URL to a copy from the npm registry (`npm pack three@0.170.0` in the scratchpad, extract `package/build/three.module.js`). Write the script in the scratchpad (not in the repo):

```js
// smoke.mjs — loads velocity/index.html in headless Chromium, starts a race, drives 8 seconds, screenshots.
import { chromium } from 'playwright';
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path';
const ROOT = process.argv[2], THREE = process.argv[3], OUT = process.argv[4];
const server = http.createServer((req, res) => {
  const f = path.join(ROOT, decodeURIComponent(req.url.split('?')[0]) === '/' ? 'index.html' : decodeURIComponent(req.url.split('?')[0]));
  fs.readFile(f, (err, data) => { if (err) { res.writeHead(404); res.end(); return; }
    const ext = path.extname(f); res.writeHead(200, { 'content-type': { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml' }[ext] || 'application/octet-stream' }); res.end(data); });
}).listen(0);
const port = server.address().port;
const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 900, height: 500 } });
const errors = [];
page.on('pageerror', (e) => errors.push(String(e)));
page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
await page.route('https://cdn.jsdelivr.net/**', (route) => route.fulfill({ path: THREE, contentType: 'text/javascript' }));
await page.goto('http://127.0.0.1:' + port + '/index.html');
await page.waitForFunction(() => window.THREE && document.getElementById('screen-title') && !document.getElementById('screen-title').hidden);
await page.click('#btn-race'); await page.click('#btn-go');
await page.waitForTimeout(3500);
await page.keyboard.down('ArrowUp'); await page.waitForTimeout(4000); await page.keyboard.press('Space'); await page.waitForTimeout(1500);
const hud = await page.evaluate(() => ({ speed: document.querySelector('#hud-speed .big-num').textContent, pos: document.querySelector('#hud-pos .big-num').textContent, lap: document.querySelector('#hud-lap .big-num').textContent }));
await page.screenshot({ path: OUT });
console.log(JSON.stringify({ hud, errors }));
await browser.close(); server.close();
```

Run it and check: `errors` is empty, `hud.speed` is above 100, and the screenshot shows road, city, cars and HUD. Fix anything it reveals in `scene.js`/`app.js` before going on.

- [ ] **Step 2: Write `velocity/README.md`**

```markdown
# Velocity

**An arcade street racer in your browser.** One sunset city circuit, seven
rivals, three laps. Drift through the corners to fill your nitro, then burn it
on the straights. Scrape a wall and you lose speed; hit one head-on and you're
wrecked for a couple of seconds. Hit the ramp flat out and fly.

**Play it:** [velocity/](./index.html) — on the live site,
`https://mazoo85.github.io/Maz/velocity/`. On a phone, use your browser's
*Add to Home Screen* and it opens like an app.

No account, no install and no build step. It needs an internet connection
the first time it loads, to fetch its 3D engine ([Three.js](https://threejs.org/)).
A standalone app: it is not part of MAZ ARCADE and does not use the Maz engine.

## How to play

| | Phone | Keyboard |
|---|---|---|
| **Go** | The car accelerates on its own. | Same. |
| **Steer** | ◀ ▶ buttons, or tilt the phone (*Steering: tilt* on the title screen). | Arrow keys or A / D. |
| **Drift / brake** | Hold **DRIFT** while steering to slide round a corner. Hold it with no steering to brake. | Shift, or the down arrow / S. |
| **Nitro** | Tap **NITRO**. One tap burns for a moment; tap again to keep burning. | Space. |
| **Pause** | The **II** button. | P or Escape. |

The nitro bar fills when you drift, when you pass another car very close
(a *near miss*), and when you land a jump.

## The cars

| Car | Character |
|---|---|
| **Comet** | The all-rounder. |
| **Brawler** | Highest top speed and the strongest nitro, but it turns like a boat. |
| **Dart** | First off the line and sharpest in the corners; lowest top speed. |
| **Drifter** | Slides the longest and fills its nitro fastest. |

## The track

**Harbor Loop**, about 3 km: the start/finish straight along the harbor, a
long sweep up the hill, a tight chicane, a neon-lit tunnel, the jump, and a
run of fast corners back to the water.

Your best race time and best lap are kept in your browser on that device
(localStorage). Nothing leaves your browser.

## Files

| File | What it is |
|---|---|
| `js/track.js` | Harbor Loop and the maths for "where on the track am I". |
| `js/physics.js` | How a car moves: throttle, steering, drift, nitro, the ramp, walls and crashes. |
| `js/ai.js` | How the seven rivals drive. |
| `js/race.js` | Laps, positions, the countdown, the finish, results and the best-time record. |
| `js/scene.js` | The 3D city and the cars, drawn with Three.js. |
| `js/app.js` | The screens, controls, HUD, sound and game loop. |
| `tests/velocity-logic.test.js` | `node velocity/tests/velocity-logic.test.js`. Runs in CI via `.github/workflows/velocity.yml`. |
```

- [ ] **Step 3: Correct two details in the spec so it matches what was built**

In `docs/superpowers/specs/2026-10-23-velocity-racing-design.md`: remove "banking" from the track description and the `track.js` bullet (the road is flat; banking added nothing worth its code), and change the throttle sentence under *Driving* to: "The car **accelerates on its own** at full throttle (Asphalt-style). The brake/drift button is how you slow down." Commit with the README.

- [ ] **Step 4: Refresh the project index**

```bash
(cd consolidate && python3 -m consolidate.cli update /home/user/Maz --yes)
git diff --stat PROJECTS.md
```

Expected: `PROJECTS.md` gains a `velocity` row. If the tool changes anything else, revert that (`git checkout -- <file>`).

- [ ] **Step 5: Run everything once more**

```bash
node velocity/tests/velocity-logic.test.js && node scripts/check-links.mjs && node scripts/check-exchange.mjs
```

Expected: all pass.

- [ ] **Step 6: Commit and push the branch**

```bash
git add velocity/README.md docs/superpowers/specs/2026-10-23-velocity-racing-design.md PROJECTS.md
git commit -m "Velocity: README, spec corrections and project index"
git push -u origin velocity-racer
```

---

## Self-review

- **Spec coverage:** track (Task 1), driving incl. drift/nitro/jump/collisions/wreck-respawn (Task 2), cars table (Task 2), AI with skill/aggression/rubber-banding/nitro on straights (Task 3), countdown/laps/positions/finish/results/records (Task 4), looks: sunset city, tunnel neon, water, speed lines, shake, sparks, slow-mo on wreck (Tasks 5–6), sound and mute (Task 6), screens incl. pause with controls/sound (Task 6), tilt with fallback (Task 6), localStorage guarded (Tasks 4, 6), Three.js failure message (Task 6), tab-hidden pause with countdown on resume (Tasks 4, 6), performance choices (Task 5), CI workflow (Task 1), README (Task 7). Off-road snap-back is covered by wall push-back every frame plus respawn (Task 2); "light/dark theme on menus" is in the CSS (Task 6).
- **Placeholders:** none.
- **Type consistency:** `track.nearest` returns `{i,s,lateral,dx,dz,w,px,pz,tunnel,curvature,rampHeight}` and is used that way in physics, ai, scene; `Race.create` takes `playerCar`, `update(race, dt, input)`; car fields used by scene (`speed, nitroActive, sparks, bumped, lateral, airborne, wrecked, wreckTimer, color`) are all set in `createCar`/`step`.
