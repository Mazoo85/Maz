# The engine renders the reel

**Date:** 2026-09-12
**Status:** approved, ready to plan
**Sub-project:** 5 of the combined movie maker

## The goal

Every film so far has been drawn by a browser and captured by a browser. The
reel — the ordered list of timed shots that *is* the film — has been kept as
plain data with no browser in it since the first sub-project, precisely so that
something else could one day read it. This is that.

## Why it is worth doing, measured

Recording today is `canvas.captureStream(30)` into a `MediaRecorder`. That is a
**realtime** capture, and three facts follow from it:

1. **A three-minute film takes three minutes to record.** There is no way to go
   faster; the recorder is watching a canvas play.
2. **A frame the browser misses is missing from the file.** The capture takes
   whatever the canvas had when it looked. Nothing detects the gap.
3. **The file comes out without a duration in it** — `film-webm.js` exists only
   to walk the container afterwards and patch one in, because the recorder
   writes its header before it knows how long the film is.

A native renderer has none of those problems: it draws frame *n* at time
`n/fps`, writes it, and moves on. It cannot drop a frame, it knows the duration
before it starts, and it runs as fast as the machine allows.

## What the engine already has, checked rather than assumed

| Need | Status |
|---|---|
| Read the reel | **`io/Json.hpp`** — full parser and writer |
| A CPU raster to draw into | **`render/Image.hpp`** — RGBA8, top-left origin |
| Write the frame out | **`render/ImageCodecQoi.hpp`** — `encodeQoi`, lossless |
| Anti-aliased coverage blending | **`render/LineAA.hpp`** — `blendCoverage` |
| Curves to build shapes from | **`math/Curve2D.hpp`**, `BezierIntersect`, … |
| **Fill a shape** | **missing — nothing in the engine can do it** |

`render/ImageDraw.hpp` offers `drawLine`, `drawRect`, `drawCircle`,
`fillCircle`, `fillTriangle`. That is the whole of it. There is no filled
polygon, no ellipse, no path, and no anti-aliasing on any of them.

(The GPU-side `Renderer` has a `drawConvexPolygon`, but it is **convex only**
and needs Vulkan. A figure's body — arms crossing a torso — is not convex, and
the point of this sub-project is to draw without a GPU at all.)

**Every single thing a film draws is a filled path.** A figure's body is one
path of ten joints; a contact shadow is an ellipse; a sky is a filled rect under
a gradient; a set is layered silhouettes. So the gap between "the engine has a
film reel" and "the engine draws the film" is one missing primitive.

That primitive is worth having on its own account. A vector path fill is not a
film feature — it is the thing every 2D engine needs for procedural icons, map
overlays, charts, generated sprites and UI shapes, and this engine has gone to
~690 headers without one.

## Design

### Part 1 — the missing primitive

**`render/Path.hpp`** — a path built the way every 2D API builds one:
`moveTo`, `lineTo`, `quadTo`, `cubicTo`, `ellipse`, `close`. Curves flatten to
line segments at a tolerance, so the rasterizer only ever sees polygons.

**`render/PathFill.hpp`** — a scanline rasterizer with **analytic anti-aliasing**:
for each scanline, sample the path's edges at sub-scanline resolution, sum
per-pixel coverage, and composite through the `blendCoverage` that `LineAA.hpp`
already has. Both fill rules (**nonzero** and **even-odd**), because a figure
with an arm crossing its torso needs nonzero and a ring needs even-odd.

This is CPU-only and header-only, so it is testable with no GPU and no window —
and its output is an image, so it can be *looked at*, which is the check that
has caught more real bugs in this project than any assertion.

### Part 2 — the reel becomes a file

**Browser side:** a "save the reel" export writing the reel as JSON. The reel is
already plain data; this is a serialisation and a download, nothing more.

**Engine side:** **`film/Reel.hpp`** — parse that JSON into typed shots and
answer the same two questions the browser's `film-reel.js` answers: what is on
screen at time *t*, and how long is the film. Same data, same semantics, so a
film rendered natively is the *same* film, not a lookalike.

### Part 3 — the native renderer

**`apps/filmreel`** — load a reel, draw frames, write them out.

Ported faithfully, because these are what the film's look actually is and they
are small, self-contained tables of numbers:

- **The palette** — `GENRE_COLOUR` × `HOUR` × mood, ~40 lines of pure arithmetic.
- **The figures** — `POSE_LIMITS`, the ten poses, and the body geometry, with
  the rim-light/body/tint composite and the contact shadow.
- **The cutting** — `shotAt`, the camera moves, the framing rects, the fades.
- **The captions** — the engine already has a font and text layout.

**Explicitly staged for later: the fifteen sets.** `film-sets.js` is 25KB of
per-set artwork, and porting it is a sub-project of its own. Stage one draws a
palette-correct backdrop — sky, floor, horizon, light wash — so the frame is
right in colour, light and composition while the set detail is still to come.

**This is stated plainly because it is the honest limit:** after this
sub-project the native renderer produces your film's *people, light, timing and
cutting* faithfully, and its *scenery* plainly. It is not yet a replacement for
the browser's picture, and saying otherwise would be a lie the first contact
sheet would expose.

### Part 4 — what comes out

A **frame sequence** (QOI, lossless), not a video file. The engine has
`video/Ivf.hpp`, but that is a container *demuxer* — there is no VP8/VP9/AV1
encoder in the engine and writing one is not a film feature, it is a year. A
frame sequence is the standard interchange for exactly this and any tool turns
it into a video. Sound stays the browser's job for now.

## Testing

**Logic (Node/g++, no GPU, all runnable on the cloud box):**

- a filled axis-aligned rect covers exactly its pixels and nothing outside it
- a shape and the same shape wound backwards fill identically under nonzero
- a ring fills hollow under even-odd and solid under nonzero
- coverage on an anti-aliased edge is between 0 and 1 and sums to the true area
  within tolerance
- a flattened curve stays within tolerance of the true curve
- the same reel JSON parses to the same shot list, and `shotAt` agrees with the
  browser's `shotAt` on every shot boundary of a real film
- every pose in the ported table is inside the ported `POSE_LIMITS`, and the
  ported palette matches the browser's for all 10 genres × 4 hours

**Looked at, not only asserted:** a contact sheet of natively-rendered frames,
compared against the browser's for the same reel and seed.

## Risks

- **The rasterizer is the whole job.** If path filling is wrong, everything
  downstream is wrong in a way that is hard to localise. Mitigated by testing it
  as an engine primitive in its own right, against analytic ground truths, and
  by looking at its output, before any film code depends on it.
- **Visual parity will not land in one go.** Named above rather than discovered
  later. The staged-sets decision is the mitigation.
- **Two renderers can drift.** The reel is the contract, and `shotAt` agreeing
  with the browser on every boundary is the test that holds them together.
- **`-Wconversion` and `-Werror`.** Rasterizers are full of int/float crossings.
  Every narrowing gets an explicit cast; the cloud box compiles with the same
  flags CI does.

## Build order

1. The path builder and the anti-aliased fill — the engine primitive, tested and
   looked at on its own.
2. The reel as JSON, out of the browser.
3. The reel read back natively, agreeing with the browser shot for shot.
4. The palette, ported and proven identical.
5. The figures, ported — and looked at.
6. The whole frame: backdrop, figures, captions, camera, fade.
7. Measure against realtime, contact-sheet it, and say honestly what it looks
   like and what it does not do yet.


## What actually happened

Recorded after building it, because the plan's value is in what it got wrong as
much as what it got right.

**The spec was right about the gap, for once.** The four previous sub-projects
each corrected this document after reading the code; this time the survey was
done first and the "one missing primitive" claim held. The rasterizer was
genuinely the whole job, and everything above it went in without redesign.

**Three things were wrong in the picture, and all three were found by rendering
a contact sheet and looking at it, with every test green:**

1. **Every set got a city skyline** — including a lighthouse lamp room, a moving
   car and a hallway. A room is now a wall with a window; only the four sets
   that are actually outdoors get a horizon.
2. **The window was a hole cut through to the sky**, which put a big pale
   rectangle in the middle of every interior and read as a projector screen. It
   is now small, high, on the light's side, divided, and barely brighter than
   the wall.
3. **The over-the-shoulder foreground took the beat's pose like anyone else**,
   so a `reach` — one arm straight out — filled a quarter of the frame with a
   black slab at that size and zoom. It is a shoulder now, held still. This is
   the same failure as the striding zombie: a pose that is correct in isolation
   and wrong for what it is being asked to do.

**One was found by measuring, and it was the important one.** The first version
rendered at **0.3× realtime** — three times slower than the browser it exists to
beat, which would have made the entire sub-project pointless. It now runs the
whole film at **1.9× realtime** with the picture unchanged.

Two fixes, and the split between them was itself worth measuring, because the
first write-up of this had it wrong. Rebuilding the full-frame washes for every
frame of a shot instead of once was almost all of it: caching them alone takes
0.3× to 1.8×. Giving `fillPath` an active edge table — so a row looks only at
the edges crossing it — is the rest, 1.8× to 2.3× on the same stretch. The edge
table was expected to be the dominant one, on the reasoning that a line of text
is a single path of some nine thousand edges; measuring showed it is worth about
1.6× on that path alone, because a caption occupies forty rows and most of its
edges cross most of them. **Measuring which of two fixes mattered is not the same
as measuring that the pair of them worked**, and only the second had been done.

The lesson is the one this project keeps relearning: **the claim in the spec was
"it runs as fast as the machine allows", and nothing tested it.** Correctness had
six test files and performance had none, so the one property the sub-project
existed to deliver was the one that was broken. A test is now owed here.

**Two engine gaps turned up that the survey missed**, both because the survey
asked "can the engine fill a shape" and not "can the engine draw a frame":

- **no PNG encoder** (only a decoder) — sidestepped, `encodeQoi` already existed
  and QOI suits flat-colour films better anyway;
- **no CPU text at all.** `ui::Font` needs a Vulkan renderer and a TTF, and stb
  is linked PRIVATE to the engine so no header can reach it. That became
  `render::StrokeFont`, 77 glyphs of vector geometry — a bigger detour than
  anything else here, and not one the plan anticipated.
