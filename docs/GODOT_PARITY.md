# Maz vs Godot — complete parity plan

The goal: make the Maz Engine **equal to or better than Godot in every way**. This is the honest,
complete list of what that takes — what already matches or beats Godot, every remaining gap, and a
phased plan to close them. Each gap is tagged with how it can be verified in this project:

- **[CPU]** — has a pure, deterministic core we can implement and unit-test with no GPU (done here).
- **[GPU]** — needs a real graphics device to build/see; buildable in CI, visually verified only on
  real hardware.
- **[DESK]** — needs a real desktop OS surface (window manager, drivers).
- **[BIG]** — a large multi-milestone subsystem, not a single task.

> **Honest stance:** Maz is a from-scratch native C++20 engine. In the systems that are pure logic
> (math, physics solvers, animation, audio DSP, AI/pathfinding, ECS, serialization, tooling) it is
> already at or beyond Godot parity and fully unit-tested. Godot's remaining structural advantages
> are almost all **GPU-heavy rendering** and **breadth of platform/editor/ecosystem** — years of
> work by a large team. This plan does not pretend those are done; it sequences them.

---

## Part A — Where Maz already meets or beats Godot

These are implemented and tested in-tree (see `docs/ROADMAP.md` for the Mxxx milestone tags):

- **Core / math / containers:** vectors/matrices/quats, Transform2D/3D, Rect2, Geometry2D/3D,
  curves, easing, two RNGs (xoshiro + PCG32), SlotMap, SmallVector/SparseSet, RingBuffer, string
  interning, reflection, JSON/CSV/XML/base64/INI, binary + text serialization, resource packs,
  virtual filesystem, semver, deterministic time, replay, checkpoints, profiler, **performance
  budgets** (a formal alert layer Godot lacks), job system.
- **2D:** sprites/atlas/tilemaps, cameras, parallax, polygons (convex + concave ear-clip),
  polylines, multimesh, 2D lights + hard/soft/normal-mapped shadows, additive blending.
- **3D rendering (CPU-verifiable parts):** PBR (Cook-Torrance GGX, metallic/roughness), analytic
  IBL, tonemap operators incl. **AgX**, bloom, SSAO, MSAA, fog, shadow mapping w/ PCF, frustum
  culling, instancing, billboards, procedural primitives, glTF scenes.
- **Physics:** full 2D solver (warm-started impulse, islands/sleeping, joints+motors, CCD, materials,
  many shapes) and a from-scratch **3D** solver (OBB SAT, capsules, SAP broadphase, character
  controller, joints). Parity-or-better with Godot's built-in 2D and competitive with Godot Physics
  3D.
- **Audio:** mixer, buses/bus graph, full DSP suite (EQ, reverb, delay, chorus/flanger/phaser,
  distortion modes, compressor, limiter), spatial 2D/3D, synth/oscillator, spectrum/FFT, WAV.
- **Animation:** skeletons + GPU-ready skinning, clips, blend spaces/trees, state machines,
  timelines, IK (2-bone, FABRIK), root motion, tweening.
- **AI/nav:** A* grid + AStar2D, navmesh, steering, flow fields, RVO avoidance, behavior trees +
  blackboard, GOAP planner.
- **Gameplay/scene:** ECS, SceneTree/Node2D, prefabs, groups, signals, scene serialization,
  a scripting VM (lexer→bytecode→GC, classes, closures, modules, hot reload, gradual typing).
- **Tooling:** in-engine editor (viewport, gizmos, inspector, undo/redo, save/load, asset browser,
  play-in-editor), CI (Linux/macOS/Windows), sanitizers, clang-tidy, golden-image tests, packaging,
  API docs, crash handler, telemetry, **a one-click Windows editor download**.

---

## Part B — The complete gap list (what Godot still has that Maz doesn't)

### 1. Rendering backends & platforms  [BIG]
Godot runs on Vulkan, D3D12, Metal, and OpenGL3 (desktop + web + mobile). Maz is Vulkan-only.
- [ ] **[GPU]** D3D12 backend (Windows-native)
- [ ] **[GPU]** Metal backend (macOS/iOS; today via MoltenVK only)
- [ ] **[GPU]** OpenGL ES 3 / WebGL2 "compatibility" backend
- [ ] **[BIG]** A Rendering Device abstraction so backends are pluggable (Maz already has a
  `Renderer` interface — this widens it to RDD level). *Design/interface work is [CPU]-reviewable.*

### 2. Global illumination & advanced lighting  [GPU][BIG]
Godot's headline 3D feature set. Maz has analytic IBL + shadow maps only.
- [ ] **[GPU]** SDFGI (signed-distance-field real-time GI)
- [ ] **[GPU]** VoxelGI
- [ ] **[GPU]** LightmapGI (baked) — *the lightmap UV-unwrap + bake math has [CPU] pieces*
- [ ] **[GPU]** Reflection probes (baked + real-time)
- [ ] **[GPU]** Volumetric fog / god rays
- [ ] **[GPU]** Screen-space reflections (SSR), SSIL
- [ ] **[CPU]** Cascaded shadow-map **split math** — ✅ done (M211); GPU passes remain
- [ ] **[GPU]** Point-light cube shadows

### 3. Rendering pipeline depth  [GPU]
- [ ] **[GPU]** Clustered Forward+ / deferred path (many lights)
- [ ] **[GPU]** TAA + FSR/AMD upscaling; FXAA
- [ ] **[GPU]** GPU-driven particles (Maz particles are CPU) + particle collision/attractors on GPU
- [ ] **[GPU]** Decals
- [ ] **[CPU]** Mesh **LOD** selection + [GPU] auto-LOD generation
- [ ] **[CPU]** Occlusion culling (portal/occluder math is CPU) + [GPU] HW occlusion queries
- [ ] **[GPU]** VMA (Vulkan Memory Allocator) — replace manual allocations
- [ ] **[CPU]** Compressed textures **KTX2 container** — ✅ parsing done (M209); [GPU] transcode+upload remain
- [ ] **[GPU]** Anisotropic filtering, back-face-cull toggle, multiple/sub viewports, render-to-viewport

### 4. Shading / materials authoring  [BIG]
- [ ] **[BIG]** A shading language + compiler (Godot shader language → SPIR-V). *Parser/AST is [CPU].*
- [ ] **[GPU]** Visual shader graph (needs editor + the above)
- [ ] **[GPU]** Standard material feature parity (clearcoat, anisotropy, SSS, refraction, proximity fade)
- [ ] **[CPU]** SDF/MSDF font **generation** — ✅ done (M203); [GPU] sampling shader remains

### 5. Text & internationalization  [BIG]
- [ ] **[BIG][CPU]** TextServer: complex-script shaping (HarfBuzz-class), BiDi, line breaking for
  CJK/Arabic/Indic. *Almost entirely CPU — a large but verifiable effort.*
- [ ] **[CPU]** Translation/PO catalogs (Maz has CSV tables; add gettext PO + pluralization)
- [ ] **[GPU]** SDF font rendering at draw time

### 6. Editor breadth  [BIG][GPU][DESK]
Maz has a minimal editor; Godot's is vast.
- [ ] **[GPU]** Dockable multi-panel editor shell (Dear ImGui or custom) — *layout logic [CPU]*
- [ ] **[GPU]** Dedicated editors: animation, tilemap/tileset, shader, particles, theme, navmesh bake
- [ ] **[GPU]** Full gizmo set, snapping, multi-viewport, 2D+3D edit modes
- [ ] **[CPU]** Import dock + `.import` sidecar pipeline (Maz has reimport core; add settings UI/model)
- [ ] **[DESK]** Remote debugger / live scene inspection

### 7. Node & scene-system breadth  [CPU mostly]
Godot ships ~200 node types. Maz has the spine + many. Concrete missing high-value nodes:
- [ ] **[CPU]** CanvasLayer, ParallaxLayer node, Path2D/PathFollow2D, RemoteTransform, VisibleOnScreenNotifier
- [ ] **[CPU]** Timer, Tween node, AnimationPlayer node wrapper, Marker2D/3D
- [ ] **[CPU]** GridMap (3D tile map) — *data model [CPU]*, [GPU] render
- [ ] **[CPU]** CSG (constructive solid geometry) mesh ops — *pure mesh boolean math is [CPU]*
- [ ] **[CPU]** MultiplayerSpawner/Synchronizer scene nodes (needs networking, below)

### 8. UI (Control) library  [CPU mostly]
Maz has a strong slice (LayoutNode, containers, Tree, ItemList, PopupMenu, TextField, StyleBox,
Theme, Range/ProgressBar, nine-patch, BBCode). Missing vs Godot:
- [ ] **[CPU]** TabContainer, GraphEdit/GraphNode, RichTextLabel effects, FileDialog, ColorPicker,
  SpinBox, OptionButton, Tree editing, drag-and-drop between controls
- [ ] **[CPU]** Full theme system (per-control theme overrides, theme types)
- [ ] **[GPU]** Control clipping via viewport/backbuffer

### 9. Physics remaining  [CPU/BIG]
Maz's 2D is at parity; 3D is strong but not exhaustive.
- [ ] **[CPU]** 3D convex-hull + trimesh (concave static) colliders, height-field collider
- [ ] **[CPU]** 3D more joints (cone-twist, 6DOF, slider, generic), soft bodies, ragdolls
- [ ] **[CPU]** Cross-engine determinism audit vs Godot Jolt (fixed-point optional)

### 10. Networking / multiplayer  [BIG][CPU]
Godot has high-level multiplayer (RPC, MultiplayerSynchronizer, ENet/WebRTC/WebSocket). Maz has
**none** today. This is a whole subsystem — and almost entirely CPU-testable.
- [x] **[CPU]** Wire format: `net::BitStream` (BitWriter/BitReader) — bit-packed packet
  serialization (N-bit ints, quantizable floats, signed sign-extension, byte arrays, align,
  underflow-safe). M212. The compact encoding replication/RPC ride on.
- [x] **[CPU]** Reliability/ack layer: `net::Reliability` (seqGreaterThan wraparound comparator,
  AckReceiver producing ack + 32-bit ack-bitfield, AckSender resolving in-flight → newly-acked).
  M213. The reliable-over-UDP core (Fiedler/ENet model) — RTT + resend basis.
- [ ] **[CPU]** UDP transport binding (SDL_net or BSD sockets) wiring the above to real packets
- [x] **[CPU]** Snapshot/delta replication: `net::Snapshot` (per-field-bit-width schema; full +
  changed-mask delta encode/decode over net::BitStream). M214. Only changed fields cross the wire.
- [ ] **[CPU]** Interpolation buffer, client-side prediction + reconciliation
- [ ] **[CPU]** RPC layer + scene-replication nodes
- [ ] **[CPU]** WebSocket + WebRTC data channels

### 11. XR / VR  [GPU][DESK][BIG]
- [ ] **[GPU]** OpenXR integration, stereo rendering, XR controllers/hands

### 12. Export / platforms  [BIG][DESK]
Godot one-click exports to Win/mac/Linux/Web/Android/iOS/consoles. Maz builds native + has a
Windows editor download.
- [ ] **[DESK]** macOS + Linux packaged builds (extend the existing package pipeline)
- [ ] **[BIG]** Web export (Emscripten + WebGL/WebGPU) — depends on a GL/GPU backend
- [ ] **[BIG]** Android + iOS export
- [ ] **[CPU]** Export templates + a project/export config model

### 13. Asset import breadth  [CPU mostly]
- [ ] **[CPU]** FBX, OBJ, Collada importers (Maz has glTF); [CPU] image formats beyond PNG/JPEG (WebP, HDR/EXR)
- [ ] **[CPU]** OGG Vorbis / MP3 audio decode (Maz has WAV)
- [ ] **[CPU]** Font: OTF/collection support, dynamic font sizing cache
- [ ] **[GPU]** Video playback (Theora/WebM)

### 14. Scripting ecosystem  [CPU/BIG]
Maz has its own VM. Godot has GDScript + C# + GDExtension (native plugins).
- [ ] **[CPU]** GDExtension-style C ABI so third parties add engine modules without recompiling
- [ ] **[CPU]** Debugger protocol (breakpoints, step, variable inspection) for the maz::script VM
- [ ] **[CPU]** Optional C# / other language hosting (large)

---

## Part C — Phased execution plan

Ordered so each phase is buildable and mostly verifiable, front-loading CPU work that lands now and
sequencing GPU/desktop work for a real machine.

- **Phase α — finish every CPU-verifiable slice (in progress, this loop).** Cascade splits ✅, KTX2 ✅,
  present-mode ✅, focus/DPI/multi-monitor ✅, text/clipboard ✅. Remaining CPU slices: LOD selection,
  occlusion math, CSG boolean math, GridMap data model, more UI controls, more importers (OBJ/OGG),
  PO localization, TextServer line-breaking, the shading-language **parser/AST**.
- **Phase β — networking (whole new subsystem, ~all CPU).** Transport → replication → prediction →
  RPC → scene nodes. Biggest single parity win that needs no GPU. Fully unit-testable.
- **Phase γ — rendering-device abstraction + Forward+ (GPU, on real hardware).** Widen `Renderer`
  to a device layer, then clustered Forward+, GPU particles, decals, SSR, TAA/FSR, VMA.
- **Phase δ — global illumination (GPU).** SDFGI → reflection probes → volumetric fog → LightmapGI.
- **Phase ε — editor breadth + shading language + visual shaders (GPU + desktop).**
- **Phase ζ — export/platform matrix (desktop + web + mobile).**

---

## Part D — "Better than Godot" opportunities (not just parity)

Places Maz can exceed Godot rather than match it:

- **Performance budgets & CI-enforced regression gates** — already in (core::PerfBudget); Godot has
  no formal budget/alert layer. Extend to per-scene budgets asserted in CI.
- **Determinism-first simulation** — Maz already has deterministic RNG, replay, fixed-step. Push to
  full cross-platform lockstep determinism (a networking/e-sports advantage Godot lacks out of box).
- **Header-only, dependency-light core** — trivial embedding vs Godot's monolith.
- **Golden-image + sanitizer + clang-tidy gates on every push** — a stricter quality bar than
  upstream Godot CI on the engine core.
- **Planner-grade AI (GOAP)** built-in — beyond Godot's BT-only offering.
- **A formal, unit-tested audio DSP suite** with dB-correct units and lookahead limiter.

---

*This document is the master parity tracker. As milestones land, update the matching `[ ]`/`[~]`/`[x]`
here and in `docs/ROADMAP.md`. The one rule: never mark something done that can't be verified —
GPU/desktop items stay honest until they run on real hardware.*
