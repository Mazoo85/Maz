# Better picture — characters that act, sets with depth, a camera that means something

**Date:** 2026-09-11
**Status:** approved, ready to plan
**Sub-project:** 2 of 3 in the combined movie maker

## The goal

A SCRIPT FORGE film currently draws each character as one fixed silhouette with a
breathing wobble, on a flat single-layer set, shot with five camera moves. That
is the ceiling on how good these films can look, and it is a low one.

This raises it three ways: **characters that act**, **sets with depth and
weather**, and **a camera whose moves mean something**. All of it is driven by
data the reel already carries — the beat, the tension, who is in frame, who is
speaking — so the film's emotional shape reaches the picture without anyone
hand-animating a frame.

## Where this sits

1. ~~**Score to picture**~~ — done and merged (PR #22). SONG FORGE scores every
   film.
2. **Better picture** — this spec.
3. **Better stories** — MADLIBS becomes the story brain so films stop sharing one
   seven-beat spine.
4. **Maz Engine renders the reel** — the later track. The reel stays plain data
   with no browser in it precisely so the engine can consume it.

Explicitly **out of scope**: editing controls, the story brain, any Maz Engine
work, and sound of any kind — the score landed in sub-project 1 and is not
touched here.

## Architecture

`film/js/film-art.js` is 654 lines and would roughly double. It splits three
ways, by responsibility:

| File | Owns |
|---|---|
| `film/js/film-art.js` | the palette, the grain, the vignette, the letterbox |
| `film/js/film-sets.js` | the fifteen sets, now in three layers each |
| `film/js/film-figures.js` | the joint model, the pose library, figure drawing |

`film/js/film-player.js` keeps the camera and compositing. `film/js/film-reel.js`
gains the cutting rules, since it already decides framing.

Everything new that *chooses* — which pose, which weather, how to cut — is pure
logic, testable in Node. Only the drawing itself needs a browser.

## Part 1 — characters that act

### The figure

`drawFigure` draws one fixed pose. It is replaced by a jointed figure: head,
torso, upper arms and forearms, thighs and shins. **A pose is a set of joint
angles** — a plain object of numbers, which is what makes everything below
testable and cheap.

### The pose library

`stand`, `turn-away`, `reach`, `recoil`, `sit`, `slump`, `hands-in-pockets`,
`point`, `head-in-hands`, and a `walk` cycle. Changing pose interpolates the
angles over about 0.5s, so a character moves into a pose rather than snapping to
it.

> **Shipped differently:** `blendPoses` (the interpolation this describes) was
> built, then deleted as dead code before release. `poseFor` is keyed on the
> shot, so a pose is constant for the life of a shot and only ever changes at
> a cut — there is no moment *during* a shot where two poses need blending,
> and blending across a cut would be wrong regardless, since a character
> should already be in position when the cut lands on them. A pose change is
> a snap, at the cut, not an interpolation. See `git show 27aae39` for the
> removal.

### What chooses the pose

The beat and the tension, both already on every shot:

| Beat | Bearing |
|---|---|
| open | stillness — `stand`, `hands-in-pockets` |
| spark | `turn-away` into `reach` |
| push | movement — `walk`, `point` |
| turn | `stand` squared up, or `turn-away` from the other |
| crisis | `recoil`, `slump`, `head-in-hands` |
| choice | straightening up out of whatever the crisis left |
| after | stillness again, but not the same stillness |

Pose selection is a pure function of `(beat, tension, isSpeaker, seed)`, so the
same film poses the same way every time.

### Gesture on the voice

The score already fires one voice blip per syllable. The same timing drives a
small head-and-hand gesture, so **a speaking character moves in time with their
own voice**. It is the cheapest possible stand-in for lip-sync, costs almost
nothing, and is the single change most likely to make these read as characters
rather than shapes.

## Part 2 — sets with depth and weather

### Three layers

Every set splits into **back**, **mid** and **fore**, moving at different rates
under a pan or push. That parallax is what turns a drawing into a space.

**The figures are drawn between mid and fore.** That is the part that matters: a
doorframe edge, a curtain, railings, branches or a table edge can pass *in front
of* a character. One dark foreground element per set, sitting slightly out of
frame and moving most with the camera, buys more depth than anything else of
comparable effort.

> **Measured against what actually ships:** rendering each set's fore
> element and its figures separately and checking their painted pixels for
> overlap (not just a bounding box) says the "passes in front of a
> character" case — the thing that buys the depth — only actually happens
> in 2 of the 15 sets, and only in some framings: `field`'s fence rail
> crosses 9-12% of a figure's own silhouette in the `wide`, `mid`, `two` and
> `low` framings (it sits at shin height, so a `close` crop — chest and up
> — never reaches it), and `woods`'s low branch crosses about 2% of a
> figure's silhouette in the `low` framing only. A handful of other
> set/framing pairs (`room` `wide`, `woods` `ots`, `field` `ots`) touch
> under 0.6% of a figure — a stray pixel or two at a silhouette's edge, not
> a visible "in front of" read. Every other set's fore element sits along an
> edge the figures never stand in front of (a table edge or doorframe below
> or beside where a figure is placed, not across it). The parallax-depth
> read from the three-plane split still holds everywhere; the specific
> "occlusion" read this section describes does not.

### Weather and air

Chosen by genre and hour, drawn as its own layer with its own drift:

| Conditions | Air |
|---|---|
| thriller / horror, night | rain streaks |
| drama, day | dust motes turning in a shaft of light |
| horror / mystery | fog bands |
| western, day | heat shimmer |
| fantasy | embers |
| any night interior | haze around the key light |

### Light that moves

Within a scene, not just between them: the lighthouse beam sweeping the room,
headlights crossing a wall, a bulb that flickers harder as tension climbs, cloud
shadow passing over a field. The set says what light it owns; the reel's tension
says how agitated it is.

## Part 3 — the camera and the cutting

### Moves

Added to the existing push, pull, pan and drift:

- **handheld** — seeded jitter whose strength comes from the tension, so the
  frame is unsteady exactly when the story is
- **tracking** — moving alongside a character rather than zooming at them
- **Dutch tilt** — a couple of degrees of roll, reserved for the crisis
- **whip pan** — a transition between two shots inside one scene
- **rack focus** — shifting emphasis between foreground and figure

Rack focus is the one with a real cost question. Canvas blur may be too
expensive per frame; it gets prototyped and measured, and if it is, the fallback
is darkening and flattening the out-of-focus plane instead, which reads nearly
as well for nearly nothing.

### Framings

Two new ones, both paying off the foreground layer: **over-the-shoulder**, with
the listener as a large dark shape at the frame edge, and **low angle**, where
the figures loom. A defeat gets the high angle instead.

### The cutting

The reel picks framing with some randomness today. Instead, rules that come from
the beat it already knows:

- a two-hander alternates over-the-shoulder and reverse, so it reads as a
  conversation rather than a slideshow
- the push cuts on movement
- the choice holds longer than is comfortable
- a scene may end on a held empty frame

## The frame budget

Recording happens in real time, so every frame has a budget. **Measured before
planning** — 300 frames sampled across a whole film, so every set and framing is
hit:

| Canvas | mean | p95 | worst |
|---|---|---|---|
| 960×540 | 0.46ms | 0.8ms | 6.9ms |
| 1280×720 | 0.43ms | 0.7ms | 8.3ms |
| 1920×1080 | 0.40ms | 0.7ms | 6.5ms |

A 30fps recording allows 33ms a frame, so the renderer currently uses about 1.4%
of it — and the cost is **flat across resolution**, because this is vector
drawing rather than per-pixel work.

That changes the shape of the risk. There is room for the picture to get an order
of magnitude more expensive and still record cleanly. The one thing that would
break the pattern is anything **per-pixel**: canvas blur for rack focus is the
candidate, and it is the reason rack focus gets prototyped and measured on its
own rather than assumed.

The gate, tightened to something that actually bites while leaving eight times
today's headroom:

- particle counts are capped and scale with canvas size
- a browser test measures mean and worst frame time over a few hundred frames and
  fails if the **mean exceeds 4ms at 540p or 6ms at 1080p**
- if weather or blur proves expensive it is thinned at 1080p rather than slowing
  everything

## Testing

**Logic (Node, no browser):**
- every pose in the library defines every joint, and every angle is inside a
  plausible human range
- pose selection is deterministic for a given film, and each beat yields a pose
  from its own allowed set
- ~~interpolation between any two poses stays inside those ranges throughout,
  and lands exactly on the target~~ — not shipped: pose changes are a snap at
  the cut, not an interpolation (see the note under "The pose library" above)
- every set declares all three layers, and layer parallax rates are ordered
  back < mid < fore
- weather choice covers every genre and hour combination and never returns
  something the drawing does not implement
- the cutting rules: a two-hander alternates rather than repeating a framing,
  and the choice beat is held longer than the push

**Browser:**
- a film still plays, records and downloads (the existing suite must keep
  passing untouched)
- the picture differs materially between two moments in the same scene — proof
  the figures and the air are actually moving
- frame cost stays inside the budget above at both sizes

## Risks

- **Per-pixel effects.** Measuring first demoted the frame budget from "the
  honest risk of the sub-project" to a non-issue for everything except blur: at
  0.4ms a frame there is room for ten times the drawing. Canvas blur for rack
  focus is the one candidate that could break that, so it is prototyped and
  measured alone, with a cheap fallback already chosen.
- **Poses looking wrong rather than static.** A bad pose is more noticeable than
  no pose. Mitigation: the pose library is small and hand-checked against
  rendered frames before the selection logic is wired to it.
- **The split of `film-art.js`** touches a file every other part of the renderer
  calls. It happens first, as its own step, with the existing suites proving
  nothing changed before any new drawing is added.

## Build order

1. Split `film-art.js` into art, sets and figures with no behaviour change —
   suites green before anything else starts.
2. ~~Measure the frame budget as it stands today~~ — done before planning; the
   numbers are in *The frame budget* above. Add the browser test that enforces
   the gate.
3. The joint model and pose library, with its logic tests.
4. Pose selection from beat and tension; gesture on the voice blips.
5. Three-layer sets and the foreground element, figures drawn between mid and
   fore.
6. Weather, air and moving light.
7. Camera moves, then the new framings.
8. The cutting rules in the reel.
9. Re-measure the frame budget; thin whatever is over.
10. Documentation, and a film shot end to end to look at.
