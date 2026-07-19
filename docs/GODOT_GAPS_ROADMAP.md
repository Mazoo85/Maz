# Closing the Godot Gaps — the Work Plan (everything except ecosystem)

This is the executable plan for `GODOT_GAPS.md`. The user asked to close **every gap except the
ecosystem section** (§8 of `GODOT_GAPS.md`). This document turns the remaining gaps into an ordered
queue of concrete, buildable milestones that the autonomous milestone loop works through one at a
time, newest progress tracked in the scratchpad log and `GODOT_PARITY.md`.

## The honest constraint (read this first)

This build machine has **no GPU, no audio device, and no VR/console hardware**. That splits every
item into three verifiability tiers, and each milestone below is tagged with one:

- **[VERIFIABLE HERE]** — pure CPU/logic/parsing/tooling. Can be written *and* unit-tested to the
  same "N checks, 0 failures" bar as the rest of the engine. The bulk of the list is this.
- **[CODE HERE / SEE IT ON YOUR MACHINE]** — real code can be written and compiled here, and its CPU
  parts tested, but the visible/audible result (a lit frame, a played sound) can only be confirmed on
  a machine with a real GPU/audio device. These are honestly marked "written, not visually verified."
- **[NEEDS YOUR HARDWARE/TOOLCHAIN]** — cannot be meaningfully done in this sandbox at all (console
  SDKs under NDA, a physical VR headset, an app-store signing pipeline). These are documented and
  deferred, never claimed as done.

Nothing is ever marked complete on this branch unless it passed real verification. Where a thing can
only be written blind, the docs say exactly that.

## Queue — grouped by `GODOT_GAPS.md` section (ecosystem §8 intentionally excluded)

### §4 Import formats — mostly [VERIFIABLE HERE] (pure parsers)
- [x] **Collada `.dae` mesh import** (`render::parseCollada`) — DONE (M497). [VERIFIABLE HERE]
- [x] **PLY mesh import** (ASCII + binary) — `render::parsePly` — DONE (M498). [VERIFIABLE HERE]
- [ ] **STL mesh import** (ASCII + binary) — `render::parseStl`. [VERIFIABLE HERE]
- [x] **TGA image decode** — ALREADY PRESENT (`render::decodeTga`, `ImageCodecTga.hpp`). No work needed.
- [x] **BMP image decode** — ALREADY PRESENT (`render::decodeBmp`, `ImageCodecBmp.hpp`). No work needed.
- [x] **DEFLATE / zlib inflate** (`io::inflateRaw` / `io::zlibInflate`) — DONE (M499); prerequisite for PNG. [VERIFIABLE HERE]
- [x] **PNG decode** (`render::decodePng` / `loadPng`) — DONE (M500); chunks + all 5 filters + gray/RGB/RGBA/palette. [VERIFIABLE HERE]
- [ ] **Ogg Vorbis / MP3 decode to PCM** — large, pure-CPU decoders feeding the existing mixer. [VERIFIABLE HERE]
- [x] **Font fallback chains** (`ui::FontFallback`) — DONE (M501); per-codepoint resolution + per-font runs. [VERIFIABLE HERE]
- [ ] **FBX import** — binary + ASCII FBX is large and semi-proprietary; do the geometry subset. [VERIFIABLE HERE]

### §7 Text / UI / localization depth — [VERIFIABLE HERE]
- [x] **Unicode BiDi runs + base direction** — ALREADY PRESENT (`ui::bidiRuns` / `baseDirection`, `TextServer.hpp`).
- [x] **Line-break opportunities** — ALREADY PRESENT (`ui::lineBreakOpportunities`, `TextServer.hpp`).
- [ ] **Basic complex-script shaping hooks** (mark positioning, ligature substitution tables). [VERIFIABLE HERE]
- [ ] **Localization tooling** — POT/PO extract + import beyond the current CSV tables. [VERIFIABLE HERE]
- [ ] **Video container/codec decode** to frames (display is GPU-side). [CODE HERE / SEE IT ON YOUR MACHINE]

### §6 Scripting & language — [VERIFIABLE HERE]
- [ ] **C# / .NET-style second binding** OR deepen the existing script VM toward GDScript-grade tooling
  (autocomplete data, doc tooltips, live debug protocol). [VERIFIABLE HERE]
- [ ] **Stable C-ABI extension interface** (a GDExtension analog) so native modules load without
  recompiling the engine. [VERIFIABLE HERE]

### §2 Platforms & export — mixed
- [ ] **Desktop export/packaging** — extend `tools/package.sh` into a real per-OS bundler
  (assets + launcher + config). [VERIFIABLE HERE] (the packaging logic; running the packaged game is manual)
- [ ] **Web/WASM build target** via Emscripten for the headless/logic core. [CODE HERE / SEE IT ON YOUR MACHINE]
  (needs Emscripten present to actually emit `.wasm`)
- [ ] **Android / iOS export** — build scripts + input/sensor shims. [NEEDS YOUR HARDWARE/TOOLCHAIN]
  (Android SDK/NDK, Xcode, devices)
- [ ] **Console export** — [NEEDS YOUR HARDWARE/TOOLCHAIN] (NDA SDKs; cannot be done in a public sandbox)
- [ ] **XR/OpenXR** — runtime bindings can be stubbed; real use is [NEEDS YOUR HARDWARE/TOOLCHAIN]

### §5 High-end 3D rendering — [CODE HERE / SEE IT ON YOUR MACHINE]
All of these need a live GPU to *see*, but the CPU-side data structures, bakers, and math are testable.
- [ ] **Lightmap baker** (CPU raytraced GI into a texture atlas) — the bake is pure CPU + testable;
  sampling it is GPU-side. [CODE HERE / SEE IT ON YOUR MACHINE]
- [ ] **Reflection-probe capture plumbing**, **decal projection math**, **volumetric-fog params**,
  **SSR/SSIL passes**, **GPU particles + collision**, **occlusion culling**, **mesh LOD selection**.
  Shaders/passes written & compiled here; visual confirmation is on your GPU. [CODE HERE / SEE IT ON YOUR MACHINE]
- [ ] **3D navigation server** with runtime navmesh baking + dynamic obstacles — the baker/query is
  [VERIFIABLE HERE]; only the debug draw is GPU-side.

### §3 Editor as an application — [CODE HERE / SEE IT ON YOUR MACHINE]
The editor *logic* already exists (`maz/editor/`). Turning it into a running GUI app (dockable panels,
live viewport, visual shader/animation/theme editors, debugger GUI) requires a GPU to render the editor
itself, so it is written here and run on your machine. [CODE HERE / SEE IT ON YOUR MACHINE]

### §1 Foundational — the honest core
- [ ] **Renderer proven on real hardware** — [NEEDS YOUR HARDWARE/TOOLCHAIN] (a GPU). This is the one
  gap only your machine can close; the code exists and is what everything in §5/§3 builds on.
- [ ] **Audio device output path** — SDL audio callback wiring is [CODE HERE / SEE IT ON YOUR MACHINE]
  (compiles here; a speaker confirms it).
- [ ] **Networking transport** — real UDP/TCP/ENet-style sockets over `maz/net/`. The framing/reliability
  logic is [VERIFIABLE HERE] with loopback tests; true cross-machine play is manual.

## How the loop uses this

Each iteration: pick the next unchecked **[VERIFIABLE HERE]** item first (highest confidence, real
verification), implement it, unit-test it to green, document it, commit, and check it off here. Items in
the other two tiers are taken when they are the best remaining value, and are shipped with an explicit
"written / not verified here" note so nothing is over-claimed. The ecosystem section (§8) is skipped by
request.
