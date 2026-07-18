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
- [~] **[CPU]** Mesh **LOD** selection + [GPU] auto-LOD generation
  — **LOD selection done** (M231): `render::LodChain` — screen-coverage LOD pick (projected pixel
  size vs per-level thresholds), lod_bias, cull-below-last, and switch hysteresis. [GPU] auto-LOD
  mesh *generation* (decimation) remains.
- [x] **[CPU]** Occlusion culling — **software occluder buffer done** (M234): `render::OcclusionBuffer`
  — conservative coarse depth grid (occluders write their farthest depth; a candidate is culled only
  when every covered cell is solid AND its nearest point lies at/behind that depth, so nothing visible
  is ever hidden), plus `projectAabb` (world AABB → screen rect + depth range through a view-proj).
  This is the CPU technique behind Godot's `OccluderInstance3D`. [GPU] HW occlusion queries remain.
- [ ] **[GPU]** VMA (Vulkan Memory Allocator) — replace manual allocations
- [ ] **[CPU]** Compressed textures **KTX2 container** — ✅ parsing done (M209); [GPU] transcode+upload remain
- [ ] **[GPU]** Anisotropic filtering, back-face-cull toggle, multiple/sub viewports, render-to-viewport

### 4. Shading / materials authoring  [BIG]
- [ ] **[BIG]** A shading language + compiler (Godot shader language → SPIR-V). *Parser/AST is [CPU].*
- [ ] **[GPU]** Visual shader graph (needs editor + the above)
- [ ] **[GPU]** Standard material feature parity (clearcoat, anisotropy, SSS, refraction, proximity fade)
- [ ] **[CPU]** SDF/MSDF font **generation** — ✅ done (M203); [GPU] sampling shader remains

### 5. Text & internationalization  [BIG]
- [~] **[BIG][CPU]** TextServer: complex-script shaping (HarfBuzz-class), BiDi, line breaking for
  CJK/Arabic/Indic. *Almost entirely CPU — a large but verifiable effort.*
  — **analysis subset done** (M247): `ui::TextServer` — UTF-8 decode, base-direction detection (UAX #9
  P2/P3, first strong char), bidirectional run segmentation (mixed LTR/RTL → runs; neutrals inherit),
  and line-break opportunities (UAX #14 subset: after spaces/hyphens, mandatory at newlines, between
  CJK ideographs, collapsing space runs). Verified across ASCII/Hebrew/CJK/emoji. The [BIG] pieces —
  complex-script *shaping* (HarfBuzz-class glyph sub/positioning) and full UAX #9 (embeddings/isolates,
  weak-type resolution) — remain.
- [x] **[CPU]** Translation/PO catalogs: `io::PoCatalog` + `io::PluralRule` — gettext PO parser
  (msgctxt/msgid/msgid_plural/msgstr[n], multi-line, escapes) with gettext/ngettext/pgettext/
  npgettext and a per-language plural-rule evaluator (the C subset: n, %*/+-, comparisons, && || !,
  ?:). M226. Correct pluralization for English/Polish/etc. beyond Maz's CSV tables.
- [ ] **[GPU]** SDF font rendering at draw time

### 6. Editor breadth  [BIG][GPU][DESK]
Maz has a minimal editor; Godot's is vast.
- [ ] **[GPU]** Dockable multi-panel editor shell (Dear ImGui or custom) — *layout logic [CPU]*
- [ ] **[GPU]** Dedicated editors: animation, tilemap/tileset, shader, particles, theme, navmesh bake
- [ ] **[GPU]** Full gizmo set, snapping, multi-viewport, 2D+3D edit modes
- [~] **[CPU]** Import dock + `.import` sidecar pipeline (Maz has reimport core; add settings UI/model)
  — **sidecar model done** (M237): `io::ImportFile` parses/encodes Godot's `.import` format
  ([remap] importer/type/uid/path, [deps] source_file/dest_files array, [params] options) with a clean
  round-trip; `io::ImportFile::cookedPath` builds the `res://.godot/imported/<base>-<hash>.<ext>`
  convention; `io::contentHashHex` (FNV-1a) + `io::ImportDatabase` answer "needs reimport?" from a
  source content hash (unknown → import, unchanged → skip, changed → reimport). The [GPU] import *dock
  UI* still remains.
- [ ] **[DESK]** Remote debugger / live scene inspection

### 7. Node & scene-system breadth  [CPU mostly]
Godot ships ~200 node types. Maz has the spine + many. Concrete missing high-value nodes:
- [~] **[CPU]** CanvasLayer, ParallaxLayer node, Path2D/PathFollow2D, RemoteTransform, VisibleOnScreenNotifier
  — **PathFollow2D done** (M221): `game::PathFollow2D` walks a Curve2D by progress/progress-ratio,
  loop-or-clamp ends, hOffset along the path normal, tangent-following rotation.
  — **VisibleOnScreenNotifier2D done** (M223): `game::VisibleOnScreenNotifier2D` fires screen
  entered/exited edge events as an object's rect crosses the camera view. CanvasLayer/RemoteTransform remain.
- [~] **[CPU]** Timer, Tween node, AnimationPlayer node wrapper, Marker2D/3D
  — **Timer done** (M222): `game::Timer` countdown with wait_time, one_shot/repeating (remainder-
  carrying so cadence never drifts), pause/stop/restart, start(override), timeout callback. Others remain.
- [~] **[CPU]** GridMap (3D tile map) — **data model done** (M224): `game::GridMap` sparse cell→
  (tileId, orientation) store, set/clear/has/query, occupied-bounds, world↔cell floor-div mapping,
  packed signed 64-bit keys. [GPU] mesh-library instancing render deferred until there's a display.
- [x] **[CPU]** CSG (constructive solid geometry) mesh ops — *pure mesh boolean math is [CPU]*
  — **boolean core done** (M238): `game::Csg` — signed-distance-field CSG (Godot's CSGCombiner3D
  union/intersection/subtraction semantics). Primitives (sphere, box, rounded box, plane half-space,
  cylinder), the three booleans (union=min, intersect=max, subtract=max(a,-b)) plus smooth/rounded
  variants, translate/scale transforms, `inside()` containment, and `sdfNormal()` gradient normals.
  Exactly unit-tested by sampling distances (analytic sphere/box distances, shell subtraction,
  fillet dip, radial normals). **Mesh output done** (M239): `render::surfaceNets` (Naive Surface Nets)
  turns any CSG field into a watertight triangle mesh with per-vertex normals — one vertex per
  surface-crossing cell, quads across sign-flipping edges. Verified: every meshed vertex lies on the
  isosurface (within a cell), correct ring radius/centroid, outward normals, no orphan vertices, and a
  sphere-minus-box CSG meshing into one connected surface. [GPU] upload of the resulting buffers is the
  usual mesh path.
- [x] **[CPU]** MultiplayerSpawner/Synchronizer scene nodes (needs networking, below)
  — **done** (M243): `net::MultiplayerSpawner` — Godot's MultiplayerSpawner. The authority assigns a
  network id per spawn (scene-type tag + args), queues spawn/despawn events, and serializes them to a
  BitStream (`writeEvents`); remotes apply via `onSpawn`/`onDespawn` and mirror the live set. `writeFull`/
  `readFull` give a late joiner a one-shot keyframe of the whole world; replay is idempotent (already-live
  ids don't double-spawn) and malformed streams are rejected. Pairs with `net::Synchronizer` (M218,
  MultiplayerSynchronizer) for per-node property sync thereafter. Verified end-to-end (event replication,
  args round-trip, late-join snapshot, idempotence, truncation). [DESK] real socket transport remains.

### 8. UI (Control) library  [CPU mostly]
Maz has a strong slice (LayoutNode, containers, Tree, ItemList, PopupMenu, TextField, StyleBox,
Theme, Range/ProgressBar, nine-patch, BBCode). Missing vs Godot:
- [~] **[CPU]** TabContainer, GraphEdit/GraphNode, RichTextLabel effects, FileDialog, ColorPicker,
  SpinBox, OptionButton, Tree editing, drag-and-drop between controls
  — **GraphEdit/GraphNode done** (M241): `ui::GraphEdit` — the node-graph model behind visual
  scripting / the shader graph / blend trees. Nodes with named input/output ports + canvas position;
  connect/disconnect with full validation (endpoints exist, no self-links, no duplicate wires,
  cycle rejection for acyclic graphs), many-to-one/one-to-many wiring, `wouldCreateCycle`, incident-wire
  cleanup on node removal, and a Kahn `topologicalOrder` (empty on cycle). Verified across all of those.
  **ColorPicker colour math done** (M242): `render::ColorOps` — HSV<->RGB, hex `#rrggbb`/`#rrggbbaa`
  (+shorthand) parse/format, lighten/darken/lerp/invert, Rec.709 luminance, and sRGB<->linear transfer
  functions, all verified against known colour identities. **SpinBox / OptionButton / TabBar done**
  (M244): `ui::SpinBox` (a Range with step buttons + prefix/suffix text format/parse), `ui::OptionButton`
  (drop-down list with selected item, id lookup, disabled rejection, auto-select-first), and `ui::TabBar`
  (ordered tab strip with current tracking, disabled-skipping next/previous nav, removal that clamps
  current). Verified across clamp/snap, selection, and navigation edge cases. **drag-and-drop done**
  (M249): `ui::DragAndDrop` — Godot's Control drag/drop coordinator (get_drag_data / can_drop_data /
  drop_data): begin a drag with a typed payload, poll hover targets for acceptance, deliver to an
  accepting target on release (else keep/cancel), with single-drag and drop-when-idle guards.
  **ColorPicker widget done** (M251): `ui::ColorPicker` — the interactive model behind Godot's
  ColorPicker (colour math was M242). Stores H/S/V/A as the source of truth so the hue survives a
  value/saturation drag to an extreme (setColor to black keeps hue+saturation, to grey keeps hue),
  matching Godot; derives RGB/hex on demand. Carries the picker's editing state: RGB-channel setters,
  hex I/O (`#` optional in, none out; alpha byte gated by an edit-alpha toggle), slider mode
  (RGB/HSV/RAW with RAW allowing HDR >1), user preset swatches (dedup-to-end + erase), and a capped
  most-recent list (front-inserted, deduped). Verified across hue-preservation, round-trips, and
  preset/recent edge cases. The TabContainer / FileDialog *widgets* remain.
- [x] **[CPU]** Full theme system (per-control theme overrides, theme types)
  (M250): extended `ui::Theme` with **theme type variations** (Godot's `theme_type_variation` /
  theme type inheritance) — `setTypeVariation(type, base)` chains a variation onto a base type, and
  typed lookups `themeColor` / `themeStyleBox` / `themeConstant` (+ `hasTheme*`) walk that chain
  (multi-level, cycle-safe via a visited set) before falling back. Added a numeric **constants**
  item class alongside colours/styleboxes. Plus **per-control theme overrides** (`ui::ThemeOverrides`
  — Godot's `add_theme_color_override` / `add_theme_stylebox_override` / …) and free `resolveColor` /
  `resolveStyleBox` / `resolveConstant` that give a local override precedence over the theme chain,
  with clear-to-restore. The old freeform-key `Theme` API (M109) is untouched.
- [ ] **[GPU]** Control clipping via viewport/backbuffer

### 9. Physics remaining  [CPU/BIG]
Maz's 2D is at parity; 3D is strong but not exhaustive.
- [~] **[CPU]** 3D convex-hull + trimesh (concave static) colliders, height-field collider
  — **convex hull done** (M227): `game::buildConvexHull` incremental hull of a point cloud →
  outward-wound triangular faces + hull vertex set (ConvexPolygonShape3D geometry).
  — **height-field done** (M228): `game::HeightField3D` — grid of height samples with bilinear
  heightAt/normalAt and a grid-DDA + Möller–Trumbore raycast (HeightMapShape3D).
  — **trimesh done** (M229): `game::TriMesh3D` — concave triangle-soup static collider with a
  BVH-accelerated nearest raycast (ConcavePolygonShape3D). All three 3D collider shapes now covered.
- [~] **[CPU]** 3D more joints (cone-twist, 6DOF, slider, generic), soft bodies, ragdolls
  — **slider done** (M230): `Joint3D::Slider` / `makeSliderJoint3` — prismatic joint locking the
  2 perpendicular linear + 2 perpendicular angular DOF, leaving slide+spin along one axis free
  (SliderJoint3D). **cone-twist done** (M235): `Joint3D::ConeTwist` / `makeConeTwistJoint3` —
  point-to-point pin plus a unilateral swing cone (b's twist axis held within `swingSpan` of a's
  cone axis) and a unilateral twist limit (rotation about the axis capped at +/- `twistSpan`), the
  ragdoll-limb joint (Godot ConeTwistJoint3D). Verified: a pendulum caught exactly at a 30-degree
  cone stop, a 170-degree cone swinging freely (no false constraint), and a spun body caught at its
  twist limit. **soft bodies done** (M246): `game::SoftBody` — Godot's SoftBody3D via Position-Based
  Dynamics (Verlet particles + distance constraints + pins), unconditionally stable. Verified: a
  stretched link relaxes to rest, a pinned link/rope hangs under gravity without over-stretching (<5%),
  pins stay fixed, and energy stays bounded over thousands of steps. 6DOF / generic joints and ragdolls
  (a skeleton driven by cone-twist joints) remain; soft-body-vs-rigid collision is a follow-up.
  **ragdoll done** (M248): `game::buildRagdoll` — Godot's PhysicalBone3D ragdoll: turns a bone list
  (segment + parent + per-joint swing/twist limits) into capsule bodies oriented along each bone, tied
  to their parents by cone-twist joints at the shared joint. Verified: bodies/joints created, capsules
  oriented along their bones, joint anchors coincident, a pinned chain stays connected while it settles
  (worst gap <0.1), and a free ragdoll dropped on the ground falls, lands above the floor, and stays
  connected. 6DOF / generic joints remain.
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
- [x] **[CPU]** Connection / packet framing: `net::Connection` (protocol-id + seq/ack/ackBits
  header over net::BitStream; pack payload → bytes, unpack bytes → payload + resolve acks +
  reject foreign/truncated). M219. The framing directly beneath a real UDP socket.
- [ ] **[DESK]** UDP socket binding (SDL_net or BSD sockets) wiring net::Connection to real
  datagrams — the one part that needs actual sockets (not unit-testable headless; the framing above is)
- [x] **[CPU]** Snapshot/delta replication: `net::Snapshot` (per-field-bit-width schema; full +
  changed-mask delta encode/decode over net::BitStream). M214. Only changed fields cross the wire.
- [x] **[CPU]** Interpolation buffer: `net::InterpolationBuffer` (time-ordered sample history;
  lerp the bracketing pair at a delayed render time; bounded velocity extrapolation for late
  packets). M215. Valve-style entity interpolation — smooth motion from discrete ticks.
- [x] **[CPU]** Client-side prediction + server reconciliation: `net::PredictionBuffer` (apply
  inputs locally + immediately; on an authoritative snapshot, drop acked inputs, snap to server
  state, re-simulate the unacked ones). M216. Valve/Gaffer prediction — instant-feeling netcode.
- [x] **[CPU]** RPC layer: `net::RpcDispatcher` (name-hashed method ids, reliable/unreliable/
  ordered transfer modes, header write + dispatch-to-handler over net::BitStream; unknown/malformed
  calls counted, not crashed). M217. Godot's @rpc / rpc()/rpc_id() model.
- [x] **[CPU]** Scene-replication nodes: `net::Replication` (`ReplicatedObject` declares get/set
  properties with wire widths; `Synchronizer` holds the per-peer baseline and does writeFull/
  writeDelta/readFull/readDelta over net::Snapshot). M218. Godot's MultiplayerSynchronizer.
- [x] **[CPU]** Network-condition simulator: `net::NetSim` (seeded-deterministic latency/jitter/
  loss/duplication queue; send/receive by caller time). M220. The "bad network" test harness that
  validates the ack/interpolation/prediction layers — beyond what Godot ships built-in.
- [ ] **[DESK]** WebSocket + WebRTC data channels (browser/native transports — need real sockets)

### 11. XR / VR  [GPU][DESK][BIG]
- [ ] **[GPU]** OpenXR integration, stereo rendering, XR controllers/hands

### 12. Export / platforms  [BIG][DESK]
Godot one-click exports to Win/mac/Linux/Web/Android/iOS/consoles. Maz builds native + has a
Windows editor download.
- [ ] **[DESK]** macOS + Linux packaged builds (extend the existing package pipeline)
- [ ] **[BIG]** Web export (Emscripten + WebGL/WebGPU) — depends on a GL/GPU backend
- [ ] **[BIG]** Android + iOS export
- [x] **[CPU]** Export templates + a project/export config model — `io::ExportConfig` /
  `io::ExportPreset` (M232): per-platform presets with feature tags + include/exclude glob filters
  and an `includes(path)` decision (Godot export presets), on a reusable `globMatch` core.

### 13. Asset import breadth  [CPU mostly]
- [~] **[CPU]** FBX, OBJ, Collada importers (Maz has glTF); [CPU] image formats beyond PNG/JPEG (WebP, HDR/EXR)
  — **OBJ done** (M225): `render::parseObj`/`loadObj` — Wavefront v/vt/vn/f, 1-based + negative
  indices, v//vn form, per-combo vertex dedup, n-gon fan triangulation → MeshData. FBX/Collada remain.
  — **HDR (Radiance RGBE) done** (M233): `io::decodeHdr` — .hdr header + new-RLE + raw scanlines →
  linear float RGB (skybox/IBL source). WebP/EXR remain.
- [ ] **[CPU]** OGG Vorbis / MP3 audio decode (Maz has WAV)
- [~] **[CPU]** Font: OTF/collection support, dynamic font sizing cache
  — **dynamic sizing cache done** (M240): `ui::GlyphCache` — the size-keyed glyph atlas Godot's
  FontFile keeps for dynamic fonts. Keys by font+codepoint+pixel-size, rasterizes each glyph once (via
  an injected rasterizer) into the skyline-packed atlas, returns the atlas rect + metrics, and
  evicts-all + repacks when the page fills. Verified: miss→rasterize / hit→cached, distinct entries per
  size, non-overlapping in-bounds packing, overflow eviction that keeps serving, and clear→re-raster.
  OTF/font-collection *parsing* remains (the current rasterizer is stb_truetype/TTF).
- [ ] **[GPU]** Video playback (Theora/WebM)

### 14. Scripting ecosystem  [CPU/BIG]
Maz has its own VM. Godot has GDScript + C# + GDExtension (native plugins).
- [~] **[CPU]** GDExtension-style C ABI so third parties add engine modules without recompiling
  — **ABI core done** (M245): `ext::ExtensionRegistry` — a stable, C-compatible interface (a tagged
  `ExtVariant` + plain function pointers, no std types in the payload) for registering classes + methods
  from a separate module, instantiating them, and dispatching calls by name. Includes ABI version
  negotiation (`abiCompatible` — major must match, plugin minor ≤ host) and a `loadExtension` entry-point
  that rejects incompatible plugins up front. Verified with a mock in-process "Counter" extension:
  register/instantiate/dispatch (with args + return), unknown-method/class → ok=false, duplicate/invalid
  registration rejection, and unregister. The real `dlopen`/`LoadLibrary` of a `.so`/`.dll` remains [DESK].
- [x] **[CPU]** Debugger protocol (breakpoints, step, variable inspection) for the maz::script VM
  — **done** (M236): `script::Debugger` drives the VM's per-statement hook and adds line breakpoints,
  the four stepping modes (into / over / out / continue) resolved from call-stack depth, a call-stack
  snapshot at each stop, and paused-frame variable inspection (`locals()` / `resolve()` /
  `valueString()`). Execution pauses synchronously via an `onPause` handler that returns the next step
  mode. Verified: breakpoint + local inspection (a/b/s inside a function with the right call stack),
  step-into descending into a call, step-over skipping a body, step-out returning to the caller,
  break-at-entry single-stepping, and zero pauses when nothing is armed. Wiring this to a remote IDE
  over a socket is the [DESK] transport on top; the decision + inspection core is complete.
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
