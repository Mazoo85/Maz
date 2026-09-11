# Better Stories Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SCRIPT FORGE films stop feeling like each other — more locations, more than one beat order per length, a crisis in the default film, and 45 MADLIBS premises instead of one sentence.

**Architecture:** All three fixes live in the story layer — `film/js/parse.js`, `film/js/screenplay.js`, `film/js/lexicon.js` and one new `film/js/story-seed.js`. **Nothing in the renderer, the reel or the score changes.** Beat ids stay exactly as they are, so the picture and the music follow the new structures for free. The MADLIBS adapter feeds a generated logline through the existing parser rather than building a second premise constructor.

**Tech Stack:** ES5 browser JavaScript in IIFEs, Node for logic tests, Playwright + Chromium for browser tests. No dependencies are added.

**Spec:** `docs/superpowers/specs/2026-09-11-better-stories-design.md`

## Global Constraints

- **No new dependencies.** Everything runs offline from a static file server.
- **House module style:** each file is an IIFE ending `if (typeof module === 'object' && module.exports) module.exports = API; root.Name = API;`, loaded by a plain `<script>` tag. `film/js/*` is **ES5**: `var`, `function`, no arrow functions, no `const`/`let`. Node test files are exempt and use `const`/arrows.
- **Globals taken:** `FilmParse`, `FilmWriter`, `FilmReel`, `FilmArt`, `FilmSets`, `FilmFigures`, `FilmWeather`, `FilmPlayer`, `FilmScore` (audio), `FilmConductor`. The new module binds `window.FilmStorySeed`.
- **`madlibs/` is not modified by this plan.** It keeps working standalone and keeps its own tests. The dependency runs one way: film → madlibs.
- **The renderer is not modified by this plan.** Not `film-player.js`, `film-figures.js`, `film-sets.js`, `film-weather.js`, `film-art.js`, `film-reel.js`. If a change seems to need renderer work, stop and report — the design is wrong.
- **Beat ids are fixed:** `open spark push turn crisis choice after`. Only their *orders* become plural.
- **Determinism:** the same idea and seed must produce the same film every time. Seeded RNG only, never `Math.random()` or the clock.
- **Tests are `node <file>`** with the home-grown harness (`test`, `assert`, `eq`) in `film/tests/film-logic.test.js`; **its final report block must stay last in the file**. The suite currently has **95** tests.
- **Commit after every task** with a message describing the behaviour, not the file list.

---

### Task 1: A film gets three to five places

**Files:**
- Modify: `film/js/parse.js` (the `/* where */` block, around line 342)
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `LEX.PLACES`, `LEX.ROLES`.
- Produces: `premise.places` is now an array of **3 to 5** entries, same shape as today — `{ key, slug, int, word }`.

Today the fill loop stops at two:

```js
while (placeKeys.length < 2 && guardPlaces++ < 40) { … }
var places = placeKeys.slice(0, 3);
```

Measured across 200 films: 200 offered exactly two.

- [ ] **Step 1: Write the failing test**

Add to `film/tests/film-logic.test.js`, before the final report block:

```js
console.log('\nWHERE A FILM HAPPENS');

test('a premise offers three to five places', () => {
  for (let seed = 0; seed < 60; seed++) {
    const p = Parse.parse('a courier takes a job in a city at night', { seed });
    assert(p.places.length >= 3 && p.places.length <= 5,
      'seed ' + seed + ' offered ' + p.places.length + ' places');
  }
});

test('the places are distinct', () => {
  for (let seed = 0; seed < 60; seed++) {
    const p = Parse.parse('a lighthouse keeper finds a radio', { seed });
    const keys = p.places.map((x) => x.key);
    eq(new Set(keys).size, keys.length, 'seed ' + seed + ' repeated a place: ' + keys.join(','));
  }
});

test('a place named in the idea is still used, and comes first', () => {
  const p = Parse.parse('two sisters argue in a kitchen');
  eq(p.places[0].key, 'kitchen', 'the typed place did not lead');
});

test('the same idea and seed give the same places', () => {
  for (let seed = 0; seed < 20; seed++) {
    const a = Parse.parse('a thief in a warehouse', { seed }).places.map((x) => x.key).join(',');
    const b = Parse.parse('a thief in a warehouse', { seed }).places.map((x) => x.key).join(',');
    eq(a, b, 'seed ' + seed + ' was not deterministic');
  }
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — "offered 2 places".

- [ ] **Step 3: Implement**

Replace the `/* where */` block's fill loop and slice:

```js
    /* where — three to five locations, so a film has somewhere to go. Places
     * the idea named lead; then the hero's own workplace; then connectors that
     * plausibly adjoin anywhere, so a lighthouse story does not cut to a
     * hospital for no reason. */
    var placeKeys = findAll(hay, Object.keys(LEX.PLACES));
    if (heroRole && LEX.ROLES[heroRole].place &&
        placeKeys.indexOf(LEX.ROLES[heroRole].place) === -1) {
      placeKeys.push(LEX.ROLES[heroRole].place);
    }
    var CONNECTORS = ['car', 'street', 'porch', 'hallway', 'parking lot', 'stairwell', 'alley', 'kitchen'];
    var wanted = 3 + Math.floor(rng() * 3);          // 3, 4 or 5
    var guardPlaces = 0;
    while (placeKeys.length < wanted && guardPlaces++ < 60) {
      var candidate = pick(CONNECTORS, rng);
      if (placeKeys.indexOf(candidate) === -1) placeKeys.push(candidate);
    }
    var places = placeKeys.slice(0, 5).map(function (k) {
      var entry = LEX.PLACES[k] || { slug: k.toUpperCase(), int: 'INT.' };
      return { key: k, slug: entry.slug, int: entry.int, word: entry.slug.split(' — ')[0] };
    });
```

Note the connector pool has 8 entries, so 5 is always reachable.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS, 99 tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/parse.js film/tests/film-logic.test.js
git commit -m "Give a film three to five places instead of two"
```

---

### Task 2: The beat decides where it happens

**Files:**
- Modify: `film/js/screenplay.js` (`BEAT_PLACE` at line 94, its use at line 151)
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `premise.places` (Task 1).
- Produces: `Writer.placeForBeat(beatId, placeCount, seed)` → an integer index into `premise.places`. Exported so it can be tested directly.

`BEAT_PLACE` is a hardcoded table naming only indices 0 and 1, so extra places would never be reached.

- [ ] **Step 1: Write the failing test**

```js
test('a festival film uses at least three distinct places', () => {
  for (let seed = 0; seed < 40; seed++) {
    const script = Writer.write(Parse.parse('a courier takes a job', { seed }), { length: 'festival', seed });
    const used = new Set(script.scenes.map((s) => s.heading.place.key));
    assert(used.size >= 3, 'seed ' + seed + ' used only ' + used.size + ' places');
  }
});

test('a film ends where it began', () => {
  for (let seed = 0; seed < 40; seed++) {
    const script = Writer.write(Parse.parse('a lighthouse keeper finds a radio', { seed }), { length: 'festival', seed });
    const byBeat = {};
    script.scenes.forEach((s) => { byBeat[s.beat.id] = s.heading.place.key; });
    if (byBeat.open && byBeat.after) {
      eq(byBeat.after, byBeat.open, 'seed ' + seed + ' did not return to the opening place');
    }
  }
});

test('the crisis happens somewhere the film has not been', () => {
  let elsewhere = 0, total = 0;
  for (let seed = 0; seed < 40; seed++) {
    const script = Writer.write(Parse.parse('a thief in a warehouse', { seed }), { length: 'festival', seed });
    const byBeat = {};
    script.scenes.forEach((s) => { byBeat[s.beat.id] = s.heading.place.key; });
    if (byBeat.crisis && byBeat.open) { total++; if (byBeat.crisis !== byBeat.open) elsewhere++; }
  }
  assert(total > 0, 'no festival film reached a crisis');
  eq(elsewhere, total, 'the crisis shared the opening place in ' + (total - elsewhere) + ' of ' + total);
});

test('placeForBeat stays inside the places it is given', () => {
  ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'].forEach((beat) => {
    for (let count = 1; count <= 5; count++) {
      for (let seed = 0; seed < 20; seed++) {
        const i = Writer.placeForBeat(beat, count, seed);
        assert(Number.isInteger(i) && i >= 0 && i < count,
          beat + ' with ' + count + ' places returned ' + i);
      }
    }
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Writer.placeForBeat is not a function`.

- [ ] **Step 3: Implement**

Replace `BEAT_PLACE` with:

```js
  /* Which of the premise's locations each beat plays in.
   *
   * Two rules carry the feeling and the rest is seeded spread. A film opens and
   * closes in the same place, which is what makes an ending feel like one. And
   * the crisis happens somewhere the film has not been — being somewhere
   * unfamiliar is part of what a crisis is. */
  function placeForBeat(beatId, placeCount, seed) {
    if (placeCount <= 1) return 0;
    var rng = PARSE.makeRng((PARSE.hashText(beatId) ^ (seed >>> 0)) >>> 0);

    if (beatId === 'open' || beatId === 'after') return 0;
    if (beatId === 'crisis') return placeCount - 1;          // the far end, never the opening
    if (beatId === 'choice') return placeCount > 2 ? 1 : 0;  // on the way back

    // spark, push, turn: spread across the middle ground, never the opening.
    var span = Math.max(1, placeCount - 1);
    return 1 + Math.floor(rng() * span) % span;
  }
```

`screenplay.js` already binds `PARSE` at line 21, so `PARSE.makeRng` and
`PARSE.hashText` are available. Note its `API` currently exports **only**
`write` and `paginate` — `placeForBeat` is a new export, not an existing one.

Then at the call site (line 151):

```js
      var heading = headingFor(premise, beatId,
        placeForBeat(beatId, premise.places.length, seed), previousHeading);
```

Export `placeForBeat` on the module's `API`.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS, 103 tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/screenplay.js film/tests/film-logic.test.js
git commit -m "Send each beat somewhere its own, and the crisis somewhere new"
```

---

### Task 3: More than one shape per length

**Files:**
- Modify: `film/js/lexicon.js` (`STRUCTURES`)
- Modify: `film/js/screenplay.js` (where the structure is chosen, around line 124)
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Produces: each `LEX.STRUCTURES[length]` gains `spines`, an array of beat-id arrays. `structure.beats` stays for compatibility and equals `spines[0]`. `Writer.spineFor(length, seed)` → the chosen beat-id array, exported.

**This is the task that fixes the default film having no crisis.**

- [ ] **Step 1: Write the failing test**

```js
console.log('\nTHE SHAPE OF A STORY');

test('every length offers more than one shape', () => {
  Object.keys(LEX.STRUCTURES).forEach((len) => {
    const spines = LEX.STRUCTURES[len].spines;
    assert(Array.isArray(spines) && spines.length >= 2,
      len + ' offers ' + (spines ? spines.length : 0) + ' shapes');
  });
});

test('every shape is made of real beats and has a beginning', () => {
  const known = ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'];
  Object.keys(LEX.STRUCTURES).forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      eq(spine[0], 'open', len + ' shape ' + i + ' does not open on the open beat');
      eq(new Set(spine).size, spine.length, len + ' shape ' + i + ' repeats a beat');
      spine.forEach((b) => assert(known.indexOf(b) !== -1, len + ' shape ' + i + ' has unknown beat ' + b));
    });
  });
});

test('a short film always reaches a crisis', () => {
  ['short', 'festival'].forEach((len) => {
    LEX.STRUCTURES[len].spines.forEach((spine, i) => {
      assert(spine.indexOf('crisis') !== -1,
        len + ' shape ' + i + ' has no crisis: ' + spine.join(' '));
    });
  });
  // and the shipped default really does produce one
  for (let seed = 0; seed < 30; seed++) {
    const script = Writer.write(Parse.parse('a stranger arrives', { seed }), { length: 'short', seed });
    assert(script.scenes.some((s) => s.beat.id === 'crisis'),
      'a default-length film at seed ' + seed + ' had no crisis');
  }
});

test('two films of the same length can be shaped differently', () => {
  const shapes = new Set();
  for (let seed = 0; seed < 40; seed++) shapes.add(Writer.spineFor('festival', seed).join(' '));
  assert(shapes.size >= 2, 'every festival film had the same shape');
});

test('the same seed always gives the same shape', () => {
  for (let seed = 0; seed < 20; seed++) {
    eq(Writer.spineFor('short', seed).join(' '), Writer.spineFor('short', seed).join(' '));
  }
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `spines` undefined.

- [ ] **Step 3: Implement the shapes**

In `film/js/lexicon.js`, give each structure a `spines` array and keep `beats` pointing at the first:

```js
    micro: {
      label: '3 scenes',
      spines: [
        ['open', 'spark', 'choice'],
        ['open', 'crisis', 'after']
      ]
    },
    short: {
      label: '5 scenes',
      spines: [
        ['open', 'spark', 'crisis', 'choice', 'after'],
        ['open', 'push', 'turn', 'crisis', 'choice'],
        ['open', 'spark', 'turn', 'crisis', 'after']
      ]
    },
    festival: {
      label: '7 scenes',
      spines: [
        ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'],
        ['open', 'spark', 'turn', 'push', 'crisis', 'choice', 'after'],
        ['open', 'push', 'spark', 'turn', 'crisis', 'after', 'choice']
      ]
    }
```

Then, after the table is built, keep the old field working:

```js
  /* `beats` was the single shape each length used to have. Keep it pointing at
   * the first shape so anything still reading it sees a valid story. */
  Object.keys(STRUCTURES).forEach(function (key) {
    STRUCTURES[key].beats = STRUCTURES[key].spines[0];
  });
```

- [ ] **Step 4: Choose a shape per film**

In `film/js/screenplay.js`:

```js
  /* Two films of the same length should not be the same shape. */
  function spineFor(lengthKey, seed) {
    var structure = LEX.STRUCTURES[lengthKey] || LEX.STRUCTURES.short;
    var spines = structure.spines || [structure.beats];
    var rng = PARSE.makeRng((PARSE.hashText('spine:' + lengthKey) ^ (seed >>> 0)) >>> 0);
    return spines[Math.floor(rng() * spines.length) % spines.length];
  }
```

and use it where the scene loop reads `structure.beats`:

```js
    var spine = spineFor(lengthKey, seed);
    spine.forEach(function (beatId, index) {
```

Export `spineFor`.

- [ ] **Step 5: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS, 108 logic tests.

```bash
git add film/js/lexicon.js film/js/screenplay.js film/tests/film-logic.test.js
git commit -m "Give every length several shapes, and every short film a crisis"
```

---

### Task 4: The MADLIBS mappings

**Files:**
- Create: `film/js/story-seed.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `madlibs/js/templates.js` and `madlibs/js/generator.js` (unmodified).
- Produces: `FilmStorySeed.GENRE_FOR` — MADLIBS genre → SCRIPT FORGE genre key; `FilmStorySeed.BEAT_FOR` — MADLIBS beat label → SCRIPT FORGE beat id.

MADLIBS genres are `fantasy sci-fi horror mystery romance comedy adventure thriller heist` and more; SCRIPT FORGE's are `drama thriller horror comedy romance scifi mystery fantasy heist western`. **`adventure` has no equivalent and needs a deliberate choice.**

- [ ] **Step 1: Write the failing test**

```js
console.log('\nSTORIES FROM MADLIBS');
const Seed = require(path.join(__dirname, '..', 'js', 'story-seed.js'));
const MADLIBS = require(path.join(__dirname, '..', '..', 'madlibs', 'js', 'generator.js'));
const MAD_TEMPLATES = require(path.join(__dirname, '..', '..', 'madlibs', 'js', 'templates.js'));

test('every MADLIBS genre maps to a genre SCRIPT FORGE actually has', () => {
  const templates = MAD_TEMPLATES.templates || MAD_TEMPLATES;
  const genres = new Set(templates.map((t) => t.genre));
  assert(genres.size >= 5, 'expected several MADLIBS genres, found ' + genres.size);
  genres.forEach((g) => {
    const mapped = Seed.GENRE_FOR[g];
    assert(mapped, 'no mapping for MADLIBS genre "' + g + '"');
    assert(LEX.GENRES[mapped], g + ' maps to "' + mapped + '", which SCRIPT FORGE does not have');
  });
});

test('every MADLIBS beat label maps to a real beat', () => {
  const known = ['open', 'spark', 'push', 'turn', 'crisis', 'choice', 'after'];
  const templates = MAD_TEMPLATES.templates || MAD_TEMPLATES;
  const labels = new Set();
  templates.forEach((t) => (t.beats || []).forEach((b) => labels.add(b.label)));
  labels.forEach((label) => {
    const mapped = Seed.BEAT_FOR[label];
    assert(mapped !== undefined, 'no mapping for MADLIBS beat "' + label + '"');
    if (mapped !== null) assert(known.indexOf(mapped) !== -1, label + ' maps to unknown beat ' + mapped);
  });
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — cannot find `story-seed.js`.

- [ ] **Step 3: Implement**

Create `film/js/story-seed.js` in the house style, binding `root.FilmStorySeed`:

```js
/*
 * SCRIPT FORGE — stories borrowed from MADLIBS.
 * --------------------------------------------
 * MADLIBS, next door in this repo, knows 45 story skeletons across nine
 * genres and fills them with words that stay consistent across the beats. This
 * turns one of those into something SCRIPT FORGE can shoot.
 *
 * MADLIBS is not modified: it is used as a library, one way.
 *
 * Exposed as window.FilmStorySeed (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var MAD = root.MadlibsGenerator ||
    (typeof require !== 'undefined' ? require('../../madlibs/js/generator.js') : {});

  /* MADLIBS has genres SCRIPT FORGE does not. Adventure is the real decision:
   * its stories are journeys with a villain and a confrontation, which shoot
   * closest to fantasy here. */
  var GENRE_FOR = {
    'fantasy': 'fantasy',
    'sci-fi': 'scifi',
    'scifi': 'scifi',
    'horror': 'horror',
    'mystery': 'mystery',
    'romance': 'romance',
    'comedy': 'comedy',
    'adventure': 'fantasy',
    'thriller': 'thriller',
    'heist': 'heist',
    'western': 'western',
    'drama': 'drama'
  };

  /* MADLIBS's six labels against the seven beats a film is cut from. The
   * logline is the premise rather than a scene, so it maps to nothing. */
  var BEAT_FOR = {
    'Logline': null,
    'Setup': 'open',
    'Inciting Incident': 'spark',
    'Conflict': 'push',
    'Climax': 'crisis',
    'Resolution': 'after'
  };

  var API = { GENRE_FOR: GENRE_FOR, BEAT_FOR: BEAT_FOR };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmStorySeed = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
```

If a MADLIBS genre or beat label exists that these tables miss, the test will say so — add it rather than loosening the test.

- [ ] **Step 4: Run it and watch it pass**

Run: `node film/tests/film-logic.test.js`
Expected: PASS, 110 tests.

- [ ] **Step 5: Commit**

```bash
git add film/js/story-seed.js film/tests/film-logic.test.js
git commit -m "Map MADLIBS genres and beats onto the ones a film is cut from"
```

---

### Task 5: A MADLIBS story becomes a film premise

**Files:**
- Modify: `film/js/story-seed.js`
- Test: `film/tests/film-logic.test.js`

**Interfaces:**
- Consumes: `GENRE_FOR` (Task 4), `MADLIBS.generate`, `FilmParse.parse`.
- Produces: `FilmStorySeed.idea(seed)` → `{ text, genre, title }` — a typed-idea sentence and the genre the template declares.

**The design decision that keeps this small:** do **not** build a second premise constructor. MADLIBS's logline is exactly the kind of sentence `Parse.parse` was written for — *"A brazen pilot named Cordelia discovers they are the last heir to Umberfall."* Feed it in and every existing extraction (names, roles, places, objects, genre scoring) is reused.

- [ ] **Step 1: Write the failing test**

```js
test('every MADLIBS story yields an idea a film can be made from', () => {
  for (let seed = 0; seed < 60; seed++) {
    const idea = Seed.idea(seed);
    assert(idea && typeof idea.text === 'string' && idea.text.length > 20,
      'seed ' + seed + ' gave no usable idea');
    assert(LEX.GENRES[idea.genre], 'seed ' + seed + ' gave genre ' + idea.genre);

    const premise = Parse.parse(idea.text, { seed, genre: idea.genre });
    const script = Writer.write(premise, { length: 'short', seed });
    assert(script.scenes.length >= 3, 'seed ' + seed + ' produced ' + script.scenes.length + ' scenes');
    assert(script.title && script.title.length, 'seed ' + seed + ' produced no title');
    const reel = Reel.build(script);
    assert(reel.duration > 30, 'seed ' + seed + ' produced a ' + reel.duration + 's film');
  }
});

test('the same seed always gives the same story', () => {
  for (let seed = 0; seed < 20; seed++) {
    eq(Seed.idea(seed).text, Seed.idea(seed).text, 'seed ' + seed + ' was not deterministic');
  }
});

test('different seeds give different stories', () => {
  const seen = new Set();
  for (let seed = 0; seed < 40; seed++) seen.add(Seed.idea(seed).text);
  assert(seen.size >= 20, 'only ' + seen.size + ' distinct stories in 40 seeds');
});
```

- [ ] **Step 2: Run it and watch it fail**

Run: `node film/tests/film-logic.test.js`
Expected: FAIL — `Seed.idea is not a function`.

- [ ] **Step 3: Implement**

```js
  /* A MADLIBS story, reduced to the one sentence SCRIPT FORGE's parser reads
   * best. The logline already names a person, a place and what turns — which
   * is exactly what parse() goes looking for — so the whole existing
   * extraction is reused rather than duplicated here. */
  function idea(seed) {
    var story = MAD.generate({ seed: (seed >>> 0) });
    var logline = '';
    for (var i = 0; i < story.beats.length; i++) {
      if (story.beats[i].label === 'Logline') { logline = story.beats[i].text; break; }
    }
    if (!logline && story.beats.length) logline = story.beats[0].text;
    return {
      text: logline,
      genre: GENRE_FOR[story.genre] || 'drama',
      title: story.title
    };
  }
```

Export `idea`.

- [ ] **Step 4: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js`
Expected: both PASS, 113 logic tests.

```bash
git add film/js/story-seed.js film/tests/film-logic.test.js
git commit -m "Turn a MADLIBS story into an idea a film can be made from"
```

---

### Task 6: Surprise me

**Files:**
- Modify: `film/index.html` (load the MADLIBS modules and `story-seed.js`; add the control)
- Modify: `film/js/app.js` (wire the control and the thin-idea fallback)
- Test: `film/tests/film-browser.test.js`

**Interfaces:**
- Consumes: `FilmStorySeed.idea` (Task 5).
- Produces: a **Surprise me** control that fills the idea box from MADLIBS and makes the film.

- [ ] **Step 1: Load the modules**

In `film/index.html`, before `js/app.js` and after `js/parse.js`:

```html
  <script src="../madlibs/js/dictionary.js"></script>
  <script src="../madlibs/js/templates.js"></script>
  <script src="../madlibs/js/generator.js"></script>
  <script src="js/story-seed.js"></script>
```

Those files bind `window.MADLIBS_DICT`, `window.MADLIBS_TEMPLATES` and
`window.MadlibsGenerator` respectively, which is what `story-seed.js`'s `MAD`
binding reads — the browser path resolves without `require`. Load order
matters: the dictionary and templates must come before the generator.

- [ ] **Step 2: Add the control**

Beside the existing buttons at `film/index.html:51-52` (`#write` and
`#reroll`), matching their markup and classes:

```html
        <button id="surprise" title="Borrow a story from MADLIBS">🎁 Surprise me</button>
```

- [ ] **Step 3: Wire it**

In `film/js/app.js`:

```js
`film/js/app.js` caches its elements into an `el` map via
`el[id] = document.getElementById(id)` (line 38), so add `'surprise'` to that
id list and follow the file's own idiom rather than calling `getElementById`
directly. The idea field is `el.idea` (the `<textarea id="idea">` at
`film/index.html:26`) and the existing action is the `write` button
(`<button id="write">✍ Write the script</button>`, line 51) — reuse whatever
handler that button already calls rather than inventing a new entry point.

```js
    el.surprise.addEventListener('click', function () {
      var seed = (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0;
      el.idea.value = FilmStorySeed.idea(seed).text;
      el.write.click();          // the same path the Write button takes
    });
```

The seed here is the one place a random number is correct: the user asked to be surprised. Everything downstream stays deterministic given that seed.

- [ ] **Step 4: The thin-idea fallback**

Where the app reads the typed idea, an idea that is empty or under three words borrows one:

```js
    var typed = String(el.idea.value || '').trim();
    if (typed.split(/\s+/).filter(Boolean).length < 3) {
      typed = FilmStorySeed.idea(seed).text;
    }
```

Find where the app currently reads the idea before writing a script and put it
there, so both the Write button and Another take benefit.

- [ ] **Step 5: Prove it in a browser**

Add to `film/tests/film-browser.test.js`, in the existing page section:

```js
    console.log('\nSURPRISE ME');
    const surprised = await page.evaluate(() => {
      const before = document.getElementById('idea').value;
      document.getElementById('surprise').click();
      return { before: before, after: document.getElementById('idea').value };
    });
    check(surprised.after.length > 20, `surprise me filled the idea box (${surprised.after.length} chars)`);
    check(surprised.after !== surprised.before, 'surprise me changed the idea');
```

- [ ] **Step 6: Run everything and commit**

Run: `node film/tests/film-logic.test.js && node film/tests/film-browser.test.js && node scripts/check-links.mjs && node scripts/smoke-site.cjs`
Expected: all PASS.

```bash
git add film/index.html film/js/app.js film/tests/film-browser.test.js
git commit -m "Add Surprise me, and borrow a story when the idea box is thin"
```

---

### Task 7: Prove the sameness is gone, and say what changed

**Files:**
- Modify: `film/README.md`, `shared/projects.js`

- [ ] **Step 1: Measure it**

Write a throwaway script in the scratchpad that generates **200 films at the default length** and reports:

- the distribution of distinct places per film
- the distribution of beat shapes
- the proportion of films containing a crisis
- the number of distinct premises

Run it against this branch, and against `main` (`git stash` or a worktree at `origin/main`) for the before figures. **Put both columns in your report.** The before figures are known to be: 2 places in every film, one shape per length, and **no crisis at all** at the default length.

- [ ] **Step 2: Shoot one and look at it**

Record a film end to end through the app at the default length and render a contact sheet, the way the earlier sub-project did (there are working scripts in the scratchpad). Confirm by eye that a default film now reaches a crisis — a tilted or handheld frame, a character in a broken pose. That is the visible payoff of this whole task.

- [ ] **Step 3: Update the docs**

Rewrite the story section of `film/README.md`: three to five locations, several shapes per length, a crisis in every film, and stories borrowed from MADLIBS when you have no idea of your own. Add `js/story-seed.js` to the file map. Extend the film entry's `blurb` in `shared/projects.js`; keep the badges at three.

- [ ] **Step 4: Run everything**

```bash
node scripts/check-links.mjs
node film/tests/film-logic.test.js
node music/tests/music-logic.test.js
node film/tests/film-browser.test.js
node scripts/smoke-site.cjs
```

- [ ] **Step 5: Commit**

```bash
git add film/README.md shared/projects.js
git commit -m "Document the stories: more places, more shapes, and a crisis every time"
```

---

## Self-review notes

- **Spec coverage:** more places → Task 1; a beat-driven place choice → Task 2; plural spines with the crisis guaranteed → Task 3; genre and beat mappings → Task 4; MADLIBS premises → Task 5; the Surprise me control and thin-idea fallback → Task 6; the before-and-after measurement and docs → Task 7.
- **The renderer is untouched by every task.** Beat ids do not change, so the poses, the camera, the weather and the score follow the new structures with no work at all. Any task that seems to need renderer changes is a signal the design is wrong.
- **Task 5 deliberately avoids a second premise constructor.** Feeding MADLIBS's logline through the existing parser reuses every extraction rather than duplicating it, and is why the MADLIBS work is two small tasks rather than one large one.
- **One task ends in looking rather than asserting** (Task 7's contact sheet), because "a default film now has a crisis" is a claim about what a viewer sees, and the whole point of the sub-project.
- **Not covered by tests, by choice:** whether the stories are *good*. The suite proves they vary, reach a crisis, stay deterministic and always produce a playable film. Taste is the user's call on a finished film.

---

## Results — measured, not asserted

200 films at the **default length** (`short`), ten typed ideas cycled across
seeds 0-199, generated through the real reader and writer. "Before" is
`origin/main` at the merge of sub-project 2, measured in a worktree with the
same script.

| | Before | After |
|---|---|---|
| Locations offered per film | 2, in 200 of 200 | 3 · 4 · 5 (66 / 64 / 70) |
| Locations **used** per film | 2, in 200 of 200 | 3 or 4 — avg **3.67** |
| Distinct beat shapes | **1** | **3** |
| Films reaching a crisis | **0 / 200** | **200 / 200** |
| Distinct premises | 200 | 200 |

The shapes in use at the default length are `open spark turn crisis after`,
`open spark crisis choice after` and `open push turn crisis choice`.

Four places is the ceiling at this length, not a bug: five beats, and the
opening and closing beat deliberately share one location.

### What looking at it caught

A contact sheet of one default film confirmed the payoff on screen — the
crisis plays in a location no earlier scene used, tension reaches 0.88, and
the poses break (one character sits down mid-scene, the other's hands go to
their head).

It also caught a defect no test was looking for: the crisis was
`EXT. PARKING LOT` and the action lines read *"Rain finds the same crack in
the sill it always finds"* and *"The radio is on the floor between them"*.
The lexicon's images assumed an interior, which was safe when every film had
two rooms. Measured over 400 films: 238 such lines on main, **369** after the
extra places went in — this work made a pre-existing defect worse. Fixed with
`LEX.OUTDOORS`, an outdoor twin per line swapped per scene; **369 → 6**, and
the six remaining are the idiom "has learned to take up very little room",
which is correct English in a field.

### Still open

- The engine's Windows/MSVC pragma guards (`Model.cpp`, `Font.cpp`,
  `VulkanTexture.cpp`) remain unverified — every CI run on this branch was
  cancelled by `cancel-in-progress` before the Windows job finished. Only CI
  can confirm the warning numbers are the right ones.
- No test pins the three-word boundary of the thin-idea fallback; the `< 3`
  is correct by inspection.
- `spineFor` picks by seed only, not genre — recorded as a deliberate
  deviation in the design spec.
