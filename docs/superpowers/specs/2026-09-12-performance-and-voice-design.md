# Characters that live in the room, and voices that carry

**Date:** 2026-09-12
**Status:** approved, ready to plan
**Sub-project:** 4 of the combined movie maker

## The goal

The films now look good, are cut well, and no longer feel like each other. What
they still are is **expressive cardboard**: a figure holds one of ten fixed
poses, sways once per syllable, is lit from the upper left no matter where the
room's light is, and speaks in uniform blips.

> **Correction, made before any of this was built.** The first draft of this
> spec said figures cast no shadow, that `drawBody` issues one fill per figure,
> and that sets already carry a light direction. Reading the code found all
> three wrong: `drawFigure` already paints a contact-shadow ellipse; it calls
> `drawBody` **three times** (rim, body, tint) plus a shadow fill and a halo
> rect, so a figure costs five fills, not one; and a palette carries colours
> (`key accent sky deep ink shadow lift tension`) but **no direction at all** —
> sets paint their lamps and beams as artwork, and nothing says where the light
> is. The design below is the corrected one.

This fixes those three things — how characters move, how they sit in the
picture, and what they sound like.

## Where this sits

1. ~~**Score to picture**~~ — done (PR #22).
2. ~~**Better picture**~~ — done (PR #24).
3. ~~**Better stories**~~ — done (PR #31).
4. **Motion, light and voice** — this spec.
5. **Maz Engine renders the reel** — still the later track. The reel stays plain
   data with no browser in it precisely so the engine can consume it.

Explicitly **out of scope**: the story layer, the score's composition, editing
controls, and any Maz Engine work.

## What is actually there now, measured

**Frame cost**, 240 frames of a festival film through the real `drawFrame`:

| | median | p95 | worst |
|---|---|---|---|
| 540p | **0.2 ms** | 1.4 ms | 14.4 ms |
| 1080p | **0.3 ms** | 0.7 ms | 4.1 ms |

The gate is 4 ms at 540p and 6 ms at 1080p. **A typical frame is using about a
twentieth of its budget.** There is room for everything below. (The 14.4 ms
outlier at 540p is worth a look on its own; it is not the common case and the
existing gate passes, but it is recorded here so it is not forgotten.)

**Motion**: ten joints (`head torso armL foreL armR foreR legL shinL legR
shinR`), ten fixed poses, and `gestureAt()` — a single sine swing, 0 → 1 → 0,
once per syllable. A cut snaps instantly from one pose to the next. Nothing in
the system knows where the other character is standing.

**Drawing**: `drawBody` builds legs, torso, arms and head as sub-paths of **one
path** and issues **one `fill()`**. That is not incidental — per-segment fills
previously caused a 2.9-second stall, and the single fill is what fixed it.

`drawFigure` then composites that body **three times** — offset up-left in the
key colour for a rim light, then near-black, then a tinted pass under
`lighter` — plus a contact-shadow ellipse and a halo rect. So a figure already
costs **five fills**, and already has both a shadow and a rim light. What it
does not have is any relationship between those and the room: the rim is a
fixed `translate(-w * 0.055, -h * 0.012)`, always upper-left, and the shadow is
a flat ellipse directly underfoot, whatever the light is doing.

**Voice**: one filtered blip per syllable at the character's pitch, riding a
pitch contour. Everything audible is built in Web Audio and reaches a single
`master` gain, which feeds the speakers **and** the recorder.

## Design

### Part 1 — motion

**Gaze.** Each figure learns where the other one is. The speaker turns head and
torso toward the listener; the listener turns to watch. This is an adjustment to
two angles that already exist, applied per frame — no new geometry, no new fill.
It is the cheapest change here and the one that most makes two figures read as a
conversation rather than two portraits.

**Breath and weight.** A standing person is never still. A slow weight shift
between the legs, a shallower chest cycle on the torso, and an occasional
settle of the head, all as small offsets on existing joints. Deterministic —
driven by the shot clock and the character's seed, never `Math.random()`, which
the grain tile already learned the hard way.

**Easing between poses.** A cut currently snaps. Poses interpolate over a short
window instead. `blendPoses` existed for this and was deleted as dead code when
nothing called it; this is the caller it was waiting for. Blending must respect
`POSE_LIMITS`, so a blend can never produce a pose a human could not hold.

**Walking — kept, after the first render looked wrong.** On the **push** beat a
character crosses part of the frame instead of standing in it: a stride on the
legs, counter-swinging arms, and an x offset over the shot.

The first version applied the cycle to whatever pose the beat had chosen. The
push beat's usual pose is `reach`, whose arm sits at 1.7 radians — straight out
— so a stride on top of it produced a striding zombie with one arm held
horizontally. Every test was green; it was the contact sheet that caught it.
The walk now starts from the `walk` pose that already existed in the table.

The knee folds only on the swing leg (`max(0, swing)`), which keeps the stance
leg near-straight and the figure on the floor — the planted-foot property the
still poses already had, and now a test asserts it at every phase of the cycle.

### Part 2 — the picture

**Use the light direction that already exists.** *(Second correction: this
section first said one had to be invented.)* `film-sets.js` exports `LIGHT`, a
behaviour per set — sweep, passing, flicker, cloud, none, covering all 15 — and
`lightAt()`, which returns `{ brightness, offset }` where **`offset` is a signed
horizontal light position that already moves over time**. A lighthouse beam
sweeps it; a passing car runs it from one side to the other. The renderer
already places its light wash with it. The figures simply never saw it, which is
the actual gap — and using it means the rim *swings with the beam* rather than
sitting at a fixed angle, which is better than what was designed.

**The rim light follows it.** The rim is currently a fixed offset up and left,
which is right for exactly one set and wrong for the other fourteen. Offsetting
along the set's actual light direction instead means a character lit by the
lighthouse lamp is rimmed on the side the lamp is on. No new fill — the same
three-pass composite, with the offset computed rather than constant.

**The contact shadow follows it too.** Today it is a flat ellipse directly
underfoot. Falling *away* from the light, and stretching as the light gets
lower, is what makes a floor look like a floor. Still one fill; it already
exists, it just needs to know which way the light is coming from.

**Faces — tried twice, dropped.** *(Outcome recorded after building and
looking, which is what this section asked for.)*

The first attempt put two eyes and a mouth in one path. At a close framing the
three ellipses merged into an unreadable smudge; at mid and wide it was a white
speck that read as dirt on the lens. The second attempt separated the fills,
shrank the features and drew nothing below a head size of 300px, which fixed the
smudge and produced something that genuinely reads as a face in a close-up.

It was still dropped, for three reasons:

1. **It only appears in close framings**, which are a minority of shots. A face
   that is present in one shot and gone in the next cut reads as a mistake, and
   inconsistency is worse than absence.
2. **It changes what the films are.** Side by side, the faceless silhouette is
   the better image — moodier, and of a piece with the noir style the sets, the
   palette and the rim light are all built for. The face pulls it toward
   cartoon.
3. Nothing else depended on it except the mouth-sync task, which goes with it.

Recorded rather than quietly abandoned: this is a taste call made by looking at
a rendered comparison, not a technical impossibility. Anyone who wants faces can
have them — the second version worked. `git log` has both attempts.

### Part 3 — voices

**The trap, recorded so nobody walks into it.** The browser's built-in
text-to-speech (`speechSynthesis`) would say real words for almost no work. It
does **not** run through Web Audio, so it never reaches the `master` gain that
feeds `createMediaStreamDestination`. The dialogue would be audible while
previewing and **silent in every downloaded film**. It also varies by device and
operating system, which breaks the guarantee that one idea and one seed always
give one film. It is not the path.

**What is built instead**: vowel-shaped voices in Web Audio. Each syllable's
vowel picks a pair of formant frequencies, which shape a filtered buzz at the
character's own pitch. "I can't" and "Say it" stop sounding identical. It
records correctly, runs offline, and stays deterministic.

**Mouths move with it.** The vowel that shapes the sound also opens the mouth,
so the face (if it is kept) is in sync with what is heard rather than flapping
on a timer.

**Honest about the ceiling**: this produces *a voice with character*, not an
actor reading lines. Recognisably a person speaking, not intelligible words.
Real speech needs shipped audio or a cloud service, and both break the offline,
self-contained thing the whole arcade is built on.

## Testing

**Logic (Node, no browser):**

- a blend between any two poses stays inside `POSE_LIMITS` at every step
- gaze turns the speaker toward the listener's side, and away when they swap
- breath and weight are deterministic: the same shot and seed give the same
  offsets, twice
- every vowel in the lexicon's dialogue maps to a formant pair
- the same caption always produces the same sequence of vowels
- a walk cycle plants a foot on the ground at every phase (the existing
  planted-foot property, extended to a moving figure)

**Browser:**

- the frame budget gate still passes at 540p and 1080p, with the new drawing in
- a recorded film still carries its soundtrack, and the dialogue is *in the
  recording* — the specific thing `speechSynthesis` would have broken
- the existing suite keeps passing untouched

**Looked at, not only asserted:** a contact sheet before and after for figures,
shadows, light and faces. The sitting-pose bug and the parking-lot sill were
both caught by looking, not by a test.

## Risks

- **Fill count.** A figure already costs five fills; a face would make it six.
  Two figures at six fills is still far inside the measured 0.2 ms median — but
  the budget gate is the check, and it runs on every push.
- **Faces may cheapen it.** Mitigated by building them behind a decision point
  rather than assuming they stay.
- **Walking may look wrong.** The most likely thing here to look bad. Contact
  sheet, and drop it if it does not read.
- **Determinism.** Every new motion term must come from the clock and the seed.
  One `Math.random()` would silently break two recordings of one film matching.
- **Voices may annoy.** Blips are inoffensive; a formant voice has opinions.
  Judged on a real film, with the option of keeping it subtler than designed.

## Build order

1. Measure and gate: extend the frame budget test to cover the new drawing.
2. Gaze.
3. Breath and weight.
4. Pose easing (revive `blendPoses` with limits enforced).
5. A light direction per set, varying with the hour.
6. The rim light and the contact shadow both follow it.
7. Faces — build, render, look, decide.
8. Walking on the push beat — build, render, look, decide.
9. Vowel formants; captions to vowels.
10. Mouths in sync with the vowels.
11. Prove a recorded film still carries its dialogue.
12. Measure the before and after, and say what it looks like now.
