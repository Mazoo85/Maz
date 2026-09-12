# Motion, light and voice — implementation plan

> **For agentic workers:** steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** characters that move like people, sit in the room's light, and speak
with a voice instead of a blip.

**Architecture:** all three land in the existing header-free ES5 browser modules.
Motion is angles on the ten joints that already exist. Light and shadow are
paint on the single existing fill plus at most two more. Voice is Web Audio,
reaching the same `master` gain the recorder already listens to.

**Tech stack:** ES5 (`var`, `function` — no arrows, no `const`/`let`) in
`film/js/*`; Node test files are exempt. Canvas 2D. Web Audio.

## Global constraints

Copied from the spec; every task's requirements implicitly include these.

- **`drawBody` issues one `fill()` for the body**, and `drawFigure` composites it
  three times (rim, body, tint) alongside a shadow fill and a halo rect — five
  fills per figure today. A face would make it six. Do not multiply that further
  without measuring: per-segment fills caused a 2.9-second stall once already.
- **Frame budget: 4 ms at 540p, 6 ms at 1080p.** Baseline median is 0.2 ms.
- **Determinism.** Every motion term comes from the shot clock and the
  character's seed. No `Math.random()` anywhere in the renderer — the grain tile
  already broke this guarantee once.
- **Blends respect `POSE_LIMITS`.** A blend must never produce a pose a human
  could not hold.
- **Everything audible must reach the `master` gain**, or it is not in the
  downloaded film.
- **ES5 in `film/js/`.** The house module ending stays:
  `if (typeof module === 'object' && module.exports) module.exports = API; root.Name = API;`

---

### Task 1: Gaze — the speaker turns to the listener

**Files:** `film/js/film-figures.js` (add `gazeAt`), `film/js/film-player.js`
(pass the other figure's x), `film/tests/film-logic.test.js`

**Produces:** `FilmFigures.gazeAt(pose, selfX, otherX, amount)` → a new pose with
`head` and `torso` turned toward `otherX`, both clamped to `POSE_LIMITS`.

- [ ] Write failing tests: turning toward a listener on the right yields a
      positive head angle; on the left, negative; `otherX === selfX` changes
      nothing; the result never leaves `POSE_LIMITS` for any input including
      absurd distances.
- [ ] Run them, watch them fail.
- [ ] Implement `gazeAt`. Head turns more than torso. Clamp both.
- [ ] Wire it in `paintFigures`: the speaking figure gazes at the other, the
      listening figure gazes back, weaker.
- [ ] Run the logic suite and the browser budget gate.
- [ ] Commit.

### Task 2: Breath and weight

**Files:** `film/js/film-figures.js` (add `aliveAt`), `film/js/film-player.js`,
`film/tests/film-logic.test.js`

**Produces:** `FilmFigures.aliveAt(pose, seconds, seed)` → pose with a slow
weight shift on the legs, a shallow chest cycle on the torso, and a head settle.

- [ ] Write failing tests: same `(seconds, seed)` gives the identical pose twice;
      different seeds differ; every output inside `POSE_LIMITS`; the offsets are
      small (no joint moves more than a stated fraction of its range).
- [ ] Run, watch fail.
- [ ] Implement with sines on the clock, phase-offset by seed. No randomness.
- [ ] Wire into `paintFigures` before `gestureAt`.
- [ ] Run the suites; commit.

### Task 3: Poses ease instead of snapping

**Files:** `film/js/film-figures.js` (revive `blendPoses`),
`film/js/film-player.js`, `film/tests/film-logic.test.js`

- [ ] Write failing tests: a blend at t=0 is the first pose exactly, at t=1 the
      second exactly; every intermediate is inside `POSE_LIMITS`; blending any
      pair of the ten poses at 21 steps never escapes the limits.
- [ ] Run, watch fail.
- [ ] Implement `blendPoses(a, b, t)` with clamping.
- [ ] Blend across the first ~0.3 s of a shot, from the previous shot's pose.
- [ ] Run the suites; commit.

### Task 4: ~~Every set gets a light direction~~ — already existed

> **Collapsed into Task 5 after reading the code, for the second correction in
> this plan.** `film-sets.js` already exports `LIGHT` (a behaviour per set:
> sweep, passing, flicker, cloud, none — all 15 covered) and `lightAt()`, which
> returns a live `{ brightness, offset }` where **offset is a signed horizontal
> light position that already moves**: a lighthouse beam sweeps across it, a
> passing car runs it -1 to +1. The renderer already uses it to place the light
> wash. Nothing needed inventing; the figures simply could not see it.

### Task 4 (was): Every set gets a light direction

**Files:** `film/js/film-sets.js`, `film/js/film-art.js`,
`film/tests/film-logic.test.js`

> Rewritten after reading the code: there is no light direction anywhere today.
> A palette carries colours only, and sets paint their lamps as artwork. The
> original Task 4 ("add contact shadows") was dropped outright — `drawFigure`
> has drawn a contact-shadow ellipse all along.

**Produces:** a light direction per set, in radians, shifted by the hour.

- [ ] Write failing tests: every set in `SETS` has a light direction; it is a
      finite number in range; the same set and hour always give the same
      direction; DAY and NIGHT differ for a set with a window.
- [ ] Run, watch fail.
- [ ] Add the direction to all 15 sets, chosen to match where each already
      paints its lamp, window or beam.
- [ ] Run the suites; commit.

### Task 5: The rim light and the shadow follow the light

**Files:** `film/js/film-figures.js`, `film/tests/film-browser.test.js`

- [ ] Replace the rim's constant `translate(-w * 0.055, -h * 0.012)` with an
      offset along the set's light direction. Same three-pass composite, same
      fill count.
- [ ] Make the contact shadow fall away from the light and stretch as the light
      lowers, instead of sitting flat underfoot.
- [ ] Budget gate at both resolutions; contact sheet; look; commit.

### Task 6: Faces — build, look, decide

**Files:** `film/js/film-figures.js`

- [ ] Two eyes and a mouth line, rotated with the head angle, in one more fill.
- [ ] Render a contact sheet at close, mid and wide framings.
- [ ] **Look at it.** If it cheapens the silhouette style, drop it and record in
      the spec that it was tried and why it went. If it keeps, commit.

### Task 7: Walking on the push beat — build, look, decide

**Files:** `film/js/film-figures.js`, `film/js/film-player.js`

- [ ] A walk cycle on the existing leg joints, plus an x offset across the shot.
- [ ] Test: a foot is planted at every phase of the cycle (the existing
      planted-foot property, extended to a moving figure).
- [ ] Contact sheet across a whole push beat. **Look.** Keep or drop.

### Task 8: Vowel-shaped voices

**Files:** `film/js/film-audio.js`, `film/js/parse.js` (reuse `syllablesFor`),
`film/tests/film-logic.test.js`

**Produces:** a caption → vowel sequence mapping, and formant pairs per vowel.

- [ ] Write failing tests: every vowel maps to a formant pair; the same caption
      always gives the same vowel sequence; a caption with no vowels still
      produces something speakable.
- [ ] Run, watch fail.
- [ ] Implement the vowel table and the caption→vowels split, reusing the
      existing syllable clock so audio and captions stay in step.
- [ ] Replace the blip with two formant filters over the same buzz, at the
      character's pitch.
- [ ] Run the suites; listen if possible; commit.

### Task 9: Mouths move with the vowel

**Files:** `film/js/film-figures.js`, `film/js/film-player.js`

- [ ] The vowel that shapes the sound sets the mouth opening, so the face is in
      sync with what is heard. Only if Task 6 kept faces.
- [ ] Contact sheet; commit.

### Task 10: Prove the dialogue is in the recording

**Files:** `film/tests/film-browser.test.js`

- [ ] Record a film and assert the soundtrack carries the dialogue — the exact
      thing `speechSynthesis` would have broken. This is the regression guard
      for the trap the spec documents.
- [ ] Commit.

### Task 11: Measure, look, and say what changed

**Files:** `film/README.md`, the spec, `shared/projects.js`

- [ ] Frame cost before and after, both resolutions, same method as the baseline.
- [ ] A contact sheet of the same film before and after.
- [ ] Update the README and the spec with what was kept, what was dropped, and
      the honest limits of the voice.
- [ ] Commit.

## Self-review notes

- **Spec coverage:** gaze → 1; breath/weight → 2; easing → 3; shadows → 4;
  light → 5; faces → 6; walking → 7; voices → 8; mouths → 9; the recording
  guarantee → 10; measurement and docs → 11.
- **Two tasks end in looking rather than asserting** (6 and 7), because "does
  this cheapen the style" and "does this walk read" are claims about what a
  viewer sees. The sitting-pose bug and the parking-lot sill were both caught
  by looking.
- **Nothing here touches the story layer or the score's composition.** If a task
  seems to need that, the design is wrong.
