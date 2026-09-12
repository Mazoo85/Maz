# The engine renders the reel — implementation plan

> **For agentic workers:** steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** the Maz engine reads a film reel and draws its frames natively, faster
than realtime, with no browser involved.

**Architecture:** one missing engine primitive (an anti-aliased vector path
fill) unlocks everything else. On top of it, a header-only reel reader that
parses the same plain JSON the browser already produces, and an app that draws
frames into a `render::Image` and writes them as QOI.

**Tech stack:** C++20, header-only, std + GLM. `io/Json.hpp` for the reel,
`render/Image.hpp` for the raster, `render/ImageCodecQoi.hpp` for output. No
Vulkan anywhere in this sub-project — every part of it runs headless.

## Global constraints

- **`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.** Cast every
  narrowing explicitly. Rasterizer code is dense with int/float crossings.
- **Header-only**, under `engine/include/maz/`, namespaces mirroring directories.
- **Every new logic module gets a unit test** in `tests/`, registered in
  `tests/CMakeLists.txt` with `add_executable` + `target_link_libraries(...
  maz::maz maz_warnings)` + `add_test`.
- **Verified on this box** by compiling the test directly with
  `g++ -std=c++20 -Iengine/include -I<glm>`; CI does the full build.
- **`docs/API.md` is generated.** If a build regenerates it, `git checkout --`
  it before committing.
- **Determinism.** The same reel and seed must give the same frames.

---

### Task 1: A path, and a filled polygon

**Files:** create `engine/include/maz/render/Path.hpp`,
`engine/include/maz/render/PathFill.hpp`; create `tests/render/pathfill.cpp`;
modify `tests/CMakeLists.txt`

**Produces:** `render::Path` with `moveTo/lineTo/quadTo/cubicTo/ellipse/close`,
and `render::fillPath(Image&, const Path&, const Color&, FillRule)`.

- [ ] Write failing tests: an axis-aligned rect covers exactly its pixels; a
      triangle's ink equals its area within tolerance; a shape wound backwards
      fills the same under nonzero; a ring is hollow under even-odd and solid
      under nonzero; nothing is written outside the path's bounds.
- [ ] Run them, watch them fail to compile (no such header).
- [ ] Implement `Path` with curve flattening to a tolerance.
- [ ] Implement `fillPath`: per-scanline sub-sampling, coverage accumulation,
      composite through `blendCoverage`.
- [ ] Run the tests. Render a sheet of shapes and **look at it.**
- [ ] Register in `tests/CMakeLists.txt`; commit.

### Task 2: The reel leaves the browser

**Files:** `film/js/app.js`, `film/index.html`, `film/tests/film-logic.test.js`

**Produces:** a downloadable `<title>.reel.json` holding the whole reel.

- [ ] Write a failing test: the exported JSON round-trips to a reel with the
      same shot count, duration and every shot boundary intact.
- [ ] Run, watch fail.
- [ ] Implement the export and a button for it.
- [ ] Run the logic suite; commit.

### Task 3: The engine reads the reel

**Files:** create `engine/include/maz/film/Reel.hpp`; create
`tests/film/reel.cpp`; modify `tests/CMakeLists.txt`

**Produces:** `film::Reel`, `film::parseReel(json)`, `film::shotAt(reel, t)`.

- [ ] Write failing tests against a real exported reel checked in as a fixture:
      shot count and duration match; `shotAt` returns the right shot at every
      boundary, just before it, and just after; out-of-range times clamp the way
      the browser's does.
- [ ] Run, watch fail.
- [ ] Implement the parser and `shotAt`.
- [ ] Run; commit.

### Task 4: The palette, ported exactly

**Files:** create `engine/include/maz/film/Palette.hpp`; modify
`tests/film/reel.cpp` (or a new `tests/film/palette.cpp`)

- [ ] Write failing tests: all 10 genres × 4 hours × 3 moods match values
      generated from the browser and checked in as a fixture, channel for
      channel.
- [ ] Run, watch fail.
- [ ] Port `GENRE_COLOUR`, `HOUR`, `mix`, `scale`, `palette`.
- [ ] Run; commit.

### Task 5: The figures, ported — and looked at

**Files:** create `engine/include/maz/film/Figure.hpp`; create
`tests/film/figure.cpp`; modify `tests/CMakeLists.txt`

- [ ] Write failing tests: every pose is inside `POSE_LIMITS`; the planted-foot
      property holds for all ten poses; the body path is closed and non-empty.
- [ ] Run, watch fail.
- [ ] Port `POSE_LIMITS`, `POSES`, and the body geometry as a `render::Path`.
- [ ] Draw all ten poses to a sheet and **look at it.** The sitting-pose bug and
      the striding zombie were both caught exactly here.
- [ ] Run; commit.

### Task 6: A whole frame

**Files:** create `apps/filmreel/main.cpp`, `apps/filmreel/CMakeLists.txt`;
modify root `CMakeLists.txt`

- [ ] Backdrop from the palette (sky, horizon, floor, light wash).
- [ ] Figures placed by the shot's characters and sides, with rim, tint and
      contact shadow.
- [ ] Captions through the engine's existing text layout.
- [ ] Camera move and the fades, from the reel.
- [ ] `--reel <file> --out <dir> --fps N --frames N --width W`.
- [ ] Render a real film. **Look at a contact sheet.**
- [ ] Commit.

### Task 7: Measure, look, and say what it does and does not do

**Files:** `film/README.md`, `docs/`, the spec

- [ ] Time a full film natively against its realtime duration; state the ratio.
- [ ] Contact sheet, native beside browser, same reel and seed.
- [ ] Write down honestly what is faithful (people, light, timing, cutting) and
      what is plain (the sets), and what is not attempted (sound, video file).
- [ ] Commit.

## Self-review notes

- **Spec coverage:** primitive → 1; reel out → 2; reel in → 3; palette → 4;
  figures → 5; whole frame → 6; measurement and honesty → 7.
- **Task 1 is the whole risk.** It is deliberately first, deliberately tested as
  an engine primitive against analytic ground truths rather than through a film,
  and deliberately ends in looking at a sheet of shapes.
- **Two tasks end in looking** (5 and 6). Every visual bug this project has had
  was found that way and none were found by assertions.
- **Nothing here touches the story layer, the score, or the browser renderer.**
  If a task seems to need to, the design is wrong.
