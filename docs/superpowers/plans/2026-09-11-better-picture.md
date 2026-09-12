# Better Picture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SCRIPT FORGE films get characters that act, sets with depth and weather, and a camera whose moves mean something — all driven by the beat and tension the reel already carries.

**Architecture:** `film/js/film-art.js` splits by responsibility into art (palette and film effects), `film-sets.js` (fifteen sets, each in three layers) and `film-figures.js` (a jointed figure whose pose is a set of angles). Everything that *chooses* — which pose, which weather, how to cut — is pure logic testable in Node; only drawing needs a browser.

**Tech Stack:** ES5-style browser JavaScript in IIFEs, Canvas 2D, Node for logic tests, Playwright + Chromium for browser tests. No dependencies are added.

**Spec:** `docs/superpowers/specs/2026-09-11-better-picture-design.md`

## Global Constraints

- **No new dependencies.** Everything runs offline from a static file server.
- **House module style:** each file is an IIFE ending `if (typeof module === 'object' && module.exports) module.exports = API; root.Name = API;`, loaded by a plain `<script>` tag. `film/js/*` is **ES5**: `var`, `function`, no arrow functions, no `const`/`let`.
- **Globals are taken.** `window.FilmScore` is the audio module and `window.FilmConductor` is the score conductor. New modules bind `window.FilmSets` and `window.FilmFigures`. Do not reuse an existing name.
- **`music/` is untouched by this whole plan.** The score landed in the previous sub-project.
- **Tests are `node <file>`** with the home-grown harness (`test`, `assert`, `eq`) in `film/tests/film-logic.test.js`; its final report block must stay last in the file. The browser suite is `film/tests/film-browser.test.js` with `check(cond, msg)`.
- **Determinism:** the same film must draw the same way every time. All variation comes from a seed derived from the film's seed.
- **Frame budget:** mean frame cost must stay under **4ms at 960×540** and **6ms at 1920×1080**. Today it is 0.46ms and 0.40ms.
- **Commit after every task** with a message describing the behaviour, not the file list.

---

### Task 1: Split the renderer by responsibility

**Files:**
- Modify: `film/js/film-art.js` (654 lines — reduces to palette and effects)
- Create: `film/js/film-sets.js`, `film/js/film-figures.js`
- Modify: `film/index.html` (load the two new files before `film-player.js`)
- Modify: `film/js/film-player.js` (read sets and figures from their new homes)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `FilmArt` keeps `palette(genre, time, mood)`, `rgb`, `mix`, `noise`, and the grain/vignette helpers it already exports.
  - `FilmSets.SETS` — the same fifteen set functions, moved verbatim, still `function (ctx, p, n)`.
  - `FilmFigures.drawFigure(ctx, p, x, groundY, height, tint, speaking, wobble)` and `FilmFigures.GLYPHS`, `FilmFigures.glyphFor(object)` — moved verbatim.

**This is a pure move. No behaviour changes. If any test output differs, something was dropped.**

- [ ] **Step 1: Record the baseline**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS. Note the film logic test count (69 at time of writing) — it must be identical at the end of this task.

- [ ] **Step 2: Move the sets**

Create `film/js/film-sets.js` with the house IIFE preamble:

```js
/*
 * SCRIPT FORGE — the sets.
 * ------------------------
 * Fifteen places a scene can happen, drawn in code. Moved out of film-art.js,
 * which now keeps only the palette and the film-stock effects.
 *
 * Exposed as window.FilmSets (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});
  var rgb = Art.rgb;
  var mix = Art.mix;

  var SETS = {};

  // ... every SETS.* function moved verbatim from film-art.js ...

  var API = { SETS: SETS };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmSets = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

Move all fifteen `SETS.*` functions across unchanged, along with the `ground()` helper they share. Delete them from `film-art.js`.

- [ ] **Step 3: Move the figures and glyphs**

Create `film/js/film-figures.js` the same way, binding `root.FilmFigures`, and move `bodyPath`, `armsPath`, `drawFigure`, `GLYPHS`, `GLYPH_FOR` and `glyphFor` verbatim. Delete them from `film-art.js`.

The figure drawing uses the palette helpers, so give this file the same binding
the sets file gets — later tasks call `Art.rgb` from here:

```js
  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});
```

- [ ] **Step 4: Point the callers at the new homes**

In `film/js/film-player.js`, replace `Art.SETS[shot.set]` with `Sets.SETS[shot.set]`, `Art.drawFigure` with `Figures.drawFigure`, and `Art.glyphFor` with `Figures.glyphFor`, adding at the top of the IIFE beside the existing `Art` binding:

```js
  var Sets = root.FilmSets || (typeof require !== 'undefined' ? require('./film-sets.js') : {});
  var Figures = root.FilmFigures || (typeof require !== 'undefined' ? require('./film-figures.js') : {});
```

In `film/index.html`, add before `js/film-player.js`:

```html
  <script src="js/film-sets.js"></script>
  <script src="js/film-figures.js"></script>
```

- [ ] **Step 5: Prove nothing changed**

Run: `node film/tests/film-logic.test.js`
Expected: PASS with **the same count as Step 1**.

Run: `node film/tests/film-browser.test.js`
Expected: PASS.

Run: `node scripts/check-links.mjs`
Expected: `✓ everything is connected`.

- [ ] **Step 6: Commit**

```bash
git add film/js/film-art.js film/js/film-sets.js film/js/film-figures.js film/js/film-player.js film/index.html
git commit -m "Split the renderer into art, sets and figures"
```

---

### Task 2: The frame-budget gate

**Files:**
- Modify: `film/tests/film-browser.test.js`

**Interfaces:**
- Consumes: `FilmPlayer.drawFrame(ctx, w, h, reel, time)` (existing).
- Produces: nothing in the app — a test that fails if drawing gets pathologically slow.

Today's measured cost is 0.46ms mean at 960×540 and 0.40ms at 1920×1080. The gate is 4ms and 6ms, which is roughly eight times today's headroom and still well inside a 33ms recording frame.

- [ ] **Step 1: Write the check**

Add to `film/tests/film-browser.test.js`, inside the `THE FILM` section after the existing playback checks:

```js
    console.log('\nFRAME BUDGET');
    const budget = await page.evaluate(() => {
      const script = FilmWriter.write(
        FilmParse.parse("A lonely lighthouse keeper finds a radio that plays tomorrow's news."),
        { length: 'short' });
      const reel = FilmReel.build(script);
      const out = [];
      [[960, 540, 4], [1920, 1080, 6]].forEach(function (spec) {
        const w = spec[0], h = spec[1], limit = spec[2];
        const c = document.createElement('canvas');
        c.width = w; c.height = h;
        const ctx = c.getContext('2d');
        // Warm up first: the first draw of a set builds gradients and patterns.
        for (let i = 0; i < 20; i++) FilmPlayer.drawFrame(ctx, w, h, reel, (i / 20) * reel.duration);
        const times = [];
        const N = 300;
        for (let i = 0; i < N; i++) {
          const t = (i / N) * reel.duration;      // sample the whole film, every set
          const t0 = performance.now();
          FilmPlayer.drawFrame(ctx, w, h, reel, t);
          times.push(performance.now() - t0);
        }
        times.sort((a, b) => a - b);
        out.push({
          size: w + 'x' + h,
          limit: limit,
          mean: times.reduce((a, x) => a + x, 0) / times.length,
          worst: times[N - 1]
        });
      });
      return out;
    });

    budget.forEach((r) => {
      check(r.mean < r.limit,
        `${r.size} draws in ${r.mean.toFixed(2)}ms mean (limit ${r.limit}ms, worst ${r.worst.toFixed(1)}ms)`);
    });
```

- [ ] **Step 2: Run it**

Run: `node film/tests/film-browser.test.js`
Expected: PASS, printing something near `960x540 draws in 0.46ms mean (limit 4ms…)`. If it is already over, stop and report — that is a real regression from an earlier task, not a bad limit.

- [ ] **Step 3: Commit**

```bash
git add film/tests/film-browser.test.js
git commit -m "Fail the build if a frame gets pathologically slow to draw"
```

---

### Task 3: A pose is a set of angles

> **Amendment — `blendPoses` was built to this plan, then deleted.** `poseFor`
> (Task 5) is keyed on the shot, so a pose is constant for the life of a shot
> and only ever changes at a cut — there is no moment *during* a shot where
> two poses need blending, and blending across a cut would be wrong
> regardless, since a character should already be in position when the cut
> lands on them. `blendPoses` ended up with no caller and was removed as dead
> code (`git show 27aae39`), along with its export and its test. The steps
> below are left as originally written, for the record of what was planned;
> Tasks 4 and 6's "Consumes: blendPoses" lines below are stale for the same
> reason — neither `drawFigure` nor `gestureAt` calls it in what shipped.

**Files:**
- Modify: `film/js/film-figures.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `FilmFigures.JOINTS` — `['head', 'torso', 'armL', 'foreL', 'armR', 'foreR', 'legL', 'shinL', 'legR', 'shinR']`
  - `FilmFigures.POSES` — an object of pose name → `{ joint: radians }` covering every joint
  - `FilmFigures.POSE_LIMITS` — `{ joint: [min, max] }` in radians
  - `FilmFigures.blendPoses(a, b, t)` → a new pose, `t` clamped to 0..1 (planned; see the amendment above — not shipped)

Angles are radians, measured as a rotation from the figure's rest axis: `0` is hanging straight down for limbs and upright for head and torso; positive is clockwise on screen.

- [ ] **Step 1: Write the failing test**

Add to `film/tests/film-logic.test.js`, in a new section before the final report block:

```js
/* ================================================================== figures */
const Figures = require(path.join(__dirname, '..', 'js', 'film-figures.js'));

console.log('\nHOW A CHARACTER STANDS');

test('every pose defines every joint, inside a human range', () => {
  const joints = Figures.JOINTS;
  assert(joints.length >= 10, 'not enough joints to make a body');

  Object.keys(Figures.POSES).forEach((name) => {
    const pose = Figures.POSES[name];
    joints.forEach((joint) => {
      eq(typeof pose[joint], 'number', name + ' does not say what ' + joint + ' does');
      const limit = Figures.POSE_LIMITS[joint];
      assert(limit, 'no limit declared for ' + joint);
      assert(pose[joint] >= limit[0] && pose[joint] <= limit[1],
        name + '.' + joint + ' is ' + pose[joint].toFixed(2) + ', outside ' + JSON.stringify(limit));
    });
  });
});

test('the library covers the bearings the story needs', () => {
  ['stand', 'turn-away', 'reach', 'recoil', 'sit', 'slump',
   'hands-in-pockets', 'point', 'head-in-hands', 'walk'].forEach((name) => {
    assert(Figures.POSES[name], 'no pose called ' + name);
  });
});

test('blending two poses stays inside the limits and lands on the target', () => {
  const a = Figures.POSES.stand;
  const b = Figures.POSES['head-in-hands'];

  [0, 0.25, 0.5, 0.75, 1].forEach((t) => {
    const blended = Figures.blendPoses(a, b, t);
    Figures.JOINTS.forEach((joint) => {
      const limit = Figures.POSE_LIMITS[joint];
      assert(blended[joint] >= limit[0] && blended[joint] <= limit[1],
        'blend at ' + t + ' put ' + joint + ' outside its range');
    });
  });

  Figures.JOINTS.forEach((joint) => {
    eq(Figures.blendPoses(a, b, 0)[joint], a[joint], 'blend at 0 is not the first pose');
    eq(Figures.blendPoses(a, b, 1)[joint], b[joint], 'blend at 1 is not the second pose');
  });

  // Out-of-range t must clamp rather than extrapolate into a broken body.
  Figures.JOINTS.forEach((joint) => {
    eq(Figures.blendPoses(a, b, -1)[joint], a[joint], 'a negative blend did not clamp');
    eq(Figures.blendPoses(a, b, 2)[joint], b[joint], 'a blend past 1 did not clamp');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Figures.JOINTS` is undefined.

- [ ] **Step 3: Implement**

Add to `film/js/film-figures.js`, before its `API`:

```js
  /* A body is ten joints. A pose is what angle each one holds, in radians from
   * the rest axis: 0 hangs straight down for a limb, upright for head and
   * torso, and positive turns clockwise on screen. */
  var JOINTS = ['head', 'torso', 'armL', 'foreL', 'armR', 'foreR', 'legL', 'shinL', 'legR', 'shinR'];

  /* What a human can do, so a bad pose fails a test instead of looking wrong. */
  var POSE_LIMITS = {
    head:  [-0.6, 0.6],
    torso: [-0.5, 0.5],
    armL:  [-2.6, 2.6], foreL: [-2.4, 0.2],
    armR:  [-2.6, 2.6], foreR: [-0.2, 2.4],
    legL:  [-0.9, 0.9], shinL: [-0.1, 1.9],
    legR:  [-0.9, 0.9], shinR: [-0.1, 1.9]
  };

  var POSES = {
    'stand':            { head: 0,     torso: 0,     armL: 0.12, foreL: -0.10, armR: -0.12, foreR: 0.10, legL: 0.04, shinL: 0.02, legR: -0.04, shinR: 0.02 },
    'hands-in-pockets': { head: -0.06, torso: 0.04,  armL: 0.30, foreL: -0.70, armR: -0.30, foreR: 0.70, legL: 0.05, shinL: 0.02, legR: -0.05, shinR: 0.02 },
    'turn-away':        { head: 0.42,  torso: 0.22,  armL: 0.05, foreL: -0.20, armR: -0.35, foreR: 0.30, legL: 0.10, shinL: 0.04, legR: -0.12, shinR: 0.05 },
    'reach':            { head: -0.16, torso: -0.12, armL: 1.70, foreL: -0.30, armR: -0.20, foreR: 0.18, legL: 0.14, shinL: 0.05, legR: -0.16, shinR: 0.08 },
    'point':            { head: -0.10, torso: -0.06, armL: 1.45, foreL: -0.08, armR: -0.18, foreR: 0.16, legL: 0.08, shinL: 0.03, legR: -0.08, shinR: 0.03 },
    'recoil':           { head: 0.30,  torso: 0.34,  armL: 0.90, foreL: -1.40, armR: -0.85, foreR: 1.35, legL: -0.22, shinL: 0.30, legR: 0.26, shinR: 0.24 },
    'slump':            { head: 0.34,  torso: 0.40,  armL: 0.08, foreL: -0.12, armR: -0.08, foreR: 0.12, legL: 0.06, shinL: 0.10, legR: -0.06, shinR: 0.10 },
    'head-in-hands':    { head: 0.46,  torso: 0.30,  armL: 1.90, foreL: -2.00, armR: -1.90, foreR: 1.95, legL: 0.05, shinL: 0.05, legR: -0.05, shinR: 0.05 },
    'sit':              { head: 0.10,  torso: 0.12,  armL: 0.35, foreL: -0.55, armR: -0.35, foreR: 0.55, legL: 0.80, shinL: 1.60, legR: -0.80, shinR: 1.60 },
    'walk':             { head: -0.04, torso: -0.05, armL: 0.55, foreL: -0.35, armR: -0.55, foreR: 0.35, legL: 0.45, shinL: 0.30, legR: -0.45, shinR: 0.55 }
  };

  function clamp01(t) {
    return t < 0 ? 0 : t > 1 ? 1 : t;
  }

  /* Moving between poses is interpolating the angles. Clamped, because a body
   * extrapolated past its target is a body with its arm through its chest. */
  function blendPoses(a, b, t) {
    var k = clamp01(t);
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) {
      var joint = JOINTS[i];
      out[joint] = a[joint] + (b[joint] - a[joint]) * k;
    }
    return out;
  }
```

Export `JOINTS`, `POSES`, `POSE_LIMITS` and `blendPoses` on `API`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS, three new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-figures.js film/tests/film-logic.test.js
git commit -m "Give a character ten joints and ten things to do with them"
```

---

### Task 4: Draw the pose

**Files:**
- Modify: `film/js/film-figures.js` (`drawFigure`)
- Test: covered by Task 2's frame budget and a browser check here

**Interfaces:**
- Consumes: `POSES`, `JOINTS`, `blendPoses` (Task 3).
- Produces: `FilmFigures.drawFigure(ctx, p, spot)` where `spot` is
  `{ x, groundY, height, tint, speaking, wobble, pose }` and `pose` is a pose object. **This changes the signature** — `film-player.js` is the only caller.

- [ ] **Step 1: Draw limbs from angles**

Replace `bodyPath`/`armsPath` with segment drawing. Each limb is two tapered segments hinged at a joint:

```js
  /* A tapered segment from (x, y) at `angle`, `length` long, `w0` wide at the
   * root and `w1` at the tip. Returns the tip, so the next segment hangs off it. */
  function segment(ctx, x, y, angle, length, w0, w1) {
    var dx = Math.sin(angle), dy = Math.cos(angle);
    var nx = dy, ny = -dx;                       // normal, for the taper
    var tipX = x + dx * length, tipY = y + dy * length;
    ctx.beginPath();
    ctx.moveTo(x + nx * w0, y + ny * w0);
    ctx.lineTo(tipX + nx * w1, tipY + ny * w1);
    ctx.lineTo(tipX - nx * w1, tipY - ny * w1);
    ctx.lineTo(x - nx * w0, y - ny * w0);
    ctx.closePath();
    ctx.fill();
    return { x: tipX, y: tipY };
  }

  /* The whole body, from the hips up and down, in one fill colour. */
  function drawBody(ctx, h, pose) {
    var hipY = -h * 0.46, hipX = 0;
    var shoulderY = -h * 0.72;
    var unit = h * 0.01;

    // legs
    var kneeL = segment(ctx, hipX - unit * 3, hipY, Math.PI + pose.legL, h * 0.24, unit * 4.5, unit * 3.2);
    segment(ctx, kneeL.x, kneeL.y, Math.PI + pose.legL + pose.shinL, h * 0.24, unit * 3.2, unit * 2.4);
    var kneeR = segment(ctx, hipX + unit * 3, hipY, Math.PI + pose.legR, h * 0.24, unit * 4.5, unit * 3.2);
    segment(ctx, kneeR.x, kneeR.y, Math.PI + pose.legR + pose.shinR, h * 0.24, unit * 3.2, unit * 2.4);

    // torso, leaning from the hips
    var torsoTip = segment(ctx, hipX, hipY, Math.PI + pose.torso, h * 0.26, unit * 6.5, unit * 5.5);

    // arms, hung off the shoulders
    var elbowL = segment(ctx, torsoTip.x - unit * 5, torsoTip.y + unit * 1.5, Math.PI + pose.armL, h * 0.19, unit * 3, unit * 2.2);
    segment(ctx, elbowL.x, elbowL.y, Math.PI + pose.armL + pose.foreL, h * 0.18, unit * 2.2, unit * 1.6);
    var elbowR = segment(ctx, torsoTip.x + unit * 5, torsoTip.y + unit * 1.5, Math.PI + pose.armR, h * 0.19, unit * 3, unit * 2.2);
    segment(ctx, elbowR.x, elbowR.y, Math.PI + pose.armR + pose.foreR, h * 0.18, unit * 2.2, unit * 1.6);

    // head
    ctx.save();
    ctx.translate(torsoTip.x, torsoTip.y);
    ctx.rotate(pose.head);
    ctx.beginPath();
    ctx.ellipse(0, -h * 0.055, h * 0.052, h * 0.062, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
    return { shoulderY: shoulderY };
  }
```

- [ ] **Step 2: Rebuild `drawFigure` around it**

Keep everything that already works — the cast shadow, the halo, the offset-stroke rim light, the character tint — and swap the body for the posed one:

```js
  function drawFigure(ctx, p, spot) {
    var h = spot.height;
    var w = h * 0.34;
    var pose = spot.pose || POSES.stand;
    var rim = spot.speaking ? 0.75 : 0.42;

    ctx.save();
    ctx.translate(spot.x, spot.groundY);
    ctx.rotate((spot.wobble || 0) * 0.004);

    ctx.fillStyle = 'rgba(0,0,0,0.45)';
    ctx.beginPath();
    ctx.ellipse(0, 2, w * 0.75, h * 0.035, 0, 0, Math.PI * 2);
    ctx.fill();

    var halo = ctx.createRadialGradient(0, -h * 0.55, h * 0.05, 0, -h * 0.55, h * 0.75);
    halo.addColorStop(0, Art.rgb(p.key, 0.13));
    halo.addColorStop(1, Art.rgb(p.key, 0));
    ctx.fillStyle = halo;
    ctx.fillRect(-w * 1.6, -h * 1.25, w * 3.2, h * 1.4);

    // rim light: the same body, offset up-left, in the key colour
    ctx.save();
    ctx.translate(-w * 0.055, -h * 0.012);
    ctx.fillStyle = Art.rgb(p.key, rim);
    drawBody(ctx, h, pose);
    ctx.restore();

    ctx.fillStyle = 'rgba(6,6,10,0.97)';
    drawBody(ctx, h, pose);

    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.fillStyle = 'hsla(' + spot.tint + ',65%,58%,' + (spot.speaking ? 0.16 : 0.08) + ')';
    drawBody(ctx, h, pose);
    ctx.restore();

    ctx.restore();
  }
```

- [ ] **Step 3: Update the only caller**

In `film/js/film-player.js`, where figures are drawn, pass the new object and a pose:

```js
        Figures.drawFigure(ctx, pal, {
          x: spot.x, groundY: spot.ground, height: spot.height,
          tint: voice.hue, speaking: speaking, wobble: wobble,
          pose: Figures.POSES.stand
        });
```

- [ ] **Step 4: Look at it, then measure it**

Run: `node film/tests/film-browser.test.js`
Expected: PASS, including the frame budget check from Task 2.

Then render a contact sheet and **actually look at it** before moving on — a bad pose is more noticeable than no pose:

```bash
node film/tests/film-browser.test.js && echo "now render a sheet and view it"
```

Write a throwaway script in `/tmp` that loads `/film/`, draws one frame per pose into a grid canvas with `FilmFigures.POSES`, and saves a PNG. View it. Every pose must read as a human being. Fix any that do not before continuing, and say in your report which you adjusted.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-figures.js film/js/film-player.js
git commit -m "Draw a character from joint angles instead of one fixed outline"
```

---

### Task 5: The story chooses the pose

**Files:**
- Modify: `film/js/film-figures.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `POSES` (Task 3).
- Produces: `FilmFigures.poseFor(beat, tension, isSpeaker, seed)` → a pose **name** (string, a key of `POSES`), and `FilmFigures.POSES_BY_BEAT` → `{ beat: [names] }`.

- [ ] **Step 1: Write the failing test**

```js
test('the beat decides how a character carries themselves', () => {
  const allowed = Figures.POSES_BY_BEAT;
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    assert(allowed[beat] && allowed[beat].length, 'no poses for the ' + beat + ' beat');
    allowed[beat].forEach((name) => assert(Figures.POSES[name], beat + ' asks for a pose that does not exist: ' + name));

    for (let seed = 0; seed < 20; seed++) {
      const chosen = Figures.poseFor(beat, 0.5, false, seed);
      assert(allowed[beat].indexOf(chosen) !== -1,
        beat + ' chose ' + chosen + ', which is not one of its poses');
    }
  });
});

test('the crisis breaks a character and the choice straightens them up', () => {
  const crisis = [];
  const choice = [];
  for (let seed = 0; seed < 30; seed++) {
    crisis.push(Figures.poseFor('crisis', 0.88, false, seed));
    choice.push(Figures.poseFor('choice', 0.5, false, seed));
  }
  crisis.forEach((p) => assert(['recoil', 'slump', 'head-in-hands'].indexOf(p) !== -1,
    'the crisis produced ' + p));
  choice.forEach((p) => assert(['stand', 'point', 'reach'].indexOf(p) !== -1,
    'the choice produced ' + p));
});

test('the same film poses the same way twice', () => {
  for (let seed = 0; seed < 10; seed++) {
    eq(Figures.poseFor('push', 0.52, true, seed), Figures.poseFor('push', 0.52, true, seed));
  }
});

test('a speaker is never turned away from the room', () => {
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    for (let seed = 0; seed < 20; seed++) {
      assert(Figures.poseFor(beat, 0.5, true, seed) !== 'turn-away',
        'a speaking character turned away during ' + beat);
    }
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Figures.poseFor is not a function`.

- [ ] **Step 3: Implement**

```js
  /* What a beat looks like on a body. The film's emotional shape reaches the
   * picture through this table and nothing else. */
  var POSES_BY_BEAT = {
    open:   ['stand', 'hands-in-pockets', 'sit'],
    spark:  ['turn-away', 'reach', 'stand'],
    push:   ['walk', 'point', 'reach'],
    turn:   ['stand', 'turn-away', 'hands-in-pockets'],
    crisis: ['recoil', 'slump', 'head-in-hands'],
    choice: ['stand', 'point', 'reach'],
    after:  ['stand', 'hands-in-pockets', 'sit']
  };

  /* Someone mid-sentence faces the room; turning away is a thing you do while
   * someone else is talking. */
  function poseFor(beat, tension, isSpeaker, seed) {
    var pool = POSES_BY_BEAT[beat] || POSES_BY_BEAT.open;
    if (isSpeaker) {
      pool = pool.filter(function (name) { return name !== 'turn-away'; });
      if (!pool.length) pool = ['stand'];
    }
    // High tension leans on the later entries, which are the more extreme ones.
    var rng = PARSE.makeRng((PARSE.hashText(beat + ':' + seed) ^ Math.round(tension * 1000)) >>> 0);
    var bias = Math.min(0.999, Math.max(0, rng() * (1 - tension * 0.35) + tension * 0.35));
    return pool[Math.floor(bias * pool.length) % pool.length];
  }
```

`film-figures.js` needs the seeded RNG; add beside its other bindings:

```js
  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});
```

Export `poseFor` and `POSES_BY_BEAT`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS, four new tests.

- [ ] **Step 5: Wire it into the player**

In `film/js/film-player.js`, replace the hard-coded `pose: Figures.POSES.stand` with the chosen pose, held per shot so it does not flicker frame to frame:

A shot has no `index` field, but its `start` is unique within a film, so that is
the per-shot key:

```js
        var shotKey = (reel.seed + Math.round(shot.start * 100)) >>> 0;
        var poseName = Figures.poseFor(shot.beat, shot.mood, speaking, shotKey);
        // ... pose: Figures.POSES[poseName]
```

`shot.start` does not change while a shot is on screen, so the pose is held for
the whole shot rather than being re-rolled every frame.

- [ ] **Step 6: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS.

```bash
git add film/js/film-figures.js film/js/film-player.js film/tests/film-logic.test.js
git commit -m "Let the beat and the tension decide how a character stands"
```

---

### Task 6: Move on the voice

**Files:**
- Modify: `film/js/film-figures.js`, `film/js/film-player.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `blendPoses` (Task 3), `poseFor` (Task 5).
- Produces: `FilmFigures.gestureAt(pose, phase)` → a new pose with the head and near hand displaced, where `phase` is 0..1 through one syllable.

The score fires one voice blip per syllable at `max(0.4, duration * 0.78) / syllables` intervals, where `syllables = max(2, round(words * 1.7))`. The same arithmetic gives the picture its timing — see `Score.prototype.speak` in `film/js/film-audio.js`.

- [ ] **Step 1: Write the failing test**

```js
test('a gesture moves the head and hand and nothing else', () => {
  const rest = Figures.POSES.stand;
  const mid = Figures.gestureAt(rest, 0.5);

  assert(Math.abs(mid.head - rest.head) > 0.001, 'the head did not move on a syllable');
  assert(Math.abs(mid.armL - rest.armL) > 0.001 || Math.abs(mid.armR - rest.armR) > 0.001,
    'neither hand moved on a syllable');
  ['legL', 'legR', 'shinL', 'shinR'].forEach((joint) => {
    eq(mid[joint], rest[joint], 'a syllable moved the ' + joint);
  });

  Figures.JOINTS.forEach((joint) => {
    const limit = Figures.POSE_LIMITS[joint];
    [0, 0.25, 0.5, 0.75, 1].forEach((phase) => {
      const g = Figures.gestureAt(rest, phase);
      assert(g[joint] >= limit[0] && g[joint] <= limit[1],
        'a gesture at phase ' + phase + ' put ' + joint + ' outside its range');
    });
  });
});

test('a gesture starts and finishes at rest', () => {
  const rest = Figures.POSES.stand;
  Figures.JOINTS.forEach((joint) => {
    eq(Figures.gestureAt(rest, 0)[joint].toFixed(4), rest[joint].toFixed(4), joint + ' at phase 0');
    eq(Figures.gestureAt(rest, 1)[joint].toFixed(4), rest[joint].toFixed(4), joint + ' at phase 1');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Figures.gestureAt is not a function`.

- [ ] **Step 3: Implement**

```js
  /* One syllable of movement: the head dips and the nearer hand lifts, both
   * returning to rest by the end so syllables can run back to back without the
   * body drifting. The score fires a blip on the same clock. */
  function gestureAt(pose, phase) {
    var swing = Math.sin(Math.max(0, Math.min(1, phase)) * Math.PI);   // 0 → 1 → 0
    var out = {};
    for (var i = 0; i < JOINTS.length; i++) out[JOINTS[i]] = pose[JOINTS[i]];

    out.head = clampJoint('head', pose.head - swing * 0.07);
    out.torso = clampJoint('torso', pose.torso - swing * 0.02);
    out.armR = clampJoint('armR', pose.armR - swing * 0.22);
    out.foreR = clampJoint('foreR', pose.foreR + swing * 0.30);
    return out;
  }

  function clampJoint(joint, value) {
    var limit = POSE_LIMITS[joint];
    return value < limit[0] ? limit[0] : value > limit[1] ? limit[1] : value;
  }
```

Export `gestureAt`.

- [ ] **Step 4: Drive it from the shot's own clock**

In `film/js/film-player.js`, for a speaking figure, work out which syllable is sounding and how far through it we are:

```js
        if (speaking && shot.kind === 'line') {
          var words = String(shot.caption).trim().split(/\s+/).length;
          var syllables = Math.max(2, Math.round(words * 1.7));
          var span = Math.max(0.4, shot.duration * 0.78);
          var gap = span / syllables;
          var into = time - shot.start;
          if (into < span) pose = Figures.gestureAt(pose, (into % gap) / gap);
        }
```

- [ ] **Step 5: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS.

```bash
git add film/js/film-figures.js film/js/film-player.js film/tests/film-logic.test.js
git commit -m "Move a character's head and hand in time with their own voice"
```

---

### Task 7: Sets in three layers

**Files:**
- Modify: `film/js/film-sets.js` (all fifteen sets), `film/js/film-player.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `FilmSets.SETS` (Task 1).
- Produces: each entry of `FilmSets.SETS` becomes `{ back: fn, mid: fn, fore: fn }`, each `function (ctx, p, n)`; plus `FilmSets.PARALLAX = { back: 0.35, mid: 1, fore: 1.7 }`. `FilmPlayer` draws back, then mid, then **the figures**, then fore.

- [ ] **Step 1: Write the failing test**

```js
const Sets = require(path.join(__dirname, '..', 'js', 'film-sets.js'));

console.log('\nSETS WITH DEPTH');

test('every set is built in three layers', () => {
  const names = Object.keys(Sets.SETS);
  assert(names.length >= 15, 'expected fifteen sets, found ' + names.length);
  names.forEach((name) => {
    ['back', 'mid', 'fore'].forEach((layer) => {
      eq(typeof Sets.SETS[name][layer], 'function', name + ' has no ' + layer + ' layer');
    });
  });
});

test('the layers move at different speeds, in the right order', () => {
  const p = Sets.PARALLAX;
  assert(p.back < p.mid && p.mid < p.fore,
    'parallax is not ordered back < mid < fore: ' + JSON.stringify(p));
  assert(p.back > 0 && p.fore < 4, 'parallax rates are out of a sane range');
});

test('every set a scene can ask for still exists', () => {
  Object.keys(Reel.SET_BY_PLACE).forEach((place) => {
    const set = Reel.SET_BY_PLACE[place];
    assert(Sets.SETS[set], place + ' maps to "' + set + '", which no longer exists');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `kitchen has no back layer`.

- [ ] **Step 3: Convert the sets**

Each set's existing body splits by what it draws. Worked example — the kitchen, whose current single function becomes:

```js
  SETS.kitchen = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.25));
      ctx.fillRect(0, 0, 1000, 420);
      var g = ctx.createLinearGradient(120, 40, 420, 300);
      g.addColorStop(0, rgb(p.key, 0.55));
      g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = rgb(p.key, 0.75); ctx.fillRect(140, 50, 220, 170);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(244, 50, 10, 170); ctx.fillRect(140, 128, 220, 10);
      ctx.fillStyle = g; ctx.fillRect(120, 40, 400, 300);
    },
    mid: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(0, 300, 1000, 22);
      ctx.fillRect(600, 60, 340, 120);
      ctx.fillStyle = rgb(p.deep); ctx.fillRect(770, 60, 8, 120);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 322, 1000, 98);
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(660, 268, 44, 32);
      ctx.fillRect(740, 282, 24, 18); ctx.fillRect(784, 282, 24, 18);
    },
    fore: function (ctx, p, n) {
      // A table edge across the bottom of frame, for the camera to move against.
      ctx.fillStyle = 'rgba(0,0,0,0.92)';
      ctx.fillRect(-40, 386, 1080, 60);
      ctx.fillRect(120, 372, 300, 16);
    }
  };
```

Apply the same division to all fifteen — `lighthouse`, `kitchen`, `room`, `corridor`, `woods`, `street`, `field`, `vehicle`, `industrial`, `office`, `bar`, `ship`, `water`, `ward`, `chapel`. **Back** is sky, walls, distant scenery; **mid** is the furniture and structures the characters stand among; **fore** is one dark element near the lens — a table edge, a doorframe, railings, branches, a curtain, a console lip.

Add:

```js
  /* How fast each plane moves under the camera. Back barely shifts; fore
   * swings, which is what tells the eye it is close. */
  var PARALLAX = { back: 0.35, mid: 1, fore: 1.7 };
```

- [ ] **Step 4: Draw them as planes**

In `film/js/film-player.js`, replace the single `draw(ctx, pal, noise)` call. Each layer gets its own transform, scaled by its parallax rate, and the figures go between mid and fore:

```js
    function plane(rate, paint) {
      ctx.save();
      ctx.translate(frameW / 2 + cam.panX * frameW * rate, frameY + frameH / 2 + cam.panY * frameH);
      ctx.scale(scale, scale);
      ctx.translate(-WORLD_W / 2, -WORLD_H / 2);
      paint();
      ctx.restore();
    }

    var set = Sets.SETS[shot.set] || Sets.SETS.room;
    var grain = Art.noise('set-' + shot.set + '-' + reel.seed, 80);
    plane(Sets.PARALLAX.back, function () { set.back(ctx, pal, grain); });
    plane(Sets.PARALLAX.mid, function () { set.mid(ctx, pal, grain); });
    plane(Sets.PARALLAX.mid, function () { /* figures, exactly as they are drawn today */ });
    plane(Sets.PARALLAX.fore, function () { set.fore(ctx, pal, grain); });
```

Split the figure loop by a `foreground` flag on each spot while you are here, so
Task 12 has somewhere to put an over-the-shoulder listener. `figureLayout`
returns no such spots yet, so this changes nothing today:

```js
    var spots = figureLayout(shot);
    function paintFigures(wantForeground) {
      spots.forEach(function (spot) {
        if (!!spot.foreground !== wantForeground) return;
        // ... the existing per-figure drawing
      });
    }
    plane(Sets.PARALLAX.mid, function () { paintFigures(false); });
    plane(Sets.PARALLAX.fore, function () { set.fore(ctx, pal, grain); paintFigures(true); });
```

- [ ] **Step 5: Run everything, then look**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS, including the frame budget.

Render a contact sheet of all fifteen sets (the same technique as Task 4) and **look at it**. Every set must still read as its place, and the foreground element must sit in front of the figures without hiding them. Report which sets you adjusted.

- [ ] **Step 6: Commit**

```bash
git add film/js/film-sets.js film/js/film-player.js film/tests/film-logic.test.js
git commit -m "Build every set in three planes and put the characters between them"
```

---

### Task 8: Weather and air

**Files:**
- Create: `film/js/film-weather.js`
- Modify: `film/index.html`, `film/js/film-player.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: the shot's `genre`, `time`, `set`, `mood`.
- Produces: `FilmWeather.forShot(genre, time, set)` → one of `'rain' | 'dust' | 'fog' | 'shimmer' | 'embers' | 'haze' | 'none'`, and `FilmWeather.draw(ctx, kind, p, time, seed, w, h)`.

- [ ] **Step 1: Write the failing test**

```js
const Weather = require(path.join(__dirname, '..', 'js', 'film-weather.js'));

console.log('\nWEATHER');

test('every genre and hour gets air the artist can draw', () => {
  const drawable = ['rain', 'dust', 'fog', 'shimmer', 'embers', 'haze', 'none'];
  Object.keys(LEX.GENRES).forEach((genre) => {
    ['NIGHT', 'DAY', 'DUSK', 'DAWN'].forEach((time) => {
      Object.keys(Sets.SETS).forEach((set) => {
        const kind = Weather.forShot(genre, time, set);
        assert(drawable.indexOf(kind) !== -1,
          genre + '/' + time + '/' + set + ' asked for "' + kind + '", which nothing draws');
      });
    });
  });
});

test('the obvious cases land where they should', () => {
  eq(Weather.forShot('thriller', 'NIGHT', 'street'), 'rain');
  eq(Weather.forShot('western', 'DAY', 'field'), 'shimmer');
  eq(Weather.forShot('fantasy', 'NIGHT', 'woods'), 'embers');
  eq(Weather.forShot('drama', 'DAY', 'kitchen'), 'dust');
  eq(Weather.forShot('horror', 'NIGHT', 'woods'), 'fog');
});

test('the same shot always has the same weather', () => {
  for (let i = 0; i < 5; i++) {
    eq(Weather.forShot('mystery', 'DUSK', 'office'), Weather.forShot('mystery', 'DUSK', 'office'));
  }
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — cannot find `film-weather.js`.

- [ ] **Step 3: Implement the choice**

Create `film/js/film-weather.js` in the house style, binding `root.FilmWeather`:

```js
  var EXTERIOR = { street: 1, woods: 1, field: 1, water: 1 };

  /* What hangs in the air. Genre first, then the hour, then whether we are
   * outdoors — a rainstorm indoors is a mistake, not a mood. */
  function forShot(genre, time, set) {
    var outside = !!EXTERIOR[set];
    var night = time === 'NIGHT' || time === 'DUSK';

    if (genre === 'western' && !night) return 'shimmer';
    if (genre === 'fantasy') return 'embers';
    if ((genre === 'horror' || genre === 'mystery') && outside) return 'fog';
    if ((genre === 'thriller' || genre === 'horror') && night) return outside ? 'rain' : 'haze';
    if (!night && !outside) return 'dust';
    if (night) return 'haze';
    return 'none';
  }
```

- [ ] **Step 4: Draw it**

Add `draw(ctx, kind, p, time, seed, w, h)`, with particle counts capped and scaled to the canvas so 1080p does not cost four times 540p:

```js
  /* Particles are capped and scaled: a bigger canvas gets slightly more air,
   * not proportionally more, because the frame budget is shared with everything
   * else on screen. */
  function count(base, w) {
    return Math.min(base, Math.round(base * Math.sqrt(w / 960)));
  }

  function draw(ctx, kind, p, time, seed, w, h) {
    if (!kind || kind === 'none') return;
    var n = Art.noise('weather-' + kind + '-' + seed, 120);
    ctx.save();
    if (kind === 'rain') {
      ctx.strokeStyle = Art.rgb(p.key, 0.18);
      ctx.lineWidth = Math.max(1, w / 900);
      for (var i = 0; i < count(90, w); i++) {
        var s = n[i % n.length];
        var x = ((s[0] + time * 0.06) % 1) * w;
        var y = ((s[1] + time * 0.9) % 1) * h;
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x - w * 0.006, y + h * 0.045);
        ctx.stroke();
      }
    } else if (kind === 'dust') {
      for (var d = 0; d < count(60, w); d++) {
        var q = n[d % n.length];
        var dx = ((q[0] + time * 0.01) % 1) * w;
        var dy = ((q[1] + Math.sin(time * 0.3 + q[2] * 6) * 0.02 + time * 0.006) % 1) * h;
        ctx.fillStyle = Art.rgb(p.key, 0.05 + q[2] * 0.13);
        ctx.fillRect(dx, dy, Math.max(1, w / 700), Math.max(1, w / 700));
      }
    } else if (kind === 'fog' || kind === 'haze') {
      var bands = kind === 'fog' ? 5 : 2;
      for (var b = 0; b < bands; b++) {
        var r = n[b];
        var by = h * (0.25 + r[0] * 0.6) + Math.sin(time * 0.12 + b) * h * 0.02;
        var g = ctx.createLinearGradient(0, by - h * 0.12, 0, by + h * 0.12);
        g.addColorStop(0, Art.rgb(p.key, 0));
        g.addColorStop(0.5, Art.rgb(p.key, kind === 'fog' ? 0.10 : 0.06));
        g.addColorStop(1, Art.rgb(p.key, 0));
        ctx.fillStyle = g;
        ctx.fillRect(0, by - h * 0.12, w, h * 0.24);
      }
    } else if (kind === 'shimmer') {
      for (var m = 0; m < 3; m++) {
        var sh = n[m + 7];
        var sy = h * (0.55 + sh[0] * 0.3);
        ctx.fillStyle = Art.rgb(p.key, 0.05);
        ctx.fillRect(0, sy + Math.sin(time * 2 + m) * h * 0.006, w, h * 0.012);
      }
    } else if (kind === 'embers') {
      for (var e = 0; e < count(40, w); e++) {
        var v = n[e % n.length];
        var ex = ((v[0] + Math.sin(time * 0.4 + v[2] * 9) * 0.03) % 1) * w;
        var ey = h - ((v[1] + time * 0.05) % 1) * h;
        ctx.fillStyle = Art.rgb(p.accent, 0.15 + v[2] * 0.5);
        ctx.fillRect(ex, ey, Math.max(1, w / 640), Math.max(1, w / 640));
      }
    }
    ctx.restore();
  }
```

- [ ] **Step 5: Hang it in the frame**

Load it in `film/index.html` before `js/film-player.js`, and in `drawFrame` draw it **after the fore layer and before the captions**, in screen space (not a world plane), so it sits between the audience and everything:

```js
    Weather.draw(ctx, Weather.forShot(reel.genre, shot.time, shot.set),
      pal, time, reel.seed, frameW, frameH);
```

- [ ] **Step 6: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js && node scripts/check-links.mjs`
Expected: all PASS, frame budget included. If the budget check fails, thin the particle counts — that is what the cap is for — and say so in your report.

```bash
git add film/js/film-weather.js film/js/film-player.js film/index.html film/tests/film-logic.test.js
git commit -m "Put something in the air, chosen by genre and hour"
```

---

### Task 9: Light that moves

**Files:**
- Modify: `film/js/film-sets.js`, `film/js/film-player.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: the shot's `set`, `mood`, `time`.
- Produces: `FilmSets.LIGHT` — `{ setName: 'sweep' | 'passing' | 'flicker' | 'cloud' | 'none' }`, and `FilmSets.lightAt(kind, time, tension)` → `{ brightness, offset }` where `brightness` is a multiplier around 1 and `offset` is -1..1 across the frame.

- [ ] **Step 1: Write the failing test**

```js
test('light moves within a scene, and stays in a sane range', () => {
  Object.keys(Sets.SETS).forEach((set) => {
    const kind = Sets.LIGHT[set];
    assert(['sweep', 'passing', 'flicker', 'cloud', 'none'].indexOf(kind) !== -1,
      set + ' declares light "' + kind + '", which nothing draws');
  });

  ['sweep', 'passing', 'flicker', 'cloud', 'none'].forEach((kind) => {
    for (let t = 0; t < 40; t++) {
      [0.15, 0.5, 0.88].forEach((tension) => {
        const light = Sets.lightAt(kind, t * 0.37, tension);
        assert(light.brightness > 0.4 && light.brightness < 2.2,
          kind + ' went to brightness ' + light.brightness.toFixed(2));
        assert(light.offset >= -1 && light.offset <= 1,
          kind + ' put its light at ' + light.offset.toFixed(2));
      });
    }
  });
});

test('a bulb flickers harder when the story is tense', () => {
  const spread = (tension) => {
    let lo = 2, hi = 0;
    for (let t = 0; t < 200; t++) {
      const b = Sets.lightAt('flicker', t * 0.11, tension).brightness;
      if (b < lo) lo = b;
      if (b > hi) hi = b;
    }
    return hi - lo;
  };
  assert(spread(0.88) > spread(0.15) * 1.5,
    'the crisis flickers no harder than the opening');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Sets.LIGHT is undefined`.

- [ ] **Step 3: Implement**

```js
  /* What kind of light each place owns. */
  var LIGHT = {
    lighthouse: 'sweep',  vehicle: 'passing', street: 'passing',
    industrial: 'flicker', corridor: 'flicker', ward: 'flicker',
    field: 'cloud', water: 'cloud', woods: 'cloud',
    kitchen: 'none', room: 'none', office: 'none',
    bar: 'none', ship: 'none', chapel: 'none'
  };

  /* Light that changes during a scene rather than only between them. Tension
   * makes a bulb more agitated; it does not touch the sun. */
  function lightAt(kind, time, tension) {
    var t = tension == null ? 0.4 : tension;
    if (kind === 'sweep') {
      return { brightness: 1 + 0.35 * Math.max(0, Math.sin(time * 0.55)), offset: Math.sin(time * 0.55) };
    }
    if (kind === 'passing') {
      var phase = (time * 0.28) % 1;
      var near = Math.max(0, 1 - Math.abs(phase - 0.5) * 4);
      return { brightness: 1 + near * 0.5, offset: phase * 2 - 1 };
    }
    if (kind === 'flicker') {
      var depth = 0.05 + t * 0.35;
      var jitter = Math.sin(time * 31.7) * Math.sin(time * 7.3) * Math.sin(time * 2.1);
      return { brightness: 1 - depth * Math.max(0, jitter), offset: 0 };
    }
    if (kind === 'cloud') {
      return { brightness: 1 - 0.18 * Math.max(0, Math.sin(time * 0.08)), offset: 0 };
    }
    return { brightness: 1, offset: 0 };
  }
```

Export `LIGHT` and `lightAt`.

- [ ] **Step 4: Apply it to the frame**

In `drawFrame`, take the light before drawing and fold `brightness` into the key-light wash already drawn over the frame, and `offset` into where that wash is centred:

```js
    var light = Sets.lightAt(Sets.LIGHT[shot.set] || 'none', time, shot.mood);
    // ... the existing key-light gradient, with its alpha multiplied by
    // light.brightness and its origin shifted by light.offset * frameW * 0.3
```

- [ ] **Step 5: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS.

```bash
git add film/js/film-sets.js film/js/film-player.js film/tests/film-logic.test.js
git commit -m "Let the light move while a scene plays"
```

---

### Task 10: A camera with intent

**Files:**
- Modify: `film/js/film-player.js` (`framingFor`), `film/js/film-reel.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: the shot's `camera`, `mood`, `framing`.
- Produces: `framingFor(shot, progress, time)` — **this adds a third parameter.** Today the function is `framingFor(shot, progress)` (`film/js/film-player.js:63`) and `drawFrame` calls it with two arguments; both change in this task. It additionally honours `camera` values `'handheld'`, `'track-l'`, `'track-r'`, `'whip'`, and returns `{ zoom, panX, panY, roll }` — `roll` in radians, new. The reel may now choose those cameras.

The handheld and whip moves need a clock that keeps running, which `progress`
does not give them: `progress` is 0..1 through the shot, so two shots of
different lengths would shake at different speeds. `time` is the film's own
elapsed seconds, already available in `drawFrame`.

- [ ] **Step 1: Write the failing test**

```js
console.log('\nTHE CAMERA');

test('every camera the reel can ask for is one the player knows', () => {
  const known = ['push', 'push-slow', 'pull', 'pan-l', 'pan-r', 'static',
                 'handheld', 'track-l', 'track-r', 'whip'];
  ['micro', 'short', 'festival'].forEach((length) => {
    for (let seed = 0; seed < 8; seed++) {
      const reel = Reel.build(Writer.write(Parse.parse('a ghost in the attic', { seed }), { length, seed }));
      reel.shots.forEach((shot) => {
        assert(known.indexOf(shot.camera) !== -1, 'unknown camera: ' + shot.camera);
      });
    }
  });
});

test('the camera stays pointed at the set, whatever the move', () => {
  const known = ['push', 'push-slow', 'pull', 'pan-l', 'pan-r', 'static',
                 'handheld', 'track-l', 'track-r', 'whip'];
  known.forEach((camera) => {
    [0, 0.25, 0.5, 0.75, 1].forEach((progress) => {
      const f = PlayerLib.framingFor({ camera, framing: 'mid', mood: 0.88, kind: 'action' }, progress, progress * 3);
      assert(f.zoom > 0.8 && f.zoom < 3.2, camera + ' zoomed to ' + f.zoom.toFixed(2));
      assert(Math.abs(f.panX) < 0.5 && Math.abs(f.panY) < 0.5, camera + ' panned off the set');
      assert(Math.abs(f.roll || 0) < 0.09, camera + ' rolled ' + (f.roll || 0).toFixed(3) + ' radians');
    });
  });
});

test('handheld is steady when the story is calm and unsteady when it is not', () => {
  const wobble = (mood) => {
    let lo = 9, hi = -9;
    for (let i = 0; i < 200; i++) {
      const f = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood, kind: 'action' }, 0.5, i * 0.05);
      if (f.panX < lo) lo = f.panX;
      if (f.panX > hi) hi = f.panX;
    }
    return hi - lo;
  };
  assert(wobble(0.88) > wobble(0.15) * 1.8, 'the crisis is no shakier than the opening');
});

test('only the crisis is allowed to tilt', () => {
  const calm = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood: 0.2, kind: 'action' }, 0.5, 1);
  const crisis = PlayerLib.framingFor({ camera: 'handheld', framing: 'mid', mood: 0.88, kind: 'action' }, 0.5, 1);
  assert(Math.abs(calm.roll || 0) < 0.005, 'a calm shot was tilted');
  assert(Math.abs(crisis.roll || 0) > 0.01, 'the crisis was not tilted');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `unknown camera` or a missing `roll`.

- [ ] **Step 3: Implement the moves**

In `framingFor`, add to the existing `switch` and return `roll`:

```js
      case 'handheld':
        // Three detuned sines beat against each other, so it never loops
        // visibly. Tension decides how far it drifts.
        var shake = 0.004 + (shot.mood || 0.4) * 0.020;
        panX += Math.sin(time * 2.7) * shake + Math.sin(time * 6.1) * shake * 0.4;
        panY += Math.cos(time * 3.3) * shake * 0.8 + Math.sin(time * 5.2) * shake * 0.3;
        zoom *= 1 + 0.01 * p;
        break;
      case 'track-l': panX += 0.13 - 0.26 * p; break;
      case 'track-r': panX += -0.13 + 0.26 * p; break;
      case 'whip':
        // Fast at the start, settling: the tail of a whip pan, not the middle.
        panX += 0.34 * Math.pow(1 - p, 3);
        break;
```

and after the switch:

```js
    // A couple of degrees of roll, and only where the story has come apart.
    var roll = (shot.mood || 0) > 0.7 ? ((shot.mood - 0.7) / 0.3) * 0.035 : 0;
    return { zoom: zoom, panX: panX, panY: panY, roll: roll };
```

Change the declaration to `function framingFor(shot, progress, time)`, and in
`drawFrame` change the existing call to pass the film time:

```js
    var cam = framingFor(shot, progress, time);
```

Apply `cam.roll` in `drawFrame`'s transform, rotating about the centre of the frame before the world translate.

- [ ] **Step 4: Let the reel ask for them**

In `film/js/film-reel.js`, where a shot's camera is chosen, use the new moves where they mean something — handheld at the crisis, tracking on the push, whip on a second shot inside a scene:

The beat inside that loop is `scene.beat.id` — there is no `beatId` variable.
Replace the `camera:` line of the **action** shot (`film/js/film-reel.js:180`):

```js
            camera: scene.beat.id === 'crisis' ? 'handheld'
                  : scene.beat.id === 'push' ? (rng() < 0.5 ? 'track-l' : 'track-r')
                  : framing === 'insert' ? 'push-slow'
                  : (rng() < 0.5 ? 'push' : 'static'),
```

and the **line** shot's camera (`film/js/film-reel.js:203`), so a tense
conversation is unsteady too:

```js
            camera: scene.beat.id === 'crisis' ? 'handheld'
                  : both ? 'static' : 'push-slow',
```

The whip pan is a transition *between* two shots inside one scene, so it can
only be used on a shot that is not the scene's first. Track that in the scene
loop and let it take over occasionally — otherwise the move ships dead:

```js
      var shotsThisScene = 0;   // reset at the top of each scene, after `var onScreen = [];`
```

then in the action shot, ahead of the choice above:

```js
            camera: (shotsThisScene > 1 && rng() < 0.18) ? 'whip'
                  : scene.beat.id === 'crisis' ? 'handheld'
                  : scene.beat.id === 'push' ? (rng() < 0.5 ? 'track-l' : 'track-r')
                  : framing === 'insert' ? 'push-slow'
                  : (rng() < 0.5 ? 'push' : 'static'),
```

incrementing `shotsThisScene` on every `push()` within the scene. Add a test
that the move is reachable, so a later refactor cannot quietly drop it:

```js
test('the whip pan is actually used somewhere', () => {
  let seen = false;
  for (let seed = 0; seed < 40 && !seen; seed++) {
    const reel = Reel.build(Writer.write(Parse.parse('two thieves argue in a warehouse', { seed }), { length: 'festival', seed }));
    seen = reel.shots.some((s) => s.camera === 'whip');
  }
  assert(seen, 'no film in forty used a whip pan');
});
```

- [ ] **Step 5: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS.

```bash
git add film/js/film-player.js film/js/film-reel.js film/tests/film-logic.test.js
git commit -m "Shake the camera at the crisis and track it on the push"
```

---

### Task 11: Rack focus — prototype, measure, decide

**Files:**
- Modify: `film/js/film-player.js`
- Test: `film/tests/film-browser.test.js`

**Interfaces:**
- Consumes: the layered draw from Task 7.
- Produces: either a working `ctx.filter`-based rack focus, or the cheap fallback — **whichever measures acceptably.** This task is allowed to end with the expensive version deleted.

The frame budget is 4ms at 540p and 6ms at 1080p. Everything else in this plan is vector drawing, which measured flat across resolution. Blur is per-pixel and will not be. **Measure before choosing.**

- [ ] **Step 1: Prototype the real thing**

In `drawFrame`, when the shot's framing is `'close'` or `'two'`, blur the fore plane:

```js
      plane(Sets.PARALLAX.fore, function () {
        if (ctx.filter !== undefined) ctx.filter = 'blur(' + (frameW / 220).toFixed(1) + 'px)';
        set.fore(ctx, pal, grain);
        if (ctx.filter !== undefined) ctx.filter = 'none';
      });
```

- [ ] **Step 2: Measure it**

Run: `node film/tests/film-browser.test.js`
Read the frame-budget line in the output for both sizes and **write the numbers into your report.**

- [ ] **Step 3: Decide, and say why**

If both sizes stay inside the budget, keep it and commit.

If either is over, delete the blur entirely and use the cheap version instead — darkening and flattening the out-of-focus plane, which reads nearly as well and costs nothing:

```js
      plane(Sets.PARALLAX.fore, function () {
        set.fore(ctx, pal, grain);
        // Out of focus, cheaply: push it down and towards the shadow colour.
        ctx.globalAlpha = 0.82;
        ctx.fillStyle = Art.rgb(pal.shadow, 0.35);
        ctx.fillRect(0, 0, WORLD_W, WORLD_H);
        ctx.globalAlpha = 1;
      });
```

Either way, re-run the browser suite and record the final numbers.

- [ ] **Step 4: Commit**

```bash
git add film/js/film-player.js
git commit -m "Throw the foreground out of focus for close shots"
```

The commit message body must say which version shipped and the measured cost of each.

---

### Task 12: Framings and the cutting

**Files:**
- Modify: `film/js/film-player.js` (`figureLayout`, `framingFor`), `film/js/film-reel.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: everything above.
- Produces: framings `'ots'` and `'low'` understood by `framingFor` and `figureLayout`; the reel alternates framings across a two-hander.

- [ ] **Step 1: Write the failing test**

```js
test('a conversation alternates rather than repeating one framing', () => {
  ['short', 'festival'].forEach((length) => {
    for (let seed = 0; seed < 6; seed++) {
      const reel = Reel.build(Writer.write(Parse.parse('two sisters argue in a kitchen', { seed }), { length, seed }));
      const lines = reel.shots.filter((s) => s.kind === 'line');
      for (let i = 2; i < lines.length; i++) {
        assert(!(lines[i].framing === lines[i - 1].framing && lines[i].framing === lines[i - 2].framing),
          'three spoken shots in a row used ' + lines[i].framing);
      }
    }
  });
});

test('the choice is held longer than the push', () => {
  const reel = Reel.build(Writer.write(Parse.parse('a lighthouse keeper finds a radio'), { length: 'festival' }));
  // Compare seconds per word, not raw duration: a wordier push line would
  // otherwise run longer than a held choice and the test would prove nothing.
  const pace = (beat) => {
    const shots = reel.shots.filter((s) => s.beat === beat && s.kind === 'action');
    assert(shots.length, 'no action shots on the ' + beat + ' beat to measure');
    const secs = shots.reduce((a, s) => a + s.duration, 0);
    const words = shots.reduce((a, s) => a + String(s.caption).trim().split(/\s+/).length, 0);
    return secs / words;
  };
  assert(pace('choice') > pace('push') * 1.2,
    'the choice is cut at the same pace as the push');
});

test('the new framings are understood by the camera', () => {
  ['ots', 'low'].forEach((framing) => {
    const f = PlayerLib.framingFor({ camera: 'static', framing, mood: 0.4, kind: 'line', speaker: 'A' }, 0.5, 1);
    assert(f.zoom > 0.8 && f.zoom < 3.2, framing + ' zoomed to ' + f.zoom.toFixed(2));
    const layout = PlayerLib.figureLayout({ framing, characters: ['A', 'B'], speaker: 'A', kind: 'line' });
    assert(layout.length >= 1, framing + ' put nobody in frame');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — three spoken shots in a row share a framing.

- [ ] **Step 3: Implement the framings**

In `framingFor`, add:

```js
    else if (shot.framing === 'ots') zoom = 1.75;
    else if (shot.framing === 'low') zoom = 1.45;
```

and in `figureLayout`:

```js
    if (shot.framing === 'ots') {
      // The listener is a big dark shape at the edge; the speaker is beyond them.
      return [
        { name: shot.speaker, x: 610, ground: 372, height: 250 },
        { name: (shot.characters || []).filter(function (n) { return n !== shot.speaker; })[0] || shot.speaker,
          x: 250, ground: 470, height: 430, foreground: true }
      ];
    }
    if (shot.framing === 'low') {
      return [{ name: shot.speaker || shot.characters[0], x: 500, ground: 420, height: 330 }];
    }
```

- [ ] **Step 4: Implement the cutting**

In `film-reel.js`, track the last two framings used for spoken shots in a scene and pick one that is not both of them; and give the `choice` beat a longer hold by multiplying its action durations:

Declare this **once inside `build()`**, above the `script.scenes.forEach` loop,
so the run of framings carries across scene boundaries — the test looks at every
spoken shot in the film, not each scene separately:

```js
    /* A two-hander reads as a conversation when the camera changes sides. Never
     * the same framing three times running. `ots` needs two people in frame, so
     * it is only on the table when both of them are. */
    var spokenFramings = [];
    function nextLineFraming(both) {
      var options = both ? ['two', 'ots', 'close'] : ['close', 'low'];
      var last = spokenFramings[spokenFramings.length - 1];
      var prev = spokenFramings[spokenFramings.length - 2];
      var fresh = options.filter(function (f) { return !(last === f && prev === f); });
      if (!fresh.length) fresh = options;
      var pick = fresh[Math.floor(rng() * fresh.length) % fresh.length];
      spokenFramings.push(pick);
      return pick;
    }
```

Then in the `dialogue` branch (`film/js/film-reel.js:198-212`), replace the
`framing` and `characters` lines. Both must agree: `two` and `ots` put two
people on screen, `close` and `low` put one.

```js
          var lineFraming = nextLineFraming(both);
          var pair = lineFraming === 'two' || lineFraming === 'ots';
          push({
            kind: 'line',
            // ...
            framing: lineFraming,
            characters: pair ? onScreen.slice(0, 2) : [speaker],
            // ...
          });
```

Note `both` is already `onScreen.length > 1 && rng() < 0.35`; leave that as it is.

And where an action shot's duration is set (`film/js/film-reel.js:176`), give the
choice its longer hold — the beat there is `scene.beat.id`, there is no `beatId`
variable:

```js
          // The choice should sit a beat longer than is comfortable.
          var hold = scene.beat.id === 'choice' ? 1.35 : 1;
          // duration: Math.max(MIN_ACTION, words(element.text) * ACTION_SECONDS_PER_WORD) * hold,
```

- [ ] **Step 5: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS.

```bash
git add film/js/film-player.js film/js/film-reel.js film/tests/film-logic.test.js
git commit -m "Cut a conversation like a conversation, and hold the choice"
```

---

### Task 13: Say what it looks like now

**Files:**
- Modify: `film/README.md`, `shared/projects.js`, `README.md`

- [ ] **Step 1: Rewrite the picture section of `film/README.md`**

Replace the bullet describing the picture under **The film** with an accurate account: characters built from joint angles who take their bearing from the beat and move in time with their own voice; sets in three planes with the characters between them; weather chosen by genre and hour; light that moves within a scene; and a camera that shakes at the crisis, tracks on the push and tilts only when the story has come apart.

Add `js/film-sets.js`, `js/film-figures.js` and `js/film-weather.js` to the file map, with one line each.

- [ ] **Step 2: Update the hub**

In `shared/projects.js`, extend the film entry's `blurb` to mention that the films are performed and shot rather than merely drawn. Keep the badge list at three.

- [ ] **Step 3: Run everything**

```bash
node scripts/check-links.mjs
node film/tests/film-logic.test.js
node music/tests/music-logic.test.js
node film/tests/film-browser.test.js
node scripts/smoke-site.cjs
```
Expected: all pass.

- [ ] **Step 4: Shoot a film and look at it**

Record a short film end to end through the app at 540p, then view a contact sheet of six frames spread across it. It must show: a character in a pose that suits the beat, a foreground element in front of a figure, visible air, and at least one frame from the crisis that is tilted or shaky. Report what you saw — this is the last gate before the work is called done.

- [ ] **Step 5: Commit**

```bash
git add film/README.md README.md shared/projects.js
git commit -m "Document what the films look like now"
```

---

## Self-review notes

- **Spec coverage:** module split → Task 1; frame-budget gate → Task 2 (baseline measured before planning, recorded in the spec); joint model and pose library → Task 3; drawing a pose → Task 4; pose selection from beat and tension → Task 5; gesture on the voice → Task 6; three layers, figures between mid and fore, foreground element → Task 7; weather → Task 8; moving light → Task 9; camera moves including handheld and Dutch tilt → Task 10; rack focus, measured and decided → Task 11; new framings and cutting rules → Task 12; documentation and a film to look at → Task 13.
- **Two tasks end in looking rather than asserting** (4 and 7, plus the final gate in 13). That is deliberate: a pose or a set that is *wrong* rather than *broken* passes every test. Those steps name what to look for and require a report.
- **Task 11 is allowed to fail its own prototype.** If blur is too expensive it ships the cheap version; the commit message must say which and what both measured.
- **Checked against the real code, not from memory.** Seven mismatches were
  found and fixed in place: `shot.index` does not exist (shots are keyed by
  `start`); `framingFor` takes two arguments today and the third is a signature
  change the caller must follow; the reel has no `beatId` variable, it has
  `scene.beat.id`; `film-figures.js` needs its own `Art` binding once it draws
  with the palette; an over-the-shoulder framing needs two people in frame, so
  it cannot be offered to a single speaker; the choice-held-longer test compared
  raw durations, which word counts would have swamped, and now compares seconds
  per word; and the whip pan was implemented but never chosen by anything, so it
  would have shipped dead.
- **Not covered by tests, by choice:** whether the films look *good*. The suite proves the picture moves, stays in frame, stays inside its budget and never draws a body outside a human range. Taste is the user's call on a finished film.
