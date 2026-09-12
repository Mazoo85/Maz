# Characters that live in the room, and voices that carry

**Date:** 2026-09-12
**Status:** approved, ready to plan
**Sub-project:** 4 of the combined movie maker

## The goal

The films now look good, are cut well, and no longer feel like each other. What
they still are is **expressive cardboard**: a figure holds one of ten fixed
poses, sways once per syllable, casts no shadow, ignores the light in the room
it is standing in, and speaks in uniform blips.

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
previously caused a 2.9-second stall, and the single fill is what fixed it. It
also means a figure is one flat colour, which is why it reads as pasted on.

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

**Walking.** On the **push** beat — the beat that is about momentum — a
character crosses part of the frame instead of standing. Needs a walk cycle
(legs already have knees) and an x offset over the shot. This is the largest
piece of the three and the one most likely to look wrong; it gets its own
contact sheet and a look before it is kept.

### Part 2 — the picture

**Contact shadows.** A figure currently floats: nothing joins it to the floor it
is standing on. A soft ellipse under the planted foot, in one extra fill, seats
it. `drawBody` already computes where the lowest foot lands in order to plant
the figure — the shadow reuses that number rather than recomputing it.

**Light on the figure.** Sets already carry a light direction and colour; the
figure ignores both and is one flat ink. Painting the body with a gradient
along the light direction keeps it to **one fill** (a gradient is a paint, not
an extra draw) and seats the figure in the room's light.

**Faces, on probation.** Two eyes and a mouth line, oriented with the head
angle, in one more fill. This could transform how much emotion reads, or it
could wreck the silhouette style the films have. It is **built, rendered to a
contact sheet, and looked at** before anyone decides to keep it. If it cheapens
the look, it is dropped and the spec records that it was tried.

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

- **The single fill.** Shadows and faces each add a fill. Three fills per figure
  instead of one, times two figures, is still far inside the measured budget —
  but the budget gate is the check, and it runs on every push.
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
5. Contact shadows.
6. Light on the figure.
7. Faces — build, render, look, decide.
8. Walking on the push beat — build, render, look, decide.
9. Vowel formants; captions to vowels.
10. Mouths in sync with the vowels.
11. Prove a recorded film still carries its dialogue.
12. Measure the before and after, and say what it looks like now.
