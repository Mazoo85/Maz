# Score to Picture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every SCRIPT FORGE film gets a real SONG FORGE score, fitted to its exact length, with sections turning over on the scene cuts, instruments following the tension, and the music ducking under every spoken line.

**Architecture:** A new pure-logic module, `film/js/film-score.js`, turns a reel into a *score request* (genre, mood, tempo, sections with energy and instruments) and a *duck envelope*. SONG FORGE composes from that request and plays it **live** through the film's own `AudioContext`, so the existing recorder captures it with no render step. SONG FORGE and the film app each gain small, optional, backwards-compatible inputs; neither changes behaviour when those inputs are absent.

**Tech Stack:** Plain ES5-style browser JavaScript in IIFEs (the house style in `film/js` and `music/js`), Web Audio API, Node for the logic tests, Playwright + Chromium for the browser tests. No dependencies are added.

**Spec:** `docs/superpowers/specs/2026-09-10-score-to-picture-design.md`

## Global Constraints

- **No new dependencies.** Everything runs offline from a static file server.
- **House module style:** every file is an IIFE ending with `if (typeof module === 'object' && module.exports) module.exports = API; root.Name = API;` and is loaded with a plain `<script>` tag. `film/js/*` uses `var` and ES5 function syntax; `music/js/*` uses `const`/`let`. Match the file you are in.
- **SONG FORGE must keep working unchanged.** Every addition to `music/js/*` is optional; with the new options absent, behaviour is identical and `node music/tests/music-logic.test.js` passes untouched.
- **Tests are `node <file>`** with a home-grown harness (`test`, `assert`, `eq`) — no test framework. Follow the existing style in `film/tests/film-logic.test.js`.
- **Determinism:** the same film must produce the same score. All randomness comes from a seed derived from the film's seed.
- **Music must never block the film.** Any failure falls back to the existing simple bed and says so in the UI.
- **Commit after every task** with a message describing the behaviour, not the file list.

---

### Task 1: The genre and mood map

**Files:**
- Create: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js` (append a new section)

**Interfaces:**
- Consumes: nothing.
- Produces: `FilmConductor.MUSIC_FOR` — an object keyed by the ten film genre ids (`drama`, `thriller`, `horror`, `comedy`, `romance`, `scifi`, `mystery`, `fantasy`, `heist`, `western`), each `{ genre: string, mood: string }` naming a SONG FORGE genre id and mood id.

- [ ] **Step 1: Write the failing test**

Append to `film/tests/film-logic.test.js`, after the existing `CHOOSING A VIDEO FORMAT` section and before the report block at the end:

```js
/* ==================================================================== score
 * The conductor is pure data in, pure data out, so the entire musical shape of
 * a film is checkable here. SONG FORGE's own modules are browser files, so they
 * load the way SONG FORGE's tests load them: in a vm sandbox with a fake window.
 */
const vm = require('vm');
const fs = require('fs');
// `Score` is already taken in this file by film-audio.js; the conductor binds
// as `Conductor`, which is what its own doc comment calls it.
const Conductor = require(path.join(__dirname, '..', 'js', 'film-score.js'));

function loadSongForge() {
  const sandbox = { console: console };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  ['theory.js', 'genres.js', 'composer.js'].forEach((f) => {
    vm.runInContext(
      fs.readFileSync(path.join(__dirname, '..', '..', 'music', 'js', f), 'utf8'),
      sandbox, { filename: f });
  });
  return sandbox;
}

console.log('\nSCORING THE FILM');

test('every film genre maps to music SONG FORGE actually has', () => {
  const forge = loadSongForge();
  const genres = forge.Genres.GENRES;
  const moods = forge.Genres.MOODS;

  Object.keys(LEX.GENRES).forEach((filmGenre) => {
    const pick = Conductor.MUSIC_FOR[filmGenre];
    assert(pick, 'no music for film genre ' + filmGenre);
    assert(genres[pick.genre], filmGenre + ' asks for genre "' + pick.genre + '", which SONG FORGE does not have');
    assert(moods[pick.mood], filmGenre + ' asks for mood "' + pick.mood + '", which SONG FORGE does not have');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Cannot find module '.../film/js/film-score.js'`

- [ ] **Step 3: Write the module**

Create `film/js/film-score.js`:

```js
/*
 * SCRIPT FORGE — the conductor.
 * -----------------------------
 * Turns a reel into a score request SONG FORGE can compose from, and a duck
 * envelope that keeps the music under the dialogue.
 *
 * Pure logic: a reel goes in, plain data comes out. No audio and no DOM, so the
 * musical shape of a film is checkable in Node the way its edit already is.
 *
 * Exposed as window.FilmScore (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* Which of SONG FORGE's genres and moods each kind of film is scored with. */
  var MUSIC_FOR = {
    drama:    { genre: 'cinematic', mood: 'chill' },
    thriller: { genre: 'cinematic', mood: 'driving' },
    horror:   { genre: 'ambient',   mood: 'dark' },
    comedy:   { genre: 'lofi',      mood: 'uplifting' },
    romance:  { genre: 'lofi',      mood: 'dreamy' },
    scifi:    { genre: 'synthwave', mood: 'dreamy' },
    mystery:  { genre: 'cinematic', mood: 'dark' },
    fantasy:  { genre: 'cinematic', mood: 'dreamy' },
    heist:    { genre: 'dnb',       mood: 'driving' },
    western:  { genre: 'cinematic', mood: 'chill' }
  };

  var API = { MUSIC_FOR: MUSIC_FOR };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmScore = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS — the new `every film genre maps to music SONG FORGE actually has` is ok, and the existing count rises by one.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Map every film genre to a score SONG FORGE can play"
```

---

### Task 2: Tempo chosen to land bars on the cuts

**Files:**
- Modify: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `FilmConductor.MUSIC_FOR` (Task 1).
- Produces:
  - `FilmConductor.BEATS_PER_BAR` = `4`, `FilmConductor.BLOCK_BARS` = `4`
  - `FilmConductor.blockSeconds(bpm)` → seconds in one four-bar block
  - `FilmConductor.cutTimes(reel)` → `number[]`, the start time of every scene after the first
  - `FilmConductor.chooseBpm(reel, range)` → integer bpm; `range` is `[low, high]`

- [ ] **Step 1: Write the failing test**

```js
test('the tempo is chosen to land bars near the cuts', () => {
  const reel = Reel.build(sample);
  const range = [70, 110];
  const bpm = Conductor.chooseBpm(reel, range);
  assert(bpm >= range[0] && bpm <= range[1], 'bpm outside the genre range: ' + bpm);
  assert(bpm === Math.round(bpm), 'bpm is not a whole number: ' + bpm);

  // It must be no worse than the middle of the range, or choosing is pointless.
  const error = (candidate) => Conductor.cutTimes(reel).reduce((total, cut) => {
    const block = Conductor.blockSeconds(candidate);
    return total + Math.abs(cut - Math.round(cut / block) * block);
  }, 0);
  const middle = Math.round((range[0] + range[1]) / 2);
  assert(error(bpm) <= error(middle) + 1e-9,
    `chosen ${bpm} (error ${error(bpm).toFixed(2)}s) is worse than ${middle} (${error(middle).toFixed(2)}s)`);

  eq(Conductor.chooseBpm(reel, range), bpm, 'the same reel must choose the same tempo');
  eq(Conductor.chooseBpm(reel, [96, 96]), 96, 'a single-value range must be honoured');
});

test('cut times are the scene starts after the first', () => {
  const reel = Reel.build(sample);
  const cuts = Conductor.cutTimes(reel);
  eq(cuts.length, sample.scenes.length - 1, 'one cut between each pair of scenes');
  cuts.forEach((t, i) => {
    assert(t > 0 && t < reel.duration, 'cut outside the film: ' + t);
    if (i > 0) assert(t > cuts[i - 1], 'cuts are not in order');
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Conductor.chooseBpm is not a function`

- [ ] **Step 3: Implement**

Add to `film/js/film-score.js` before `var API`:

```js
  var BEATS_PER_BAR = 4;      // SONG FORGE's own bar length
  var BLOCK_BARS = 4;         // sections are placed on four-bar blocks

  function blockSeconds(bpm) {
    return (BEATS_PER_BAR * BLOCK_BARS * 60) / bpm;
  }

  /* Where the film cuts from one scene to the next. The title card belongs to
   * the first scene and the end card to the last, so neither makes a cut. */
  function cutTimes(reel) {
    var seen = {};
    var cuts = [];
    reel.shots.forEach(function (shot) {
      if (!shot.scene || seen[shot.scene]) return;
      seen[shot.scene] = true;
      if (cuts.length || shot.scene > 1) cuts.push(shot.start);
    });
    // The first scene's start is not a cut — the film opens there.
    return cuts.slice(1);
  }

  /* Pick the tempo inside the genre's range whose four-bar boundaries land
   * closest to the actual cuts. Ties go to the slower tempo, so the choice is
   * stable for a given film. */
  function chooseBpm(reel, range) {
    var low = Math.round(range && range[0] ? range[0] : 72);
    var high = Math.round(range && range[1] ? range[1] : 108);
    if (high < low) { var swap = low; low = high; high = swap; }

    var cuts = cutTimes(reel);
    var best = low;
    var bestError = Infinity;
    for (var bpm = low; bpm <= high; bpm++) {
      var block = blockSeconds(bpm);
      var error = 0;
      for (var i = 0; i < cuts.length; i++) {
        error += Math.abs(cuts[i] - Math.round(cuts[i] / block) * block);
      }
      if (error < bestError - 1e-9) { bestError = error; best = bpm; }
    }
    return best;
  }
```

and extend the exported object:

```js
  var API = {
    MUSIC_FOR: MUSIC_FOR,
    BEATS_PER_BAR: BEATS_PER_BAR,
    BLOCK_BARS: BLOCK_BARS,
    blockSeconds: blockSeconds,
    cutTimes: cutTimes,
    chooseBpm: chooseBpm
  };
```

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS on both new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Choose the tempo that lands bars closest to the cuts"
```

---

### Task 3: A section per scene

**Files:**
- Modify: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `blockSeconds`, `cutTimes` (Task 2).
- Produces: `FilmConductor.SECTION_TYPE` (beat id → SONG FORGE section type) and `FilmConductor.sectionPlan(reel, bpm)` → array of `{ type, bars, energy, scene }`, in order, `bars` always a multiple of 4 and at least 4, together lasting at least the film's duration.

- [ ] **Step 1: Write the failing test**

```js
test('the section plan covers the whole film', () => {
  ['micro', 'short', 'festival'].forEach((length) => {
    const reel = Reel.build(Writer.write(Parse.parse('a ghost in the attic'), { length }));
    const bpm = Conductor.chooseBpm(reel, [70, 110]);
    const plan = Conductor.sectionPlan(reel, bpm);

    assert(plan.length >= 1, 'no sections for a ' + length + ' film');
    plan.forEach((section) => {
      assert(section.bars >= 4, 'section shorter than four bars: ' + section.bars);
      eq(section.bars % 4, 0, 'section is not a whole number of four-bar blocks');
      assert(section.energy >= 0 && section.energy <= 1, 'energy out of range: ' + section.energy);
      assert(typeof section.type === 'string' && section.type.length > 0, 'section has no type');
    });

    const bars = plan.reduce((total, s) => total + s.bars, 0);
    const seconds = (bars * Conductor.BEATS_PER_BAR * 60) / bpm;
    assert(seconds >= reel.duration,
      `${length}: the music runs ${seconds.toFixed(1)}s but the film runs ${reel.duration.toFixed(1)}s`);
  });
});

test('sections take their type and energy from the beat they cover', () => {
  const reel = Reel.build(sample);
  const plan = Conductor.sectionPlan(reel, Conductor.chooseBpm(reel, [70, 110]));
  eq(plan[0].type, 'intro', 'a film opens on an intro');

  const crisisOrClimax = plan.reduce((best, s) => (s.energy > best.energy ? s : best), plan[0]);
  eq(plan.indexOf(crisisOrClimax) < plan.length - 1, true, 'the highest-energy section is not the last one');
  assert(plan[plan.length - 1].energy <= crisisOrClimax.energy,
    'the film ends on more energy than its peak');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Conductor.sectionPlan is not a function`

- [ ] **Step 3: Implement**

```js
  /* What each story beat sounds like structurally. */
  var SECTION_TYPE = {
    open: 'intro', spark: 'verse', push: 'verse', turn: 'bridge',
    crisis: 'chorus', choice: 'bridge', after: 'outro'
  };

  /* One entry per scene: when it starts, how long it runs, which beat it is. */
  function scenesOf(reel) {
    var scenes = [];
    var byNumber = {};
    reel.shots.forEach(function (shot) {
      if (!shot.scene) return;               // title and end cards
      if (!byNumber[shot.scene]) {
        byNumber[shot.scene] = { scene: shot.scene, start: shot.start, end: 0, beat: shot.beat, mood: shot.mood };
        scenes.push(byNumber[shot.scene]);
      }
      byNumber[shot.scene].end = shot.start + shot.duration;
      if (shot.mood > byNumber[shot.scene].mood) byNumber[shot.scene].mood = shot.mood;
    });
    if (scenes.length) {
      scenes[0].start = 0;                   // the title card belongs to scene one
      scenes[scenes.length - 1].end = reel.duration;   // and the end card to the last
    }
    return scenes;
  }

  function sectionPlan(reel, bpm) {
    var barSeconds = (BEATS_PER_BAR * 60) / bpm;
    var scenes = scenesOf(reel);

    var plan = scenes.map(function (scene, index) {
      var blocks = Math.max(1, Math.round((scene.end - scene.start) / (barSeconds * BLOCK_BARS)));
      var energy = Math.max(0, Math.min(1, scene.mood));
      var isFinal = index === scenes.length - 1;
      return {
        type: SECTION_TYPE[scene.beat] || 'verse',
        bars: blocks * BLOCK_BARS,
        energy: energy,
        scene: scene.scene,
        last: isFinal,
        parts: partsFor(energy, isFinal)
      };
    });

    // Rounding can leave the music a block short. It may run long; it may never
    // run out before the picture does.
    var totalSeconds = function () {
      return plan.reduce(function (bars, s) { return bars + s.bars; }, 0) * barSeconds;
    };
    var guard = 0;
    while (plan.length && totalSeconds() < reel.duration && guard++ < 200) {
      plan[plan.length - 1].bars += BLOCK_BARS;
    }
    return plan;
  }
```

Export `SECTION_TYPE`, `sectionPlan` and `scenesOf` on `API`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS on both new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Turn each scene into a section of the score"
```

---

### Task 4: Instruments follow the tension

**Files:**
- Modify: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `sectionPlan` (Task 3).
- Produces: `FilmConductor.partsFor(energy, isFinal)` → `{ drums, bass, chords, arp, lead, pad }` of booleans. `sectionPlan` now sets `section.parts` on every entry.

- [ ] **Step 1: Write the failing test**

```js
test('the band grows with the tension and stands down at the end', () => {
  const quiet = Conductor.partsFor(0.15, false);
  eq(quiet.drums, false, 'drums under the opening');
  eq(quiet.bass, false, 'bass under the opening');
  eq(quiet.pad && quiet.chords, true, 'the opening still needs pad and chords');

  eq(Conductor.partsFor(0.4, false).bass, true, 'bass joins in the middle band');
  eq(Conductor.partsFor(0.4, false).drums, false, 'drums are too early at 0.4');
  eq(Conductor.partsFor(0.6, false).drums, true, 'drums join by 0.6');
  eq(Conductor.partsFor(0.6, false).lead, false, 'the lead is not out yet at 0.6');

  const crisis = Conductor.partsFor(0.88, false);
  eq(crisis.drums && crisis.bass && crisis.lead && crisis.arp, true, 'the crisis gets the full band');

  // The last scene resolves regardless of its own tension — a micro film ends
  // on the choice at 0.5 and must still land rather than stop.
  const ending = Conductor.partsFor(0.5, true);
  eq(ending.drums, false, 'the closing scene still had drums');
  eq(ending.pad && ending.chords, true, 'the closing scene needs pad and chords');
});

test('every section carries its instruments', () => {
  const reel = Reel.build(sample);
  const plan = Conductor.sectionPlan(reel, Conductor.chooseBpm(reel, [70, 110]));
  plan.forEach((section) => {
    ['drums', 'bass', 'chords', 'arp', 'lead', 'pad'].forEach((part) => {
      eq(typeof section.parts[part], 'boolean', 'section is missing ' + part);
    });
  });
  eq(plan[plan.length - 1].parts.drums, false, 'the film ends on drums');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Conductor.partsFor is not a function`

- [ ] **Step 3: Implement**

```js
  /* The difference between a song playing under a film and a score: the band
   * arrives as the story tightens, and stands down for the ending. */
  function partsFor(energy, isFinal) {
    if (isFinal) {
      return { drums: false, bass: false, chords: true, arp: false, lead: false, pad: true };
    }
    return {
      pad: true,
      chords: true,
      bass: energy >= 0.3,
      drums: energy >= 0.5,
      arp: energy > 0.7,
      lead: energy > 0.7
    };
  }
```

`sectionPlan` (Task 3) already calls `partsFor(energy, isFinal)` when it builds
each entry, so nothing there changes. Export `partsFor`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS on both new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Bring the band in as the story tightens, and stand it down to end"
```

---

### Task 5: The whole score request

**Files:**
- Modify: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: everything above.
- Produces: `FilmConductor.request(reel, opts)` → `{ genre, mood, seed, seconds, bpm, sections }`. `opts.bpmRange` is `[low, high]` and defaults to `[72, 108]`.

- [ ] **Step 1: Write the failing test**

```js
test('a reel becomes a complete score request', () => {
  const reel = Reel.build(sample);
  const forge = loadSongForge();
  const music = Conductor.MUSIC_FOR[reel.genre];
  const req = Conductor.request(reel, { bpmRange: forge.Genres.GENRES[music.genre].bpm });

  eq(req.genre, music.genre);
  eq(req.mood, music.mood);
  eq(req.seconds, reel.duration);
  assert(req.sections.length >= 1, 'a request with no sections');
  assert(req.seed !== reel.seed, 'the score seed must not be the film seed itself');
  assert(typeof req.seed === 'number' && isFinite(req.seed), 'bad seed: ' + req.seed);

  const again = Conductor.request(reel, { bpmRange: forge.Genres.GENRES[music.genre].bpm });
  eq(JSON.stringify(again), JSON.stringify(req), 'the same film must ask for the same score');
});

test('any film, any length, produces a usable request', () => {
  ['', 'robot', 'two sisters rob a bank at midnight', 'a ghost in the attic'].forEach((idea) => {
    ['micro', 'short', 'festival'].forEach((length) => {
      const reel = Reel.build(Writer.write(Parse.parse(idea, { seed: 3 }), { length, seed: 3 }));
      const req = Conductor.request(reel);
      assert(req.bpm > 0 && req.sections.length > 0, 'unusable request for "' + idea.slice(0, 20) + '"');
      const seconds = (req.sections.reduce((b, s) => b + s.bars, 0) * Conductor.BEATS_PER_BAR * 60) / req.bpm;
      assert(seconds >= reel.duration - 1e-6, 'the score is shorter than the film');
    });
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Conductor.request is not a function`

- [ ] **Step 3: Implement**

```js
  var DEFAULT_BPM_RANGE = [72, 108];

  function request(reel, opts) {
    opts = opts || {};
    var music = MUSIC_FOR[reel.genre] || MUSIC_FOR.drama;
    var bpm = chooseBpm(reel, opts.bpmRange || DEFAULT_BPM_RANGE);
    return {
      genre: music.genre,
      mood: music.mood,
      // A film and its score share a lineage without sharing a number, so the
      // music is stable per film but is not the same draw as the picture.
      seed: (reel.seed ^ 0x5f356495) >>> 0,
      seconds: reel.duration,
      bpm: bpm,
      sections: sectionPlan(reel, bpm)
    };
  }
```

Export `request` and `DEFAULT_BPM_RANGE`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS on both new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Turn a reel into one complete score request"
```

---

### Task 6: Ducking under the dialogue

**Files:**
- Modify: `film/js/film-score.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: the reel's shots.
- Produces: `FilmConductor.duckEnvelope(reel)` → `[{ t, gain }]`, sorted by `t`, starting at `t: 0, gain: 1`, with `gain` either `1` or `FilmConductor.DUCK_GAIN`.

- [ ] **Step 1: Write the failing test**

```js
test('the music ducks for every line and comes back up', () => {
  const reel = Reel.build(sample);
  const env = Conductor.duckEnvelope(reel);
  const lines = reel.shots.filter((s) => s.kind === 'line');

  eq(env[0].t, 0, 'the envelope must start at the top of the film');
  eq(env[0].gain, 1, 'the film must start at full music');

  for (let i = 1; i < env.length; i++) {
    assert(env[i].t >= env[i - 1].t, 'envelope points are out of order');
    assert(env[i].gain === 1 || env[i].gain === Conductor.DUCK_GAIN,
      'unexpected gain ' + env[i].gain);
    assert(env[i].t >= 0 && env[i].t <= reel.duration + 1, 'envelope point outside the film');
  }

  const ducks = env.filter((p) => p.gain === Conductor.DUCK_GAIN).length;
  assert(ducks >= 1 && ducks <= lines.length,
    `${ducks} ducks for ${lines.length} lines — expected at most one per line`);
  eq(env[env.length - 1].gain, 1, 'the music must come back up before the end');
});

test('lines close together stay ducked rather than pumping', () => {
  const reel = {
    duration: 20, genre: 'drama', seed: 1,
    shots: [
      { kind: 'line', start: 5, duration: 2, scene: 1 },
      { kind: 'line', start: 7.1, duration: 2, scene: 1 }
    ]
  };
  const env = Conductor.duckEnvelope(reel);
  const ducks = env.filter((p) => p.gain === Conductor.DUCK_GAIN).length;
  eq(ducks, 1, 'two lines a fifth of a second apart should be one duck, not two');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Conductor.duckEnvelope is not a function`

- [ ] **Step 3: Implement**

```js
  var DUCK_GAIN = 0.35;    // how far the music drops under a line
  var DUCK_LEAD = 0.25;    // seconds before the line it starts dropping
  var DUCK_TAIL = 0.20;    // seconds after the line before it comes back

  /* The reel knows exactly when every line is spoken, so the ducking is
   * arithmetic rather than a side-chain: a sorted list of level changes. */
  function duckEnvelope(reel) {
    var spans = [];
    reel.shots.forEach(function (shot) {
      if (shot.kind !== 'line') return;
      spans.push({
        from: Math.max(0, shot.start - DUCK_LEAD),
        to: shot.start + shot.duration + DUCK_TAIL
      });
    });
    spans.sort(function (a, b) { return a.from - b.from; });

    // Lines that run into each other stay down rather than pumping between them.
    var merged = [];
    spans.forEach(function (span) {
      var last = merged[merged.length - 1];
      if (last && span.from <= last.to) {
        if (span.to > last.to) last.to = span.to;
      } else {
        merged.push({ from: span.from, to: span.to });
      }
    });

    var points = [{ t: 0, gain: 1 }];
    merged.forEach(function (span) {
      points.push({ t: span.from, gain: DUCK_GAIN });
      points.push({ t: span.to, gain: 1 });
    });
    return points;
  }
```

Export `duckEnvelope`, `DUCK_GAIN`, `DUCK_LEAD`, `DUCK_TAIL`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS on both new tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-score.js film/tests/film-logic.test.js
git commit -m "Duck the music under every spoken line"
```

---

### Task 7: SONG FORGE composes to a supplied plan

**Files:**
- Modify: `music/js/composer.js` (`compose()` around line 638, `planStructure()` line 61, `assignParts()` line 615)
- Test: `music/tests/music-logic.test.js`

**Interfaces:**
- Consumes: `FilmConductor.request()` output (Task 5) as `compose()` input.
- Produces: `compose(opts)` additionally accepting `opts.seconds` (number), `opts.sections` (`[{type, bars, energy, parts}]`) and `opts.bpm` (already supported). With none of them, output is byte-identical to today's for a given seed.

- [ ] **Step 1: Write the failing test**

Append to `music/tests/music-logic.test.js`, before its final report block. That
suite has no `test()` wrapper — it is a flat list of `check(cond, msg)` calls
against `const { Genres, Composer, Exporter } = sandbox;`. Match it:

```js
/* --- scoring to picture: an exact length and a supplied plan --- */
const plan = [
  { type: 'intro',  bars: 8,  energy: 0.15, parts: { pad: true, chords: true, bass: false, drums: false, arp: false, lead: false } },
  { type: 'chorus', bars: 16, energy: 0.9,  parts: { pad: true, chords: true, bass: true,  drums: true,  arp: true,  lead: true } },
  { type: 'outro',  bars: 8,  energy: 0.2,  parts: { pad: true, chords: true, bass: false, drums: false, arp: false, lead: false } }
];
const planned = Composer.compose({ genre: 'cinematic', mood: 'dark', seed: 'plan-7', bpm: 90, sections: plan });
check(planned.sections.length === 3, 'a supplied plan must be used as given (got ' + planned.sections.length + ' sections)');
check(planned.bars === 32, 'bar count must come from the plan (got ' + planned.bars + ')');
check(planned.sections[0].parts.drums === false, 'the supplied intro must stay drumless');
check(planned.sections[1].parts.lead === true, 'the supplied chorus must keep its lead');
check(!!planned.tracks.drums, 'a planned song still needs a drum track object');

const timed = Composer.compose({ genre: 'cinematic', mood: 'chill', seed: 'timed-11', seconds: 150 });
check(timed.duration >= 150, 'a song asked for 150s came back at ' + timed.duration.toFixed(1) + 's');
check(timed.duration < 170, 'a song asked for 150s overshot to ' + timed.duration.toFixed(1) + 's');

const before = Composer.compose({ genre: 'lofi', mood: 'chill', length: 'short', seed: 'unchanged-99' });
const after  = Composer.compose({ genre: 'lofi', mood: 'chill', length: 'short', seed: 'unchanged-99' });
check(JSON.stringify(after.sections) === JSON.stringify(before.sections),
  'composing without the new options must be unchanged');
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node music/tests/music-logic.test.js`
Expected: FAIL — the supplied plan is ignored, `song.sections.length` is not 3.

- [ ] **Step 3: Implement**

In `music/js/composer.js`, inside `compose(opts)`, replace the length and structure block:

```js
    // Length → bar count, rounded to whole 4-bar blocks.
    const targetSec = LENGTHS[opts.length] || LENGTHS.medium;
```

with:

```js
    // Length → bar count, rounded to whole 4-bar blocks. `opts.seconds` scores
    // to picture: it asks for an exact duration and never comes back short.
    const targetSec = opts.seconds > 0 ? opts.seconds : (LENGTHS[opts.length] || LENGTHS.medium);
```

then, for the bar count, round **up** when scoring to picture:

```js
    let bars = opts.seconds > 0
      ? Math.ceil(targetSec * barsPerSec / 4) * 4
      : Math.round(targetSec * barsPerSec / 4) * 4;
    bars = Math.max(24, Math.min(112, bars));
```

and replace the structure and parts assignment:

```js
    song.sections = planStructure(rng, bars);
```

with:

```js
    // A supplied plan is used as given — that is how a film scores to its own
    // cuts instead of to a pop-song pattern.
    song.sections = opts.sections && opts.sections.length
      ? opts.sections.map(function (s) {
          return { type: s.type, bars: s.bars, energy: s.energy, parts: s.parts || null };
        })
      : planStructure(rng, bars);
```

and in `assignParts(song, rng)`, skip any section that already has its parts:

```js
  function assignParts(song, rng) {
    const genre = song.genre;
    for (let i = 0; i < song.sections.length; i++) {
      const sec = song.sections[i];
      if (sec.parts) continue;          // supplied by the caller
      const e = sec.energy;
```

- [ ] **Step 4: Run both suites**

Run: `node music/tests/music-logic.test.js`
Expected: PASS, including the three new tests and every pre-existing one.

Run: `node film/tests/film-logic.test.js`
Expected: PASS — unaffected.

- [ ] **Step 5: Commit**

```bash
git add music/js/composer.js music/tests/music-logic.test.js
git commit -m "Let a caller compose to an exact length and a supplied plan"
```

---

### Task 8: SONG FORGE plays somewhere other than the speakers

**Files:**
- Modify: `music/js/engine.js` (`buildGraph` line 69, `Player` constructor line ~190, `ensureContext` line 206)
- Test: `music/tests/music-logic.test.js` is Node-only and cannot build an audio graph, so this task is covered by the browser test in Task 11. Verify by hand as below.

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `buildGraph(ctx, song, mix, withAnalyser, destination)` — connects to `destination` when given, otherwise `ctx.destination` exactly as before.
  - `new Engine.Player(opts)` where `opts` may be `{ context, destination }`; both optional. `player.ctx` is the supplied context when one is given.

- [ ] **Step 1: Make the output destination optional**

In `music/js/engine.js`, change the signature and the one hard-wired line:

```js
  function buildGraph(ctx, song, mix, withAnalyser, destination) {
```

```js
    // Default to the speakers; a caller scoring a film passes its own bus so
    // the music reaches the recorder with everything else.
    out.connect(destination || ctx.destination);
```

- [ ] **Step 2: Let a Player run in someone else's context**

In the `Player` constructor, accept options:

```js
  function Player(opts) {
    opts = opts || {};
    this.ctx = opts.context || null;
    this.destination = opts.destination || null;
```

(keep every other field as it is), and pass the destination through in `_buildGraph`:

```js
  Player.prototype._buildGraph = function () {
    this.graph = buildGraph(this.ctx, this.song, this.mix, true, this.destination);
```

`ensureContext()` needs no change: it already only creates a context `if (!this.ctx)`.

- [ ] **Step 3: Check SONG FORGE itself still works**

Run: `node music/tests/music-logic.test.js`
Expected: PASS.

Run: `npm --prefix music/tests install` (once), then `node music/tests/music-browser.test.js`
Expected: PASS — SONG FORGE still plays, exports WAV and MIDI, with no caller passing the new arguments.

- [ ] **Step 4: Commit**

```bash
git add music/js/engine.js
git commit -m "Let a song play into a supplied audio context and destination"
```

---

### Task 9: The film's two buses

**Files:**
- Modify: `film/js/film-audio.js` (remove the pad/chord bed; add the music and effects buses)
- Modify: `film/index.html` (load `../music/js/*` and `js/film-score.js` before `js/film-audio.js`)
- Test: covered by Task 11's browser test; `node film/tests/film-logic.test.js` must keep passing.

**Interfaces:**
- Consumes: `FilmConductor.request()`, `FilmConductor.duckEnvelope()` (Tasks 5–6); `Composer.compose()`, `Engine.Player` (Tasks 7–8).
- Produces: `Score` (in `film-audio.js`) gains `musicBus` and `effectsBus`, plus `startScore(reel)`, `scoreTransport()` and `usingRealScore` (boolean, false when it fell back).

- [ ] **Step 1: Load the music modules in the film page**

In `film/index.html`, before `<script src="js/film-audio.js">`:

```html
  <!-- SONG FORGE, used as a library: it scores the film -->
  <script src="../music/js/theory.js"></script>
  <script src="../music/js/genres.js"></script>
  <script src="../music/js/synth.js"></script>
  <script src="../music/js/composer.js"></script>
  <script src="../music/js/engine.js"></script>
  <script src="js/film-score.js"></script>
```

Run: `node scripts/check-links.mjs`
Expected: `✓ everything is connected` — it verifies these paths resolve.

- [ ] **Step 2: Split the buses and delete the bed**

In `film/js/film-audio.js`, add a module constant beside the existing ones —
the level the score sits at against the voices, named once because both the bus
and the ducking need it:

```js
  var MUSIC_LEVEL = 0.55;   // where the score sits under the dialogue
```

Then in the `Score` constructor, replace the pad creation (`this.padGain`, `this.filter`, `this.oscs`) with two buses:

```js
    // Two buses: the score, and everything the film makes itself. Both feed the
    // master, which already reaches the speakers and the recorder.
    this.musicBus = this.ctx.createGain();
    this.musicBus.gain.value = MUSIC_LEVEL;
    this.musicBus.connect(this.master);

    this.effectsBus = this.ctx.createGain();
    this.effectsBus.gain.value = 1;
    this.effectsBus.connect(this.master);

    this.player = null;          // SONG FORGE's Player, once a score exists
    this.usingRealScore = false;
```

Then change every existing `gain.connect(this.master)` in `hit`, `blip`, `noiseBurst` and `tick` to `gain.connect(this.effectsBus)`, and drop the `hit` strength by the factor the spec asks for:

```js
    gain.gain.exponentialRampToValueAtTime(0.18 * strength, now + 0.02);
```

Delete `Score.prototype.start`'s oscillator loop and the `padGain` ramps, but
**keep `this.started = true` and the `if (this.started) return;` guard** —
`enterShot()` and `tick()` both test that flag, so dropping it silently kills
the cut hits and the character voices:

```js
  Score.prototype.start = function () {
    if (this.started) return;
    this.started = true;
    if (this.ctx.state === 'suspended' && this.ctx.resume) this.ctx.resume();
  };
```

- [ ] **Step 3: Compose and start the score**

Add to `film-audio.js`:

```js
  /* Ask SONG FORGE for a score for this film and start it playing into the
   * music bus. Returns true if a real score is playing, false if the film is
   * carrying on without one. */
  Score.prototype.startScore = function (reel) {
    var Forge = root.Composer, Play = root.Engine, Conductor = root.FilmScore;
    if (!Forge || !Play || !Conductor) return false;

    try {
      var genreRange = root.Genres && root.Genres.GENRES[Conductor.MUSIC_FOR[reel.genre].genre];
      var req = Conductor.request(reel, { bpmRange: genreRange && genreRange.bpm });
      var song = Forge.compose({
        genre: req.genre, mood: req.mood, seed: req.seed,
        bpm: req.bpm, seconds: req.seconds, sections: req.sections
      });

      this.player = new Play.Player({ context: this.ctx, destination: this.musicBus });
      this.player.loop = false;
      this.player.load(song);
      this.duckPoints = Conductor.duckEnvelope(reel);
      this.usingRealScore = true;
      return true;
    } catch (e) {
      this.player = null;
      this.usingRealScore = false;
      return false;
    }
  };

  /* The duck envelope is a list of level changes in *film* time. Scheduling is
   * in audio-context time, so it is laid down relative to where playback is
   * starting from — and re-laid every time the film plays or is scrubbed,
   * otherwise a scrub leaves the ducking pointing at the wrong moments. */
  Score.prototype.applyDuck = function (fromFilmSeconds) {
    if (!this.duckPoints) return;
    var bus = this.musicBus.gain;
    var base = MUSIC_LEVEL;
    var now = this.ctx.currentTime;
    var offset = fromFilmSeconds || 0;

    bus.cancelScheduledValues(now);
    // Start at whatever the level should be at this moment in the film.
    var current = base;
    this.duckPoints.forEach(function (point) {
      if (point.t <= offset) current = base * point.gain;
    });
    bus.setValueAtTime(current, now);

    this.duckPoints.forEach(function (point) {
      if (point.t <= offset) return;
      bus.linearRampToValueAtTime(base * point.gain, now + (point.t - offset));
    });
  };
```

- [ ] **Step 4: Check nothing regressed**

Run: `node film/tests/film-logic.test.js`
Expected: PASS (unchanged count).

- [ ] **Step 5: Commit**

```bash
git add film/index.html film/js/film-audio.js
git commit -m "Give the film a music bus and an effects bus, and delete the chord bed"
```

---

### Task 10: The score follows the film

**Files:**
- Modify: `film/js/film-player.js` (`Player.prototype.play`, `pause`, `seek`, `stop`)
- Modify: `film/js/app.js` (the fallback note)
- Test: Task 11.

**Interfaces:**
- Consumes: `Score.startScore`, `Score.player`, `Score.usingRealScore` (Task 9).
- Produces: film transport calls drive the music transport. `beat = seconds × bpm / 60`.

- [ ] **Step 1: Convert film time to musical time**

Add to `film-player.js`:

```js
  /* The score is a live player, so the film drives it the way a projectionist
   * drives sound: same clock, same transport. */
  function beatAt(score, seconds) {
    if (!score || !score.player || !score.player.song) return 0;
    return (seconds * score.player.song.bpm) / 60;
  }
```

- [ ] **Step 2: Drive it from every transport call**

In `Player.prototype.play`, after `if (this.score) this.score.start();`:

```js
    if (this.score && this.score.player) {
      this.score.player.play(beatAt(this.score, this._offset));
      this.score.applyDuck(this._offset);
    }
```

In `Player.prototype.pause`, before `if (this.score) this.score.stop();`:

```js
    if (this.score && this.score.player) this.score.player.pause();
```

In `Player.prototype.stop`, alongside the existing score stop:

```js
    if (this.score && this.score.player) this.score.player.stop();
```

In `Player.prototype.seek`, after `this.time = target;`:

```js
    if (this.score && this.score.player) {
      this.score.player.seek(beatAt(this.score, target));
      if (this.playing) this.score.applyDuck(target);
    }
```

- [ ] **Step 3: Start the score when the player is made, and say so when it fails**

In `film/js/app.js`, inside `makePlayer()` after the `Score` is constructed:

```js
      if (score) {
        var scored = score.startScore(reel);
        // The panel carries what it has: which kind of score, and what the
        // music is doing. The film note and the tests both read it.
        el.viewFilm.dataset.score = scored ? 'real' : 'fallback';
        el.viewFilm.dataset.sections = scored && score.player && score.player.song
          ? String(score.player.song.sections.length) : '0';
        if (!scored) say('Could not compose a score in this browser — using simple music.');
      }
```

and set the music's state from the same hooks that already drive the film's,
in the `hooks` object passed to `PlayerLib.Player`:

```js
      onPlay: function () { el.viewFilm.dataset.music = 'playing'; },
      onPause: function () { el.viewFilm.dataset.music = 'paused'; ... },
      onStop: function () { el.viewFilm.dataset.music = 'stopped'; ... },
```

(merge these lines into the existing `onPause`/`onStop` handlers rather than
replacing them, and add `onPlay` — `film-player.js` already calls
`hooks.onPlay()` when playback starts.)

- [ ] **Step 4: Check nothing regressed**

Run: `node film/tests/film-logic.test.js`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add film/js/film-player.js film/js/app.js
git commit -m "Make the score follow the film's play, pause, scrub and stop"
```

---

### Task 11: Prove it in a browser

**Files:**
- Modify: `film/tests/film-browser.test.js` (extend the `THE FILM` section)

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Write the failing checks**

In `film/tests/film-browser.test.js`, after the existing `stop returns to the start` check:

```js
    console.log('\nTHE SCORE');
    const scoreState = () => page.evaluate(() => {
      const panel = document.getElementById('viewFilm');
      return { score: panel.dataset.score || '', music: panel.dataset.music || '',
               sections: parseInt(panel.dataset.sections || '0', 10) };
    });

    await page.click('#playFilm');
    await page.waitForTimeout(2500);
    const playing = await scoreState();
    check(playing.score === 'real', `a real composed score is playing, not the fallback (${playing.score})`);
    check(playing.music === 'playing', 'the music is running while the picture runs');
    check(playing.sections >= 3, `the score has a section per scene (${playing.sections})`);

    await page.click('#playFilm'); // pause
    await page.waitForTimeout(300);
    check((await scoreState()).music === 'paused', 'pausing the film pauses the music');

    await page.click('#stopFilm');
    await page.waitForTimeout(300);
    check((await scoreState()).music === 'stopped', 'stopping the film stops the music');

    // A page where SONG FORGE is missing: the film must still play, and say why.
    const bare = await context.newPage();
    await bare.addInitScript(() => {
      // Take the composer away before the app ever looks for it.
      Object.defineProperty(window, 'Composer', { get: () => undefined, set: () => {} });
    });
    await bare.goto(base + '/film/', { waitUntil: 'load' });
    await bare.fill('#idea', 'a kid and a walkie-talkie in the attic');
    await bare.click('#write');
    await bare.waitForTimeout(400);
    await bare.click('#tabFilm');
    await bare.click('#playFilm');
    await bare.waitForTimeout(1500);

    const withoutForge = await bare.evaluate(() => ({
      music: document.getElementById('viewFilm').dataset.music || '',
      score: document.getElementById('viewFilm').dataset.score || '',
      clock: document.getElementById('filmClock').textContent,
      status: document.getElementById('status').textContent
    }));
    check(/0:0[1-9]|0:[1-9]/.test(withoutForge.clock),
      `the film still plays with SONG FORGE missing (clock ${withoutForge.clock})`);
    check(withoutForge.score === 'fallback', 'it knows it is not using a real score');
    check(/simple music/i.test(withoutForge.status),
      `it says so plainly (status: "${withoutForge.status}")`);
    await bare.close();
```

> The checks read `#viewFilm`'s `data-score` and `data-music` attributes, which
> Task 10 sets. They are ordinary UI state — the panel knows whether it has a
> real score and whether the music is running — not test-only globals, so
> nothing here exists purely for the tests.

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-browser.test.js`
Expected: FAIL — `a real composed score is playing` (the seam is missing).

- [ ] **Step 3: Confirm the state attributes are being published**

Nothing to add — Task 10 already publishes this state on the film panel as
`data-score` and `data-music`. If the checks fail here, the bug is in Task 10's
attribute updates, not in a missing seam.

- [ ] **Step 4: Run the full browser suite**

Run: `node film/tests/film-browser.test.js`
Expected: PASS, every check including the pre-existing recording ones.

- [ ] **Step 5: Commit**

```bash
git add film/js/app.js film/tests/film-browser.test.js
git commit -m "Prove in a browser that the score plays, follows the film and survives failure"
```

---

### Task 12: Say what it now does

**Files:**
- Modify: `film/README.md`, `README.md`, `shared/projects.js`

- [ ] **Step 1: Update the film README**

In `film/README.md`, replace the sound bullet under **The film**:

```markdown
- **The sound** is a real score. SONG FORGE composes a song for this film
  specifically — to its exact length, with its sections turning over on your
  scene cuts and its instruments following the story: pad and chords under the
  opening, bass as it builds, drums through the middle, the full band only at
  the crisis, and back down to pad to end. It plays live, ducking under every
  line of dialogue, and the character voices and cut hits sit over the top.
```

- [ ] **Step 2: Update the hub blurb**

In `shared/projects.js`, in the `film` entry, change `badges` to include `'Scored by SONG FORGE'` in place of `'Zero deps'`, and extend `blurb` with `'…scored by SONG FORGE, and downloadable as a video file.'`

- [ ] **Step 3: Run everything**

```bash
node scripts/check-links.mjs
node film/tests/film-logic.test.js
node music/tests/music-logic.test.js
node film/tests/film-browser.test.js
node scripts/smoke-site.cjs
```
Expected: all pass.

- [ ] **Step 4: Commit**

```bash
git add film/README.md README.md shared/projects.js
git commit -m "Document the score"
```

---

## Self-review notes

- **Spec coverage:** genre/mood map → Task 1; tempo choice → Task 2; sections and types → Task 3; energy and instruments, including the final-scene resolve → Task 4; determinism and the full request → Task 5; ducking → Task 6; SONG FORGE's optional `seconds`/`sections`/`parts` → Task 7; its optional destination and external context → Task 8; the two buses and the deleted chord bed → Task 9; transport following and the fallback note → Task 10; browser proof including fallback → Task 11; docs → Task 12.
- **Not covered by tests, by choice:** whether the recorded file *sounds* right — whether 0.55 is the correct music level against the voices, and whether ducking to 0.35 is enough. The browser tests prove the music runs, follows the transport, reaches the graph the recorder captures, and degrades honestly; judging the balance is a listening job on the first real film, and the two numbers are one-line changes when it is judged.
- **Watch for during implementation:** Task 9 changes where the existing hits and blips connect (`this.master` → `this.effectsBus`). Miss one and that sound silently keeps bypassing the duck. Grep `film/js/film-audio.js` for `connect(this.master)` after the change; only `musicBus` and `effectsBus` should remain.
