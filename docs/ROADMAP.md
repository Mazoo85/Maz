# Maz Engine — Roadmap ("the massive list")

A native **C++20 + Vulkan + SDL3** game engine, built **2D-first but architected so 3D drops
in later** (pluggable renderer, camera abstraction, scene graph).

This document is the master to-do list. It's organized into phases; **each bullet is a
buildable task**. Phases are ordered roughly by dependency, but many items inside a phase can be
done in parallel. Status legend: `[ ]` todo · `[~]` in progress · `[x]` done.

> **M0 — walking skeleton (done).** Window opens, fixed-timestep loop runs, Vulkan clears the
> screen, clean shutdown.
>
> **M1 — 2D sprite renderer (done).** Textured, tinted, rotated sprites via a batched Vulkan
> pipeline (buffers, staging uploads, descriptor sets, dynamic viewport/scissor, alpha blend);
> `Camera2D`; stb_image loading. Verified validation-clean end-to-end on a software Vulkan
> device (llvmpipe).
>
> **M2 — playable top-down demo (done).** A `Tilemap` module + a sandbox that renders a
> view-culled tile world, moves a player with WASD, blocks movement into solid tiles
> (axis-separated sliding), and follows the player with the camera.
>
> **M3 — text + HUD (done).** TrueType text via `stb_truetype` baked to an atlas (`ui::Font`,
> `drawText`), a bundled DejaVu Sans, and multi-camera batching so a pixel-space HUD (title,
> controls, animated health bar) draws over the world-space follow camera. Fixed the Vulkan
> clip-space Y orientation to true top-left/y-down.
>
> **M4 — a complete sample game (done).** `apps/orbs` ("ORB RUN") — an original, genre-neutral
> arcade game with a full game-state machine (title → play → win/lose → restart), player
> movement, collectible orbs, roving hazards, a countdown timer, and a HUD. Proves the engine
> ships a real title and runs more than one app.
>
> **M5 — audio (done).** `maz::audio::Audio` — an SDL3 audio device with a real-time mixer that
> synthesizes voices (sine/square/triangle/noise, attack/decay envelope, frequency glide) plus a
> looping arpeggio music bed; thread-safe `play`, master volume, graceful with no device. Wired
> into ORB RUN (start/pickup/hit/win SFX + music). Verified by capturing 3s of generated audio
> to a file and rendering its waveform.
>
> **M6 — particles (done).** `maz::fx::ParticleSystem` — a pooled 2D particle system (burst
> emitters with speed/angle/life/size ranges, start→end color, gravity, drag), drawn as
> size/alpha-faded sprites through the Renderer. Wired into ORB RUN: a player spark trail, pickup
> bursts, and win/lose bursts.
>
> **M7 — persistence (done).** `maz::core::KeyValueStore` (stdlib-only ini-style load/save) +
> `maz::platform::prefPath` (SDL user-data dir). ORB RUN now keeps a **high score across runs** —
> verified by writing a value from one process and loading it in another.
>
> **M8 — ECS (done).** `maz::ecs::World` — a lightweight entity-component system (entity
> free-list, type-erased sparse-set component pools, `add/get/has/remove`, `each<T>` and
> `view<A,B>` iteration). A third sample app, `apps/swarm`, runs 800 entities through movement +
> render systems.
>
> **M9 — 3D rendering (done).** Depth buffer added to the render pass; a `MeshRenderer` (indexed
> position/normal/color meshes, MVP+model push constants, directional lighting) behind a new
> Renderer 3D API (`createMesh` / `setViewProjection3D` / `drawMesh`); `math::perspective`
> camera. `apps/cube` spins a lit cube with 2D HUD text over it — 2D and 3D compose in one frame.
> The 2D path is unchanged (sprites disable depth).
>
> **M10 — 3D scene (done).** Procedural mesh primitives (`render::shapes` — box / sphere /
> plane) and `apps/scene3d`: a ground plane plus a ring of 20 spinning, lit spheres and cubes,
> each an **ECS entity** (Transform3D + Renderable), viewed by an orbiting camera with a 2D HUD —
> the 3D renderer, procedural geometry, and the ECS composed together.
>
> **M11 — textured 3D (done).** A shared `TextureStore` (one descriptor layout/pool) now backs
> both the sprite and mesh renderers, so a texture handle works in 2D or 3D. Meshes gained UVs
> and a sampler; the mesh shader does texture × vertex-color × lighting. `apps/cube` is now a
> checkerboard cube and `apps/scene3d` has a tiled floor. The 2D games are unchanged (verified).
>
> **M12 — explorable 3D (done).** `maz::game::FlyCamera` — a first-person camera (position +
> yaw/pitch, `move`/`look`, view matrix) + `Window::setRelativeMouse` for mouse-look. `apps/world`
> is a navigable field of 3D blocks over a textured floor (WASD + mouse-look; `--demo` autopilots
> a fly-through).
>
> **M13 — 3D collision (done).** `maz::game::Collision` — `Aabb` + `slideMove` (per-axis resolve
> so you slide along surfaces). `apps/world` is now solid and walkable, turned into a
> first-person collect-em-up (glowing pickups + a HUD counter; autopilot steers around blocks it
> bumps).
>
> **M14 — shadows (done).** Shadow mapping in the `MeshRenderer`: a depth-only pass renders
> casters from a directional light into a 2048² shadow map, sampled with 2×2 PCF in the mesh
> shader. The frame runs shadow-pass → main-pass; 2D games skip the shadow pass and are
> unaffected.
>
> **M15 — skybox (done).** A gradient sky drawn behind the 3D scene: a fullscreen pass
> reconstructs the per-pixel view ray from the inverse view-projection and shades a
> zenith→horizon→ground gradient with a sun glow. Drawn automatically for any 3D scene (no meshes
> queued → no sky), so 2D is unaffected.
>
> **M16 — MSAA (done).** Multisample anti-aliasing smooths jagged edges everywhere, 2D and 3D.
> The swapchain picks the best supported sample count (≤4×), renders into a multisampled
> color+depth target, and resolves into the swapchain image for presentation. All pipelines
> (sprites, meshes, sky) rasterize at that sample count; the shadow-map pass stays single-sample.
> A single-sample fallback path keeps devices without MSAA working.
>
> **M17 — glTF model loading (done).** `maz::render::loadGltf` parses glTF 2.0 files (via cgltf),
> merges every scene mesh into one `MeshData` with each node's world transform baked into
> positions/normals, and maps POSITION/NORMAL/TEXCOORD_0/COLOR_0 to the engine's mesh vertex. The
> `model` demo loads a bundled `house.gltf` at runtime and renders it with shadows, sky, and a HUD
> — the engine now shows artist-authored models, not just procedural shapes.
>
> **M18 — textured glTF (done).** `loadGltf` now also decodes the first material's base-color
> texture — embedded via a bufferView (stb_image decodes the PNG bytes) or referenced as an
> external image file — into `ModelData`. The bundled `house.gltf` carries a 4-quadrant detail
> atlas (brick / shingle / plank / glass) with per-face UVs, so its walls read as brick, the roof
> as shingle, the door as planks, and the windows as glass.
>
> **M19 — glTF scene loading (done).** `maz::render::loadGltfScene` reads a glTF file as a *scene*:
> each node with a mesh becomes a `SceneNode` carrying that mesh's geometry (in local space), its
> world transform, and its base-color texture — nothing is merged, so one source mesh can appear
> many times at different positions. The `village` app loads a bundled `village.gltf` (a ground
> plane, six textured houses, and nine trees — 16 nodes over 3 shared meshes). This is the
> data-driven step: a whole level lives in one asset file, not in C++.
>
> **M20 — VILLAGE QUEST (done).** The village scene becomes a playable game: the houses turn into
> solid AABB collision (built from each node's world bounds), golden coins are scattered in the open
> spaces, and you walk the village first-person (WASD + mouse-look) collecting every coin against a
> timer — with a win state and a best time saved across runs. It composes scene loading + collision
> + audio + save into one game, the proof the engine ships real games.
>
> **M21 — point lights (done).** The 3D mesh path gains real positional lighting: a lights UBO
> (descriptor set 2) carries ambient + one shadow-mapped directional "sun" plus up to eight point
> lights, and the fragment shader accumulates each point light with smooth distance attenuation.
> `Renderer::setLighting(SceneLighting)` drives it; the defaults reproduce the prior daytime look so
> every other app is untouched. VILLAGE QUEST uses it for dusk — a low warm sun over dim ambient
> with a warm lamp glowing at each house. Everything marked `[x]` below is done; everything else is
> the road ahead.

---

## Phase 0 — Foundation & tooling
- [x] CMake project, C++20, out-of-source build, Debug/Release
- [x] Warnings-as-errors, per-compiler warning flags
- [x] Dependency management via `FetchContent` (SDL3, GLM), `find_package(Vulkan)`
- [x] Shader compile step (GLSL → SPIR-V via `glslangValidator`)
- [x] `.clang-format`, `.gitignore` for build artifacts
- [x] Logging system (levels: trace/info/warn/error, `MAZ_LOG*` macros)
- [x] Assertion macros (`MAZ_ASSERT`, `MAZ_VERIFY`) with message + abort
- [x] Command-line argument parsing (`--headless`, `--frames N`, `--vsync`)
- [ ] clang-tidy config + CI lint gate
- [ ] Address/UB sanitizer presets (Debug), leak checks
- [ ] CI matrix (Linux/Windows/macOS) running the headless smoke test
- [x] Persistent key-value store (ini-style, user-data path) — `maz::core::KeyValueStore`
- [x] **CVars / config system** (`maz::core::CVarRegistry`: named typed tunables — bool/int/float/string
  with descriptions + numeric range clamps + string coercion for CLI flags; the `io::Config` bridge
  loads/saves them as JSON so `config.json` drives the engine; the `config` demo runs a cvar-driven
  scene; M77)
- [x] **INI ConfigFile** (`io::ConfigFile`: Godot's ConfigFile — ordered `[section]` + `key=value` store with
  typed get/set (bool synonyms, int, float, quote-stripped strings), lenient `parse()` (comments, blank lines,
  global section) and stable insertion-ordered `encode()` that round-trips; the `inifile` demo parses a
  settings.cfg into a table, edits it, and shows the re-encoded text; M165) — full Variant-literal values +
  direct file-path load/save later
- [ ] Crash handler / stack-trace dump, structured log sinks (file, console)
- [ ] Semantic-version header, `CHANGELOG.md`

## Phase 1 — Platform layer
- [x] Window creation, resize, close (SDL3)
- [x] Event pump; quit/resize handling
- [x] Keyboard + mouse state, just-pressed / just-released edge detection
- [x] Fixed-timestep clock (accumulator) + frame delta
- [ ] Fullscreen / borderless, multi-monitor, DPI / content scaling
- [ ] Focus / minimize / occlusion handling (pause when unfocused)
- [x] **Gamepad / controller support** (SDL3, sticks/buttons/triggers + deadzone; M23) — haptics TODO
- [x] **Action-mapping layer** (`maz::input::ActionMap`: named button actions with any-of
  keyboard/mouse/gamepad sources + pressed/held/released edges, and axis actions from key pairs +
  analog pad axes clamped to -1..1; SDL-free via sampler callbacks; the `actions` demo drives an
  avatar by mapped actions; M80)
- [x] **Analog-stick deadzone conditioning** (`input::analogVector` / `input::applyDeadzone`: Godot's
  `Input.get_vector`/`get_axis` maths — a *radial* deadzone on the whole stick vector, magnitude rescaled so
  the deadzone edge maps to 0 and full tilt to 1, and clamped to the unit circle so diagonals aren't faster;
  stateless, pairs with any input source; the `deadzone` demo shows a stick field + the 1-D response curve;
  M157) — a 2D get_vector convenience over ActionMap's four directional actions + per-action deadzone config
  later
- [ ] Text input / IME, clipboard, drag-and-drop
- [ ] vsync toggle, frame pacing, present-mode selection
- [ ] Filesystem abstraction, virtual paths, save-directory resolution
- [x] **Thread pool + job system** (`core::JobSystem`: worker pool, `submit`/`parallelFor`/
  `parallelRanges`; the `jobs` demo shows a ~3.8x fractal speedup; M65) — lock-free queues later

## Phase 2 — Core utilities
- [x] Math via GLM (vectors, matrices, quaternions) re-exported under `maz::math`
- [x] **Cubic Bézier path** (`math::Curve2D`: points with in/out handles joined by cubic Béziers; `sample`/
  `sampleSegment`/`tangent`/`length`, plus **arc-length baking** — `bake(interval)` lays down constant-speed
  points and `sampleBaked(distance)` moves at uniform speed regardless of curvature; Godot Curve2D/Path2D; the
  `curve` demo draws the spline + handles + baked dots + a traveller with its tangent; M142) — a 3D `Curve3D`,
  per-point tilt, and a Path2D/PathFollow2D scene node that advances a transform along it later
- [x] **Rect2** (`math::Rect2`: axis-aligned rectangle by position+size with `hasPoint` (min-incl/max-excl),
  `intersects`/`intersection` (clip), `merge` (union), `encloses`, `grow`/`growIndividual`, `expand`, `abs` —
  Godot Rect2; the `rects` demo draws overlapping rects with their intersection, union bounds, grow halo, and
  point tests; M149) — an integer `Rect2i` + a 3D `AABB` type + retrofitting UI/culling to use it later
- [x] **Geometry2D helpers** (`math::Geometry2D`: `segmentIntersect` (segment×segment → point + params),
  `closestPointOnSegment` / `distanceToSegment`, `pointInPolygon` (even-odd, concave-safe), and
  `segmentIntersectsCircle` — the workhorse queries behind line-of-sight, hit-picking, and trigger zones;
  Godot's `Geometry2D`; the `geometry` demo shows segment intersections + a point-in-polygon grid + closest-
  point projections; M160) — convex hull + polygon boolean clip/merge/offset (Clipper) + Delaunay later
- [x] **Transform2D** (`math::Transform2D`: Godot's 2×3 affine — two basis columns + origin; builders
  (identity/rotation/scaling/translation + the `compose(rot,scale,pos,skew)` node constructor), `xform`/
  `basisXform`/`xformInv`, `operator*` (parent×child), `affineInverse`, `getRotation`/`getScale`/`getSkew`,
  `orthonormalized`, `determinant`, `interpolateWith`; the `xform2d` demo draws an arrow under rotate/scale/
  skew/mirror with basis gizmos; M167) — a 3D `Transform3D`/`Basis` sibling + retrofitting sprites/TransformGraph later
- [ ] AABB/OBB, ray, plane, frustum
- [ ] Easing / interpolation, deterministic RNG (PCG/xoshiro)
- [ ] Memory: linear / stack / pool / frame allocators, arenas
- [x] **Handles / generational indices, object pools** (`core::SlotMap<T>` + `SlotHandle{index,
  generation}`: insert into recycled free slots, `get`/`contains`/`erase` with stale-handle (ABA)
  detection via a generation bump on free, double-free safe, `forEach` over live values — Godot's `RID` /
  stable entity handles; the `slotmap` demo drives insert/free/reuse and flags live vs stale handles;
  M137) — a typed multi-resource RID server + retrofitting texture/mesh/ECS handles onto it later
- [ ] Containers: `small_vector`, sparse set
- [x] **Ring / circular buffer** (`core::RingBuffer<T>`: fixed-capacity, serving a rolling window
  (`push` overwrites the oldest when full) and a bounded FIFO (`pushBack`/`popFront`); logical `at(0)`=oldest
  indexing hides the wrap; `toVector`/`front`/`back`/`clear`/`reset` — Godot's `RingBuffer`, behind frame-time
  graphs, input/replay buffers, moving averages; the `ring` demo plots a 96-slot frame-time history bar graph
  + an 8-slot input buffer; M164) — a lock-free MPSC variant + a byte/bit stream view later
- [x] **String interning / `StringId`** (`core::StringTable`: intern-or-find each unique name once →
  a stable 32-bit `StringId` handle so name equality is an int compare; `find` (non-inserting) / `str`
  (reverse) / `hash` (stored FNV-1a-32) / `contains` / `clear`; a `std::hash<StringId>` for unordered
  containers; the free `fnv1a32` content hash — Godot `StringName`; the `strtable` demo interns a repeated
  tag stream into a dedup pool; M132) — a global process-wide registry + retrofitting subsystems to use it later
- [x] **Event bus / signals** (`core::EventBus`: type-safe subscribe/emit/unsubscribe, per-type
  isolation, re-entrancy-safe snapshot dispatch; the `events` demo fans one event to 3 subscribers; M64)
- [x] **Per-object named signals** (`core::Signal<Args...>`: each object owns typed named channels others
  `connect` to — Godot signal/connect/emit; `connect`/`connectOnce`/`connectDeferred(Once)` +
  `disconnect`/`isConnected`/`connectionCount`, immediate `emit`, snapshot-safe dispatch, and a deferred
  queue drained by `flushDeferred`; the `signals` demo wires a Button→Player→died graph with an event log;
  M128) — string-keyed emit-by-name + connect binds + object-lifetime auto-disconnect later
- [ ] Minimal reflection (type ids, property registration) for serialization + editor
- [x] **Serialization** (`maz::io` ByteWriter/ByteReader: POD/string/vector, versioned magic
  headers, bounds-checked reads + file IO; the `persist` demo round-trips a scene to disk; M61)
- [x] **JSON / text format** (`maz::io::JsonValue` + never-throwing recursive-descent `parseJson`
  with line/column errors + compact/pretty `dump`; insertion-ordered objects for stable round-trips;
  the `data` demo builds an entire scene from an embedded JSON document; M75)
  + **JSON file IO** (`parseJsonFile`/`writeJsonFile`/`readTextFile`/`writeTextFile`); the `level`
  demo loads `assets/levels/arena.json` from disk into a tilemap + pickups and round-trips it back
  to the save directory — the editable-content pipeline end to end (M76)
- [x] **Base64** (`io::base64Encode` / `base64Decode`: RFC 4648 raw↔text so binary rides through JSON/.tres/
  URLs — encode overloads for bytes/vector/string; validating, whitespace-tolerant decode — Godot Marshalls;
  the `base64` demo shows text + byte buffers with their encodings + a round-trip check; M154) — a URL-safe
  variant, a streaming encoder, and Godot-style `var_to_bytes` variant marshalling later
- [x] **Debug stats overlay** (`ui::DebugOverlay`: smoothed FPS/frame-ms + per-frame draw counts
      via `Renderer::renderStats`; M29)
- [x] **Profiling: scoped timers** (`maz::core::Profiler`: nestable begin/end timing zones with
  inclusive + self time, call counts, depth, and EMA smoothing; a `ScopedZone` RAII over steady_clock;
  the `profiler` demo draws the zone tree as an indented bar chart; M78) — frame graph / Tracy later
- [x] **Unit tests** (dependency-free runner: math, collision, spatial grid, ECS, shake, particles;
  `maz_unit_tests` via ctest; M50)

## Phase 3 — Rendering (Vulkan)
- [x] Instance + validation layers (debug), debug messenger
- [x] Surface, physical-device selection, logical device + queues
- [x] Swapchain, image views, render pass, framebuffers
- [x] Command pool/buffers, sync (semaphores/fences), frames-in-flight
- [x] `beginFrame` / clear / `endFrame` present loop (clear color)
- [x] Swapchain recreation on resize / out-of-date
- [x] Graceful degrade when no GPU/ICD present (headless safe)
- [x] Shared `TextureStore` (one descriptor layout/pool) used by both 2D sprites and 3D meshes
- [x] Textured meshes (UVs + sampler; texture × vertex-color × lighting; REPEAT tiling)
- [x] Buffer + image helpers, staging uploads, `findMemoryType` (manual alloc; VMA swap-in later)
- [x] Graphics pipeline + descriptor-set management, push constants, dynamic state
- [x] **Per-frame scene UBO** (camera/light matrices in set-2 UBO; slim ≤96-byte per-draw push
  with named material fields, under the 128-byte limit; M54)
- [x] Shader module loading from SPIR-V  ·  [ ] reflection + hot reload
- [x] **2D:** sprite batch renderer, `Camera2D`, per-sprite tint/rotation, uv sub-rects (atlas-ready)
- [x] **2D:** tilemap rendering (view-culled, via atlas uv sub-rects)
- [x] **2D:** multi-camera passes (world-space + pixel-space HUD in one frame)
- [x] **2D:** follow camera (`game::CameraController2D`: deadzone + frame-rate-independent smoothing +
  world-bounds clamp + shake offset + worldToScreen; the `camera` demo tracks an avatar in a large world; M82)
- [x] **2D:** parallax scrolling backgrounds (`game::Parallax`: a `ParallaxLayer` scrolls by its
  `motionScale` relative to the camera and mirror-tiles by a period — `layerOffset` computes the on-screen
  offset, `firstTile`/`tileCount`/`pmod` place seamless tiles; the `parallax` demo shows one scene at three
  scrolls so far layers barely move while near ones sweep — Godot ParallaxBackground/ParallaxLayer; M123) —
  a texture-tiling draw call + CameraController2D integration later
- [x] **2D:** filled convex polygons (`Renderer::drawConvexPolygon`: triangle-fan flat shapes streamed
  through the sprite batch via a 1×1 white texture — Godot Polygon2D-style vector shapes; the `vectors`
  demo draws N-gons + a disc + translucent overlaps; M88)
- [x] **2D:** concave polygon fill via ear clipping (`render::triangulatePolygon`: the O(n²) ear-clipping
  algorithm tiles an arbitrary *simple* polygon — concave included — into triangles, detecting winding via the
  shoelace area and normalising to CCW, returning vertex indices three-per-triangle that the convex-fill path
  draws; a triangle fan can only fill convex shapes — Godot Polygon2D; the `polyfill` demo fills a star, a
  block arrow, a plus/cross, and a thick C-ring with the triangle mesh + outline overlaid; M156) — polygons
  with holes + self-intersecting input + per-vertex UV/colour for a textured Polygon2D later
- [x] **2D:** polyline stroking (`render::buildPolyline`: thicken a point path to a ribbon of a given width
  with Miter/Bevel/Round joints + None/Box/Round caps + closed loops → a triangle soup for
  `drawConvexPolygon` — Godot Line2D; the `line2d` demo strokes zig-zags per joint mode, bars per cap mode,
  a sampled sine curve, and a closed star; M126) — per-vertex gradient/width + texture-along-the-line + AA
  edges later
- [x] **2D:** multi-mesh instancing (`render::MultiMesh2D`: one convex base polygon + a per-instance buffer
  of `Instance2D{position, rotation, scale, colour}`; `transformInstance` applies scale→rotate→translate,
  `transformedPolygon(i)` yields one instance's world polygon, `bakeTriangles()` flattens all instances to one
  triangle soup — Godot MultiMesh/MultiMeshInstance2D; the `multimesh` demo stamps one dart 540× into a
  colour-swirled spiral field; M143) — a real GPU instanced draw call, per-instance texture/atlas + custom-data
  channels, and a 3D MultiMesh later
- [x] **2D:** lights + shadows (`game::Visibility2D` angle-sweep visibility polygon + a per-vertex-color
  gradient fan `Renderer::drawPolygonFan` — Godot Light2D / LightOccluder2D-style; occluder boxes carve
  real hard-edged shadow notches out of each light pool; the `lights2d` demo lights a dark room with three
  colored lights; M89)
- [x] **2D:** additive blending (`BlendMode::Additive` on `drawConvexPolygon`/`drawPolygonFan`: a second
  blend pipeline where src·alpha is ADDED to the destination, batched per blend mode — overlapping 2D
  lights brighten instead of averaging, matching Godot's Light2D compositing; also for glows/fire/energy;
  M90)
- [x] **2D:** soft/penumbra shadows (`game::SoftShadow2D`: model a light as a disc, spread `diskSamples`
  Vogel-spiral samples across it, `softVisibility` = fraction of the disc visible from a point — Godot
  Light2D soft shadows; the `softshadow` demo contrasts a hard point light with an area light whose
  shadow feathers into a penumbra; M101)
- [x] **2D:** normal-mapped lighting (`game::PointLight2D` + `shadeSurface`: a per-texel surface normal
  shaded by a 3D N·L Lambert term — the light taken at a height above the plane — plus smooth distance
  falloff and ambient, so a flat surface with a normal map reads as embossed relief — Godot Light2D
  normal maps; the `normalmap` demo lights a field of dome bumps with three coloured point lights; M111)
  — texture-projected light cookies + a CanvasItem material/shader hook later
- [x] Text rendering (TTF baked to an atlas via stb_truetype, tinted glyph sprites)
- [ ] **2D:** line/shape debug draw, chunked tilemap streaming, sprite sorting / layers
- [x] **Text layout / wrapping** (`ui::layoutText` + `ui::TextLayout`: greedy word-wrap to a max width
  via an injected measure callback + `\n` hard breaks + L/C/R alignment → positioned `TextLine` list;
  renderer-independent — Godot `Label` autowrap; the `textwrap` demo fits one paragraph three ways + a
  hard-break log; M134) — SDF text for crisp scaling, mid-word/hyphenation breaks, rich-text spans, and
  RTL/complex-script shaping later
- [ ] VMA (Vulkan Memory Allocator) to replace the manual allocator
- [ ] Text rendering (bitmap + SDF fonts, glyph atlas, layout)
- [x] Mesh renderer (indexed position/normal/color, MVP+model push constants) — `MeshRenderer`
- [x] **3D:** perspective camera (`math::perspective`) + depth buffer in the shared render pass
- [x] Procedural mesh primitives (box / sphere / plane) — `render::shapes`
- [x] **More procedural mesh primitives** (`render::shapes::makeCylinder` / `makeCone` / `makeTorus` /
  `makeCapsule` in `Shapes3D.hpp`: outward-normal + UV solids added alongside the untouched box/sphere/
  plane — Godot CylinderMesh/CapsuleMesh/TorusMesh + cone; the `primitives` demo renders a lit gallery;
  M136) — prism/quad-sphere/heightmap variants + tangents + LOD auto-tessellation later
- [x] First-person fly camera controller (`maz::game::FlyCamera`) + relative-mouse look
- [x] **Dynamic meshes** (`createDynamicMesh` + `updateMesh`, per-frame-in-flight vertex buffers;
  the `water` demo animates a summed-sine grid; M35)
- [x] **Instanced rendering** (`drawMeshInstanced`: per-instance model matrix via a second vertex
  binding, one `vkCmdDrawIndexed` for N copies; the `instances` demo draws 484 cubes in one call; M55)
- [x] **Billboard modes** (`render::buildBillboard`: model matrix orienting a quad toward the camera from the
  view matrix — Disabled / Enabled (full-facing) / YBillboard (yaws but stays upright) — Godot SpriteBase3D/
  GeometryInstance3D billboards; the `billboard` demo shows three rows of cards, one per mode; M144) — a
  Sprite3D/AnimatedSprite3D node (atlas+billboard+alpha) and velocity-aligned particle billboards later
- [x] **Orthographic 3D camera** (`math::orthographic` / `orthographicSize`: Vulkan-correct parallel
  projection — no perspective divide, equal-size objects at every depth, parallel edges never converge —
  Godot Camera3D Orthogonal; the `ortho3d` demo renders an isometric field of lit cube columns; M153) — a
  runtime perspective↔ortho toggle on a camera object + an ortho frustum for culling later
- [x] **Camera3D projection** (`render::Camera3D`: view+projection+viewport → `worldToScreen` (unproject),
  `screenToRay` (world pick ray), `screenToWorld`, and `frustum()`/`isPointVisible`/`isSphereVisible`
  (Gribb-Hartmann six planes) — Godot's Camera3D `unproject_position`/`project_ray_*`/`project_position`/
  `is_position_in_frustum` for mouse picking, world-space UI labels, aim rays; the `camera3d` demo projects a
  grid + axes + wireframe cube to 2D, colours points by frustum containment, and unprojects a centre ray to
  the ground; M163) — retrofitting the mesh renderer's private frustum onto it + near-plane segment clipping later
- [ ] Back-face cull toggle
- [x] Material groundwork: base-color texture, tangent-space normal map, **emissive** term
  (`drawMeshEmissive`; M37) + **specular/roughness** (`Material` + `drawMeshMaterial`, Blinn-Phong;
  M42)
- [x] **Texture mipmaps** (blit-generated chain, trilinear min sampling, NEAREST mag; M52)
- [x] **Transparency** (`drawMeshTransparent`: alpha-blended, depth-tested/no-write, back-to-front
  sorted; the `glass` demo layers 3 panes over opaque pillars; M56)
- [~] **Physically-based rendering** (a 3D-rendering deep-dive, R-milestones): **R1 — Cook-Torrance
  GGX specular** — the mesh shader's sun specular is an energy-correct microfacet BRDF (GGX normal
  distribution + Schlick-GGX geometry + Fresnel-Schlick), driven by the metallic/roughness workflow
  (`F0 = mix(0.04, albedo, metallic)`; dielectric by default), replacing the old Blinn-Phong lobe for
  materials that opt into `specular`. **R2 — full metallic/roughness PBR** — `metallic` is exposed on
  `Renderer::Material` and packed to the shader (`material1.z`); the Cook-Torrance BRDF now applies to
  the point/spot lights too (not just the sun), diffuse is energy-conserving (metals lose their diffuse
  term), and a flat ambient-reflection stand-in keeps metals from going pure black before IBL. The new
  `pbrballs` demo renders the classic 6×6 sphere grid (glossy→rough across, dielectric→metal up).
  **R3 — analytic image-based lighting** — the sky gradient is mirrored into the mesh Scene UBO, and
  PBR materials now draw their ambient from it: diffuse uses a hemispheric sky irradiance and specular
  reflects the sky in the mirror direction (blurred toward the irradiance as roughness rises), weighted
  by Karis' analytic environment BRDF. Metals now reflect the environment instead of relying on a flat
  stand-in, and `pbrballs` shows the sky gradient curving across each chrome ball. Matte materials are
  untouched (IBL is gated on `specular`); the six PBR goldens were re-baselined. **R4 — selectable
  filmic tonemap operators** — the composite pass now offers the three operators Godot exposes: ACES
  (Narkowicz curve, the historical default), ACES fitted (Stephen Hill's accurate RRT+ODT with proper
  color matrices), and **AgX** (Godot 4.2+'s default — log-encode, sigmoid contrast, outset matrix;
  desaturates highlights gracefully and avoids ACES' hue twists). `setTonemap` gains an operator arg
  (defaulting to ACES so existing scenes are byte-identical); the new `tonemap` demo renders a hot HDR
  scene selectable with `--op N` for side-by-side comparison. This closes the core 3D render-look gap
  with Godot; screen-space/ray-traced reflections and a full IBL prefilter remain honest structural
  gaps beyond a from-scratch analytic engine.
- [x] **Normal mapping** (tangent-space, derivative-based TBN, glTF `normalTexture`; M26)
- [x] Lighting: directional (Lambert) + ambient in the mesh shader
- [x] **Point lights** (up to 8, distance-attenuated, via a lights UBO + `setLighting`; M21)
- [x] **Distance fog** (exponential, camera-distance, blends meshes into the sky; M22)
- [x] **Dynamic sky + day/night** (sky colors + sun are `SceneLighting` params; `village` animates
      a full sun arc with responding sky/ambient/fog/lamps; M24)
- [x] **Spot lights** (point lights gain an optional cone: direction + inner/outer angle; M25)
- [ ] Forward+ or deferred path
- [x] Shadow maps (directional light, depth-only pass, **5×5 PCF** soft penumbra; M44)
- [x] Gradient skybox (per-pixel view-ray sky + sun glow)
- [ ] Image-based lighting, cascaded / point-light shadows
- [x] **Post-processing** (offscreen scene target + composite pass with threshold **bloom**; M27)
- [x] **HDR scene target + ACES tonemap/exposure** (16-bit float scene color, `setTonemap`; M38)
- [x] **Separable downsampled bloom** (`BloomChain`: bright-pass + ½-res 2-pass Gaussian; M41)
- [x] **Color grade** (composite vignette + saturation + contrast, `setColorGrade`; M45)
- [x] **Chromatic aberration** (composite radial RGB split, `setChromaticAberration`; M46)
- [x] **Film grain** (composite animated hashed noise, `setFilmGrain`; M48)
- [x] **Screen-space ambient occlusion** (`setSsao`) — a camera depth prepass (reusing the shadow
  depth pass/pipeline at full res) feeds a world-space `SsaoPass`: it reconstructs position + a
  camera-facing face normal from depth, samples a 16-point hemisphere with a per-pixel rotation and a
  range check, and 4×4-blurs the result; the composite multiplies the blurred AO into the scene so
  contact creases and corners darken. Off by default (byte-identical passthrough — verified); the
  `ssao` demo (`--noao` to compare) shows objects grounded in their contact shading. Applied in the
  composite (darkens the whole scene, not the ambient term alone) — a pragmatic approximation until a
  deferred/G-buffer path exists.
- [ ] FXAA/TAA, lens dirt / bloom-dirt mask; ambient-only AO once a deferred path lands
- [x] **MSAA** (multisampled color+depth + resolve, ≤4×; M16)
- [x] Render-to-texture (offscreen scene color target for post-processing; M27)
- [x] **Wireframe debug draw** (`setWireframe`, `VK_POLYGON_MODE_LINE` mesh pipeline; F4 in `world`; M34)
- [x] **Debug line draw** (`drawLine` / `drawAabb`, world-space `LINE_LIST` pipeline; F5 collider
  overlay in `world`; M36)
- [x] **3D reference grid + RGB gizmo axes** (`render::buildGrid` / `render::buildWireBox`: an XZ-plane
  ground grid with brighter center-axis lines + the X=red/Y=green/Z=blue origin gizmo, and a placeable
  12-edge wireframe box — colored `Line3` lists drawn via `drawLine`, no shared shader change — Godot
  Node3D viewport; the `grid3d` demo draws them under a fixed camera; M130) — interactive translate/rotate/
  scale gizmo handles + screen-constant sizing + snapping + picking need the shipping editor UI
- [ ] Multiple viewports, gizmos
- [ ] GPU profiling, keep validation-clean baseline

## Phase 4 — Scene & ECS
- [x] Entity Component System (sparse-set pools, `each<T>` / `view<A,B>`) — `maz::ecs::World`
- [ ] Core engine components: `Transform`, `Hierarchy/Parent`, `Name`, `Tag`
- [x] **Scene graph / transform hierarchy** (`maz::scene::TransformGraph`: nodes with local
  pos/rot/scale + parent; `update()` propagates world transforms parent-first via decomposed TRS;
  `localToWorld`; the `solar` demo runs a sun→planets→moons hierarchy; M81) — dirty-flag caching later
- [ ] System scheduler (ordered + parallel execution)
- [x] **Scene loading from data** (glTF scene: nodes + transforms + textures via `loadGltfScene`; M19)
- [x] **Native scene serialization (ECS save/load)** (`io::SceneSerializer`: register per-component
  JSON converters, then `saveWorld`/`loadWorld` a live `ecs::World` to/from JSON — the reflection-lite
  content backbone for save games, prefabs, and an editor; the `ecsave` demo round-trips a world; M79)
- [x] **Prefabs / instancing** (`scene::Prefab`: a `PrefabNode` tree of named nodes each with an exported
  `PropBag` (Float/Int/Bool/Vec2/Color/Text `PropValue`s); `instantiate(prefab, overrides)` deep-copies the
  template and applies per-node-path property overrides → an independent instance — Godot PackedScene; the
  `prefab` demo instances one turret template six times with per-instance overrides; M125)
- [x] **Node groups** (`scene::GroupRegistry`: tag any integer node id into named groups (unique, insertion-
  ordered) with a reverse node→groups index; `add`/`remove`/`removeNode`/`isInGroup`/`nodesInGroup`/`groupsOf`/
  `groupSize` + `call(group, fn)` broadcasting over a snapshot so the callback may add/free members mid-walk —
  Godot SceneTree add_to_group/get_nodes_in_group/call_group; the `groups` demo tags a 6×6 grid and drives a
  `nodesInGroup` query + a `call` broadcast; M148) — auto-join/leave on SceneTree enter/exit + group
  persistence in scene (de)serialization later
- [x] **Text resource save/load** (`io::savePrefabText` / `io::loadPrefabText`: round-trip a `scene::Prefab`
  to Godot-`.tscn`-style text — `[node name/parent]` sections + typed `key = TYPE value` lines — diffable,
  version-control-friendly, idempotent; the `restext` demo serializes an Enemy prefab + confirms the
  parse-back; M127) — full `.tscn` parsing (ExtResource/SubResource refs, arrays) + SceneSerializer bridge
  later
- [x] **Frustum culling** (per-mesh world AABB vs viewProj planes; culled count in stats; M30)
- [ ] Spatial partitioning (grid / quadtree / octree / BVH) for broadphase culling + queries

## Phase 5 — Asset pipeline
- [x] **Asset manager core** (`core::ResourceCache<Key,T>`: load-once/dedup-by-key + ref counting +
  evict callback + stats; the `assetcache` demo dedups 240 tiles to 8 textures; M66) — async load /
  GUIDs / hot reload later
- [x] Image loading (stb_image PNG/JPEG) + **blit-generated mipmaps** (M52)
- [ ] Compressed textures (KTX2), anisotropic filtering
- [x] **Model import** (glTF 2.0 via cgltf: `maz::render::loadGltf`; M17)
- [x] **glTF material base-color textures** (embedded or external, decoded via stb_image; M18)
- [x] Audio asset loading — **WAV** (`audio::decodeWav`/`encodeWav`, 8/16-bit PCM; M129); OGG/font-import/
  shader-assets still pending
- [~] Asset cooking / packing pipeline, **pak archives** (`io::packResources` + `io::ResourcePack`: bundle
  named blobs into one `.pck`-style archive — magic+version header, `(path, offset, size)` directory,
  concatenated data — and read them back by path with bounds-checked loading; Godot `.pck`/`PackedData`; the
  `respack` demo packs level JSON + text + a synthesized WAV + a raw blob and draws the directory + hex header
  + round-trip check; M140) — DEFLATE/gzip compression, per-file checksums/encryption, streaming reads, and a
  mount-pack virtual filesystem still pending
- [ ] Import settings + dependency graph + reimport

## Phase 6 — Physics & collision
- [~] 2D: tile-grid AABB collision with axis-separated sliding (done in demo)
- [ ] 2D: circle, broadphase (grid / sweep-and-prune), general resolution
- [x] **2D physics** (`game::PhysicsWorld2D`: circle + **box** rigid bodies, gravity, impulse +
  **Coulomb friction** + positional correction, static-box bounce; the `physics` demo stacks 45
  balls, `boxes` stacks mixed boxes/balls on ledges; M69, M72)
- [x] **2D kinematic character controller** (`game::moveAndSlide` + `sweptAabb`: velocity-driven AABB body
  swept against a static AABB list (Minkowski-expand + ray-slab), sliding the leftover motion along contacts
  over several iterations, with floor / wall / ceiling classification against an `up` direction + max floor
  angle → `SlideResult` with `onFloor`/`onWall`/`onCeiling` — Godot `CharacterBody2D.move_and_slide`; the
  `kinematic` demo runs a character through an obstacle course, colouring its path by contact state; M159) —
  rotated/circle shapes + moving platforms + floor snapping/stair-stepping + `move_and_collide` later
- [x] **2D rigid-body rotation** (oriented boxes: orientation + spin + moment of inertia via
  `Body2D::enableRotation()`; oriented-box SAT contacts + rotational impulses about the contact point +
  linear/angular damping — Godot RigidBody2D-style angular dynamics; opt-in so non-rotating scenes are
  unchanged; the `tumble` demo drops tilted boxes that topple and settle; M91)
- [x] **2D physics joints** (`game::Joint2D`: a **Pin** point-to-point constraint (2×2 effective mass +
  Baumgarte), a **damped Spring**, and a **Groove**/slider (a body pinned to a line, free to slide along
  it) — Godot PinJoint2D / DampedSpringJoint2D / GrooveJoint2D; solved by sequential impulses in the
  oriented step; either end may be a fixed world anchor; the `joints` demo builds a pin-chain rope bridge
  + spring-hung masses and the `groove` demo slides boxes down tilted rails; M93, M102)
- [x] **2-point contact manifolds** (`PhysicsWorld2D::solveManifolds`: reference/incident-face clipping
  gives oriented box-box contacts TWO points along the shared face, so a stacked box has the torque
  balance to stay square instead of rotating off — Box2D/Godot-style stable stacks; opt-in so existing
  rotating scenes keep their single-point numerics; the `stack` demo drops two identical towers, one
  stable, one toppling; M110) — cross-frame warm starting / a block solver later
- [x] **2D sensor / trigger regions** (`game::Area2D`: a circle-or-box zone that detects overlap without
  applying any force; `overlaps()` covers circle-circle, box-box (AABB), and mixed circle-box via
  closest-point; an `AreaMonitor` diffs each frame's overlapping set to fire **enter / exit** events — a
  monitoring `Area2D` in Godot terms; the `area2d` demo streams agents through a circular aura + a box gate,
  showing live membership + enter/exit counts per zone; M116) — body-vs-body sensor pairs later
- [x] **Collision layers & masks** (`game::CollisionLayers`: 32-bit `LayerMask`; a directional
  `detects(observerMask, targetLayer)` for Area2D/ray-style filtering + a symmetric
  `interact(aLayer,aMask,bLayer,bMask)` for physics pairing; a `CollisionObject2D` with per-bit editing;
  a named-layer `LayerRegistry` — Godot's collision_layer / collision_mask, the "which things does this
  react to" half of the collision system; the `layers` demo streams player/enemy/pickup species through
  a hurtbox watching only enemies + a magnet watching only pickups; M118) — per-shape layers + a
  layer-filtered broadphase query later
- [x] 3D: AABB collision with axis-separated sliding (`maz::game::Collision`) + camera collision
- [x] **Broadphase: uniform spatial grid** (`maz::game::SpatialGrid`, X/Z hash + `slideMove`; M40)
- [x] **Ray vs AABB queries** (`raycastAabb` / `raycast` nearest-hit, slab method; look-at targeting
  in `world`; M53)
- [x] **2D physics-space queries** (`game::queryRay` / `querySegment` / `queryPoint` in `PhysicsQuery2D`:
  cast a ray/segment against circle + **oriented-box** shapes and get the nearest `RayHit2D`
  (t / point / normal / index / id), filtered by a 32-bit collision **mask**; `queryPoint` / `pointInShape`
  answer which shapes contain a point — Godot `PhysicsDirectSpaceState2D.intersect_ray` / `intersect_point`,
  the primitive behind hitscan, line-of-sight, ground probes, and mouse picking; pure geometry, no sim step;
  the `rayquery` demo fans mask-filtered rays through a glass layer onto solids with contact normals + a
  point-pick; M131) — convex/capsule shapes, `intersect_shape` / shape-casts, and a broadphase-accelerated
  query later
- [x] **2D convex polygon collision** (`game::satOverlap`: Separating-Axis Theorem over both polygons'
  edge normals → overlap + minimum-translation vector (axis+depth); `polyContains` point-in-poly;
  `makeRegularPoly`/`makeBoxPoly` builders — Godot `ConvexPolygonShape2D`/`CollisionPolygon2D`; the
  `polycollide` demo tests a probe against a ring of shapes with MTV arrows; M138) — rigid-body-solver
  integration (polygon contact manifolds), concave auto-decomposition, and polygon-vs-circle/shape-casts later
- [x] **One-way platforms** (`game::resolveOneWayPlatform` / `resolveOneWayPlatforms`: a ledge solid only
  from above — a swept test lands a body descending across the surface from above and passes a body launched
  from below (or already under it) straight through, with a snap tolerance; the multi-platform form returns
  the topmost landing — Godot `one_way_collision`; the `oneway` demo drops three balls onto ledges while a
  fourth is launched up through one; M145) — wiring the flag into the `Physics2D` rigid-body solver as a
  per-shape property, a down-press drop-through gesture, and arbitrarily-angled one-way surfaces later
- [x] **Area gravity fields** (`game::GravityArea2D` + `gravityAt`: a `Rect2` zone overrides the gravity a
  body feels inside it — Directional (wind/updraft) or Point (toward a centre with inverse-square falloff),
  combined across overlapping zones by priority with Replace/Add modes over the world default — Godot Area2D
  gravity_space_override; the `gravzones` demo drops balls through a wind field, an updraft, and an attractor
  and their trails drift/U-turn/orbit; M152) — wiring it into the Physics2D integrator so bodies read it every
  step + circle/polygon zones + the full five space-override modes later
- [x] **Full 2D rigid-body-solver shape story** (a 2D physics deep-dive, P1–P13, targeting Box2D/Godot
  parity in `game::Physics2D`): a warm-started accumulated-impulse solver with split-impulse position
  correction (P1); an integrated spatial-hash broadphase (P2); body sleeping / islands (P3); collision
  **layer/mask** filtering in the world (P4); **capsule** (P5, Godot `CapsuleShape2D`), infinite
  **world-boundary** half-plane (P6, `WorldBoundaryShape2D`) and **convex-polygon** (P10,
  `ConvexPolygonShape2D`) shapes as first-class solver bodies with two-point clipped manifolds; contact
  **begin/persist/end** events (P7, `body_entered`/`body_exited`); integrated **continuous collision**
  for fast bodies (P8, `continuous_cd`); pin-joint **motor + angular limit** (P9); two-point capsule
  manifolds that kill resting micro-rock (P11); **physics-material combine modes** (P12, Godot
  `PhysicsMaterial` friction/restitution combine); and a static **polyline / chain terrain collider**
  (P13, Godot `ConcavePolygonShape2D` / a `SegmentShape2D` chain — `game::makePolyline(points, thickness)`;
  each segment is a swept-capsule collider, contact points pooled across segments and reduced to the
  widest-base pair so a box straddling a joint rests flat; the `polyline` demo drops circles/boxes/capsules
  onto rolling terrain and they settle on hills, slopes and in the valley)
- [ ] Other collision shapes (sphere/capsule casts), triggers / overlaps
- [~] **3D physics** (a from-scratch 3D deep-dive in `game::Physics3D`, sibling of the 2D solver):
  **D1 — rigid-body foundation** — `PhysicsWorld3D` with dynamic **Spheres** + an infinite static
  ground **Plane** (Godot WorldBoundaryShape3D), semi-implicit-Euler gravity integration, and
  sphere-sphere + sphere-plane **impulse resolution** (restitution + Coulomb friction + split
  positional correction); orientation/inertia fields carried but locked, ready for angular dynamics.
  **D2 — boxes + angular dynamics** — the solver is now rotation-aware: a per-body inverse-inertia
  tensor (rotated into world space each step), quaternion orientation integration, and contact
  impulses applied at the contact point (lever arms). Adds the **Box** (OBB) shape (Godot
  BoxShape3D) with `enableRotation()`, sphere-box contacts, and multi-corner box-vs-plane contacts,
  so a tilted box tumbles and settles flat and a ball landing off-centre imparts spin.
  **D3 — box-vs-box (3D SAT) + stacking** — oriented-box vs oriented-box via the Separating-Axis
  Theorem over 15 axes (3+3 face normals + 9 edge-edge cross products, faces preferred by a small
  tolerance); a face contact produces up to four points by clipping the incident face against the
  reference face's side planes (Sutherland-Hodgman), an edge contact a single closest-approach point,
  so boxes stack squarely (Godot BoxShape3D).
  **D4 — warm-started accumulated-impulse solver** — the velocity solve is now constraint-based: each
  contact carries a normal + two-tangent frame, accumulated normal/friction impulses clamped inside
  the Coulomb cone, and is warm-started from the previous frame's matching contact (paired by body
  ids + contact-point proximity). This is the modern Box2D/Godot solve — a six-high box tower stays
  dead vertical at a handful of iterations, where the non-warm-started solver leaned and jittered.
  The `physics3d` demo settles a six-cube and a four-cube tower plus a tumbling box and a pile of
  balls (golden-verified).
  **D5 — capsule shape** — the **Capsule** (Godot CapsuleShape3D): a segment along local Y swept by
  `radius`, with a cylinder-approximated inertia tensor (`makeCapsule`). Contacts: capsule-vs-plane
  (both caps → a horizontal capsule rests flat on two points), sphere-capsule, capsule-capsule
  (closest-segment), and capsule-box (segment/box closest point → sphere-box), all flowing through the
  warm solver. The `physics3d` demo adds two capsules that fall and rest flat on the ground.
  **D6 — broadphase (sweep-and-prune)** — candidate pairs come from a sweep-and-prune over world-space
  AABBs (spheres/boxes/capsules bounded; infinite planes tested against all), sorted to the same
  (i,j) order as brute force so the simulation is bit-identical — it only skips pairs whose AABBs are
  disjoint, cutting the O(n^2) narrow-phase down for spread-out scenes. Toggle via `broadphase`
  (default on). Verified by a test that runs a scene with it on and off to identical results.
  **D7 — body sleeping / islands** — opt-in via `allowSleep`: bodies connected by contacts form
  islands (union-find over last frame's constraints); an island sleeps once every dynamic member has
  stayed below the linear + angular thresholds for `sleepTime`. A sleeping body is made temporarily
  immovable so it's skipped by integration and acts as a static obstacle in the solve (zero CPU),
  and an island-mate touched by a mover wakes on the next step with no tunneling (Godot can_sleep).
  Verified by a test where a settled stack falls asleep and a dropped box wakes it.
  **D8 — physics ray queries** (`PhysicsWorld3D::queryRay`, Godot
  `PhysicsDirectSpaceState3D.intersect_ray`): cast a ray and get the nearest body it enters within a
  max distance — `RayHit3{t, point, normal, index}` — with analytic ray-vs-sphere / oriented-box /
  capsule / half-plane intersections and 32-bit collision-**mask** filtering; pure geometry, no sim
  step. The primitive behind hitscan weapons, line-of-sight, ground probes and mouse picking.
  **D9 — kinematic character controller** (`game::moveAndSlide3`, Godot
  `CharacterBody3D.move_and_slide`): advance a kinematic mover (sphere/box/capsule) by a velocity,
  then collide-and-slide against static colliders — push out of penetrations and strip the
  into-surface velocity component so the body slides along walls instead of stopping dead — returning
  a `MoveResult3{position, velocity, onFloor, onWall, onCeiling, floorNormal}` with floor/wall/ceiling
  classified against an up vector. The shape-vs-shape dispatch is shared with the world's narrow-phase.
  Verified by a test where a capsule lands on a floor (on_floor) and slides along a wall while still
  advancing sideways.
  **D10 — physics materials** — friction and restitution combine modes (`CombineMode` +
  `combineValue`, extracted into a shared header so the 2D and 3D solvers use identical material
  semantics — Godot PhysicsMaterial): `PhysicsWorld3D::frictionCombine` / `restitutionCombine`
  (GeometricMean / Average / Multiply / Min / Max) select how two bodies' scalars combine into the
  effective pair value; defaults (geometric-mean friction, min restitution) reproduce the prior
  hardcoded behaviour exactly. Verified by a test where Max-restitution makes a ball bounce far
  higher than Min. This brings the 3D deep-dive (D1–D10) to a complete rigid-body core.
  **D11 — pin joints** (`game::Joint3D` + `makePinJoint3`, Godot PinJoint3D): a point-to-point (ball)
  constraint holding two bodies' local anchor points together, solved in the velocity loop as a 3-DOF
  effective-mass impulse (`K = ΣinvMass·I − S(rA)·invIA·S(rA) − S(rB)·invIB·S(rB)`) with a Baumgarte
  position bias; either body may be static (a pendulum pinned to the world), and jointed bodies share
  a sleep island. Verified by a swinging pendulum that preserves its rod length and a two-link chain
  that hangs vertical.
  **D12 — distance / rod joints** (`Joint3D::Distance` + `makeDistanceJoint3`): a 1-DOF constraint
  holding two anchor points a fixed `restLength` apart along the line between them (rigid rods, rope
  links, ragdoll bones — Godot Generic6DOF distance), solved along the contact axis with a Baumgarte
  bias in the same joint loop. Verified by a ball on a rod hanging at exactly the rod length and a rod
  keeping two free-falling balls a fixed distance apart every step.
  **D13 — hinge (revolute) joint** (`Joint3D::Hinge` + `makeHingeJoint3`, Godot HingeJoint3D): the
  point-to-point linear constraint plus a 2-DOF angular lock so the bodies may only rotate relative to
  each other about the hinge axis, with a cross-product axis-realignment Baumgarte bias — doors,
  wheels, elbows/knees. Verified by a box hinged along world Z that swings down under gravity while its
  local Z axis never leaves the hinge axis and the hinge point stays pinned. Convex/trimesh colliders
  and full shape-vs-shape CCD remain honestly-noted extras.
- [x] Continuous collision (P8), **layers / masks** (`game::CollisionLayers`, M118 + world P4), physics
  materials (P12) — all landed by the P1–P13 deep-dive
- [x] Collider / grid debug visualization (`world` F5 colliders, F6 broadphase grid; M36/M40)

## Phase 7 — Audio
- [x] Audio device + real-time float mixer (SDL3, on the audio thread)
- [x] Synthesized voices (sine/square/triangle/noise), envelope, frequency glide, master volume
- [x] Looping arpeggio music bed; thread-safe `play`; graceful with no device
- [x] **Stereo mixer + 2D positional audio** (`audio::spatialize`: listener/source distance attenuation
  (linear / inverse-distance) + constant-power stereo pan; the mixer is stereo with per-voice L/R gain
  (`SoundDesc::leftGain`/`rightGain`) — Godot AudioStreamPlayer2D; the `spatial2d` demo visualizes the
  field; M94)
- [x] **DSP effects + mix buses** (`audio::Biquad` RBJ low/high/band-pass + `audio::Delay` feedback echo
  + `audio::Bus` ordered effect chain with output gain — Godot AudioEffectFilter/AudioEffectDelay + bus
  layout; pure per-sample math, the `bus` demo scopes source/low-pass/high-pass/low-pass→delay as
  stacked waveforms; M99)
- [x] **Reverb + distortion + compressor** (`audio::Reverb` Schroeder/Freeverb (4 combs + 2 allpasses),
  `audio::Distortion` tanh waveshaper, `audio::Compressor` peak-envelope dynamics — Godot AudioEffect
  Reverb/Distortion/Compressor; drop into the `Bus`; the `reverb` demo scopes a note through each; M106)
- [x] **Chorus + flanger + phaser** (`audio::Chorus` multi-voice detuned modulated delay, `audio::Flanger`
  swept feedback comb, `audio::Phaser` swept all-pass cascade — the three LFO-driven "time-modulation"
  effects, built on a shared `Lfo` + `fracTap` interpolated delay read; all slot onto the `Bus` — Godot
  AudioEffectChorus/AudioEffectPhaser; the `modfx` demo scopes one note through each; M146) — real-time
  per-voice insertion into the mixer callback + stereo widening + tempo-synced LFO later
- [x] **ADSR envelope** (`audio::ADSR`: attack/decay/sustain/release amplitude contour as a note-on/off
  gated state machine, `process(dt)`→level — the shape every synth voice is multiplied by; the `envelope`
  demo contrasts pluck/pad/stab presets as curves + shaped tones; M114) — real-time per-voice bus routing
  in the live mixer + LFOs / mod matrix later
- [x] **Spectrum analyzer / FFT** (`audio::SpectrumAnalyzer` + standalone radix-2 `fft`: window+FFT a sample
  frame → single-sided per-bin magnitudes, `peakBin`, `binFrequency`, and `magnitudeForRange(lowHz,highHz)` —
  Godot AudioEffectSpectrumAnalyzer's `get_magnitude_for_frequency_range` for rhythm games / VU + equalizer
  visualizers / beat-reactive FX; the `spectrum` demo plots a chord's spectrum + bass/mid/treble band meters;
  M166) — a live streaming analyzer in the mixer callback + mel/bark perceptual banding later
- [x] **WAV load/save** (`audio::decodeWav`/`encodeWav`: RIFF/WAVE PCM codec — parse 8-bit-unsigned +
  16-bit-signed PCM into float samples and write them back as 16-bit `.wav` bytes, byte-in/out — Godot
  AudioStreamWAV; the `wav` demo synthesizes → encodes → decodes → scopes the waveform; M129) — OGG/MP3
  decode + per-sound categories/ducking later
- [x] **Sample-playback mixer** (`audio::SampleMixer`: plays decoded `WavData` clips as voices — `play(clip,
  gain, pan, loop, speed)` → voice id, `stop`/`activeVoices`/`clear`, and `mix(out, frames, outRate)` sums
  every active voice into an interleaved-stereo buffer with linear-interpolated resampling/pitch, per-voice
  gain + stereo pan, mono→both-channels / stereo passthrough, auto-stop at clip end + loop wrap — Godot
  `AudioStreamPlayer` over `AudioStreamWAV`; the `sampler` demo plays a tone (left) + noise blip (right) and
  scopes the mixed L/R output; M139) — wiring it into the live SDL device callback + bus routing / DSP-insert
  + streaming decode later
- [x] **3D spatialization + doppler** (`audio::Spatial3D`: a `Listener3D` (pos + forward/up basis +
  velocity) and `Source3D`; four Godot AudioStreamPlayer3D attenuation models (None / Linear / Inverse /
  InverseSquare via `attenuation3D`), a listener-*orientation*-relative stereo pan (`panPosition` projects
  onto the listener's right axis `forward × up`) split constant-power (`equalPowerPan`), and doppler
  pitch (`dopplerPitch`, `(c−v_listener)/(c−v_source)`); `computeSpatialMix` bundles it into a
  left/right/pitch/distance/pan `SpatialMix`; the `spatial3d` demo is a top-down radar of six sources
  around one listener; M121) — real-time per-voice 3D bus wiring + HRTF/binaural + occlusion/reverb zones
  later
- [x] **Stream randomizer** (`audio::StreamRandomizer`: a weighted clip pool with Random / RandomNoRepeat /
  Sequential pick modes + per-trigger pitch (log-symmetric `[1/p, p]`) and volume (`±dB`) jitter — Godot's
  `AudioStreamRandomizer` that breaks up repetitive one-shots; each `next()` returns a `{index, pitchScale,
  volumeDb}` pick; deterministic via seeded `core::Random`; the `randomizer` demo shows a pick histogram +
  pitch×volume scatter + a no-repeat tick strip; M158) — auto-registering the picked clip as a live
  `SampleMixer` voice + a `.tres` resource wrapper later

## Phase 8 — Animation
- [x] **Sprite / flipbook animation** (`anim::SpriteAnim` + `gridFrames`: fps-timed loop/one-shot
  UV-frame playback; the `sprites` demo plays a phase-staggered wave; M63) — frame events later
- [x] **Skeletal animation core** (`anim::Skeleton`: joint hierarchy + bind/inverse-bind + skinning
  matrices; the `skeleton` demo CPU-skins a tapered tube on an 8-bone chain; M67) — glTF skins /
  GPU skinning later
- [x] **Keyframe clips + blending** (`anim::AnimClip`: per-joint TRS tracks, lerp/slerp sampling +
  loop, `blendPoses` cross-fade; the `animclip` demo blends a wave and a coil clip; M68) — blend
  trees later
- [x] **Animation controller** (`anim::Animator`: named clips + timed cross-fade transitions via
  play/update/pose; the `animator` demo cycles idle/wave/coil clips; M70) — full state graph / IK later
- [x] **Blend spaces** (`anim::BlendSpace1D` linear-neighbour blend + `anim::BlendSpace2D` barycentric
  blend over a triangulation + `blendPosesWeighted` N-way pose mix — Godot AnimationTree BlendSpace1D/2D;
  the `blendspace` demo morphs a skeleton across a grid of four corner poses; M92)
- [x] **Animation state machine** (`anim::AnimStateMachine`: named states + cross-fading transitions
  (fade time + condition + `travel()`) whose `active()` weights sum to 1 like a blend space, so states
  compose with blend spaces — Godot AnimationNodeStateMachine; the `statemachine` demo cross-fades a
  locomotion machine over a scripted timeline; M105) — nested sub-state-machines / root-motion later
- [x] **Animation blend tree** (`anim::BlendTree`: a node graph that NESTS the above — leaf `Input`
  nodes + `Blend2` (cross-fade) / `Add2` (additive layer) / `BlendSpace1` (1-D blend space over child
  nodes) interior nodes, each driven by a named blend parameter, evaluated recursively into one pose —
  Godot AnimationNodeBlendTree; the `blendtree` demo drives a stick figure through a gait blend-space
  layered with an additive wave and cross-faded into a jump; M117) — Add3/BlendN + a StateMachine node +
  a live parameter editor later
- [x] **Additive/layered pose blending** (`anim::makeAdditiveDelta` + `applyAdditiveDelta` /
  `additiveBlend`: compute a per-joint delta of an additive clip *relative to a reference pose*
  (translation subtract / rotation `inverse(ref)*add` / scale ratio) and layer it on a base at a weight —
  weight 0 returns the base, a zero delta leaves it untouched — Godot AnimationNodeAdd2 + the "additive"
  import that produces the delta; complements M117's BlendTree Add2 which consumes an already-delta pose;
  the `addblend` demo layers an elbow-bend onto a fixed arm at rising weights; M135) — per-bone-mask layers
  + reference extraction from imported clips later
- [x] **2-bone inverse kinematics** (`anim::solveTwoBoneIK`: law-of-cosines elbow solve + bend-side
  select + straight-arm overreach — Godot SkeletonModification2DTwoBoneIK; the `reach` demo is a grid of
  arms solving toward targets; M97)
- [x] **Multi-bone FABRIK IK** (`anim::solveFabrik`: Forward-And-Backward-Reaching IK over an N-joint
  chain — backward/forward passes preserve every bone length and reach the target, straighten when out
  of reach — Godot SkeletonModification2DFABRIK; the `tentacle` demo curls/straightens 8-bone chains
  toward targets; M107) — CCD + pole targets / bone constraints later
- [x] **Root motion** (`anim::RootMotionTrack`: cumulative clip-local position + heading; `delta` reports the
  root's per-step travel with a loop-seam sum; `advance` applies it to a world pose, rotating the clip-local
  step by the character's current facing so the feet don't slide — Godot AnimationMixer root-motion track; the
  `rootmotion` demo walks a character along a swept arc with footprints planted on it; M147) — auto-extract
  from a glTF/AnimClip root joint + blend root motion across a state-machine transition + full 3D later
- [x] **Float Curve resource** (`anim::Curve`: an editable `y=f(x)` curve of sorted control points with
  Constant / Linear / Cubic-Hermite interpolation, per-point left/right tangents, and value clamping to a
  `[min,max]` range — Godot's `Curve` resource, distinct from the `Curve2D` Bézier path; `sample(x)` holds
  the end values outside the domain; the `floatcurve` demo plots a linear ramp, a cubic ease-in-out S-curve,
  an ease-out, and a particle-size-over-life profile; M155) — bake-to-LUT + editor handles later
- [x] **Colour Gradient resource** (`anim::Gradient`: sorted `(offset, colour)` stops sampled over [0,1]
  with Constant / Linear / Cubic (Catmull-Rom, clamped) modes, a two-colour constructor, `setOffset` re-sort,
  and `bake(N)` → an N-colour ramp (GradientTexture1D) — Godot's `Gradient`, the colour analogue of `Curve`;
  the `gradient` demo shows a spectrum under all three modes plus fire / health / ocean ramps and a baked
  swatch strip; M162) — a selectable interpolation colour space (sRGB/OKLab), a GPU GradientTexture, and
  wiring it into the particle colour track later
- [x] **Tween / easing curves** (`maz::anim`: 15 easing functions + a once/repeat/ping-pong Tween
  with generic `sample`; the `tween` demo compares curves side by side; M59)
- [x] **Tween sequencer / property animator** (`anim::TweenPlayer`: chains Property/Interval/Callback
  tweeners into sequential groups that run in parallel within a group, loops the sequence, and writes bound
  `void(float)` setters every `update(dt)` — Godot SceneTreeTween (`create_tween` + `tween_property` /
  `tween_interval` / `tween_callback` + `parallel` + `set_loops`); the `choreo` demo snapshots five
  choreographies at a fixed time; M124) — vector/colour-in-one-call + `from_current`/relative + speed-scale
  later
- [x] **Keyframe timeline / sequencer** (`anim::Timeline`: named `Track`s of `Keyframe`s (time→value +
  per-segment easing) with endpoint-holding `sample`, plus a Once/Repeat/PingPong playhead — Godot
  AnimationPlayer; the `timeline` demo animates an arrow from keyed x/y/rot/scale/colour tracks with an
  editor-style track panel; M100)
- [x] **Call-method / trigger tracks** (`anim::TriggerTrack` + `MethodTimeline`: timed markers that FIRE
  as a playhead sweeps — the event half of Godot's AnimationPlayer, alongside M100's value tracks — with
  fire-once half-open semantics and correct loop-wrap; the `sequencer` demo is a four-lane drum machine
  firing kick/snare/hat/clap markers; M113) — a track editor UI later

## Phase 9 — UI
- [x] Font rendering (`ui::Font`) + a pixel-space HUD (text + health bar) in the demo
- [ ] Dear ImGui integration for tools / debug overlays
- [x] **Immediate-mode game-UI** (`ui::Context`: panel/label/button/toggle/slider with hot/active
  tracking; the `menu` demo is an interactive settings screen; M60)
- [x] **Retained UI layout** (`ui::LayoutNode`: Godot-style anchors + margins + HBox/VBox/Center
  containers with expand/spacing/padding; resolution-responsive computed rects; the `uilayout` demo
  builds a top-bar + sidebar + content + modal UI with no hand-placed pixels; M86)
- [x] **Auto-layout containers** (`ui::Container`: Godot's BoxContainer/GridContainer/MarginContainer/
  CenterContainer — per-axis `SizeFlag` (Fill/Expand/ShrinkBegin/Center/End) + stretch ratios;
  `hbox`/`vbox` grow only Expand children and split leftover by ratio, `grid` sizes columns/rows from the
  widest/tallest cell with expanding tracks sharing the surplus, `margin`/`center` handle single children,
  and `hboxMinSize`/`vboxMinSize`/`gridMinSize` compute a container's own min size for bottom-up sizing;
  the `containers` demo lays out four labelled cards; M122) — ScrollContainer/TabContainer + wiring into
  the LayoutNode tree with live min-size propagation later
- [x] **Text input + focus navigation** (`ui::TextField` single-line edit model: caret + insert/
  backspace/delete/arrows/home/end + max length; `ui::FocusChain`: ordered focusable ids with Tab/
  Shift+Tab wraparound; `Context::textField` widget draws box+text+caret + click-to-focus — Godot
  LineEdit + Control focus; the `form` demo is an editable account-settings form; M95) — controller UI nav later
- [x] **Nine-patch / StyleBox** (`ui::ninePatch`: slice a destination rect into a 3×3 grid by border
  insets — fixed corners, edges stretching one axis, center stretching both — mapping to matching source
  regions, so a themed panel scales without distorting its corner art; Godot StyleBoxTexture; the
  `stylebox` demo themes differently-sized panels + a button row from one style; M103)
- [x] **StyleBoxFlat + Theme server** (`ui::StyleBoxFlat`: procedural rounded-corner panel — fill +
  border + per-corner radius + soft drop shadow, no texture; `roundedRectPolygon` builds the convex
  outline and `drawStyleBoxFlat` layers shadow→border→fill — and `ui::Theme`: named styles/colours per
  control class + state with "type/state"→"type/normal"→default fallback, Godot StyleBoxFlat/Theme; the
  `theme` demo draws a button per state through one dark theme + a feature gallery; M109)
- [x] **Tree / TreeItem widget** (`ui::Tree`: hierarchical collapsible rows — a `TreeItem` holds
  text/id/colour/`collapsed` + heap-owned children, `visibleRows()` flattens the expanded items
  depth-first into rows carrying depth + hasChildren; folding hides a whole subtree — Godot's Tree control
  (scene dock / inspector / file browser); the `tree` demo shows a project file tree in a StyleBoxFlat
  panel with fold arrows + a selected-row highlight; M115) — scroll container / drag-reorder + a full
  per-control-class theme cascade later
- [x] **ItemList control** (`ui::ItemList`: scrollable box of selectable rows — each row has text/id +
  `selectable`/`disabled` flags; `Single` (radio) or `Multi` selection; fixed row height + separation +
  clamped `scroll` give `itemRect(i)`, `itemAtPoint()` (rejecting separator gaps), `ensureVisible()`,
  `visibleRange()`, and `selectNext`/`selectPrevious` keyboard nav that skips disabled rows — Godot's
  ItemList (file lists / inventory / level-select); the `itemlist` demo draws a scrolled single-select
  saved-games list with a scrollbar thumb + a multi-select loadout with checked rows; M161) — icon columns,
  per-item widget colours, multi-column grid, and drag-reorder later
- [x] **PopupMenu control** (`ui::PopupMenu`: vertical item list behind context menus / OptionButton dropdowns
  / menu bars — items with checkbox/radio state, disabled, separator, submenu-arrow, and accelerator hints;
  `itemRect`/`itemAtPoint` geometry, `hoverNext`/`hoverPrev` nav skipping separators+disabled, `checkRadio`
  single-choice groups, and `activate()` (toggle check / switch radio / return id) — Godot's PopupMenu; the
  `popupmenu` demo draws an open context menu with a hovered row, a checked item, a radio dot, a disabled row,
  separators, shortcuts and a submenu arrow; M168) — nested submenu popups + theme-font auto-width + an
  OptionButton wrapper later
- [x] **Range + ProgressBar** (`ui::Range`: the clamped/stepped scalar value model behind Godot's
  ProgressBar/HSlider/ScrollBar/SpinBox — min/max/step/page → a 0..1 `ratio`, `setRatio`/`step_` +
  allow-greater/lesser; `ui::ProgressBar` wraps it with `fillFraction`/`percent`; the `progress` demo draws
  six bars incl. a ratio-tinted health bar and a step-snapped bar; M150) — making the slider a Range subclass
  + an interactive ScrollBar/SpinBox + a fill/under/over StyleBox skin later
- [x] **BBCode rich text** (`ui::parseBBCode`: markup → resolved styled runs — `[b]`/`[i]`/`[u]`,
  `[color=…]` (hex `#rgb`/`#rrggbb`/`#rrggbbaa` + named), `[size=N]`, nested tags stacked, lenient
  (unclosed→to-end, stray-close ignored, `[lb]`/`[rb]` literal brackets, unknown tags/invalid colours pass
  through); `ui::stripBBCode` returns plain text; Godot RichTextLabel; the `richtext` demo shows six BBCode
  strings above their formatted results; M141) — wrapped rich layout, `[url]`/inline-image/table tags,
  `[center]`/`[right]` alignment, real bold/italic faces, and `[wave]`/`[shake]` effects later

## Phase 10 — Scripting & gameplay framework
- [x] Game-state machine (title / play / win / lose / restart) in the ORB RUN sample
- [ ] Scripting VM (Lua via sol2, or C# hosting) + engine bindings
- [ ] Script hot-reload, sandboxing
- [x] **AI steering** (`game::Steering`: seek/flee/arrive/separation/path-follow + integrate; the
  `crowd` demo flocks 14 agents through the maze; M58)
- [x] **Finite state machines** (`game::StateMachine`: enter/update/exit + guarded/any transitions;
  the `guard` demo runs patrol/chase/return AI; M62)
- [x] **Behavior trees** (`game::bt`: reactive Sequence/Selector/Inverter + Action/Condition leaves;
  the `behavior` demo runs a flee/chase/patrol priority tree; M71) + a **Blackboard** (typed shared
  memory), a **Parallel** composite (RequireOne/RequireAll), and **Repeater/AlwaysSucceed/AlwaysFail/
  Tap** decorators; the `blackboard` demo colours a sentry's tree by live per-node status as one flag
  flips it between patrol and engage; M104)
- [x] **GOAP planner** (`game::goap`: goal-oriented action planning — A* over a 64-bit-bitmask world
  state, each `Action` a precondition/effects/cost triple, returning the cheapest action sequence from
  the current world to a goal `Condition`; the `goap` demo has a survival agent plan "make fire" and
  shows the flow + fact-by-fact world-state, skipping a pricey shortcut; M108) — a planner beyond the
  behaviour tree, which Godot ships no built-in equivalent for; utility AI / HTN later
- [x] **Pathfinding** (`game::NavGrid`: 8-directional A* over a walkable/blocked grid, octile
  heuristic, no corner-cutting, world↔cell mapping; the `maze` demo re-plans a walker's route; M57)
- [x] **Navigation mesh** (`game::NavMesh`: convex-cell mesh with shared-edge adjacency, A* over cells
  + Mononen funnel string-pulling into a smooth corner-hugging path — polygon navigation like Godot's
  NavigationServer, beyond grid A*; the `navmesh` demo routes around a pillar; M87)
- [x] **Flow-field pathfinding** (`game::FlowField`: one Dijkstra outward from the goal → an integration
  cost field, then a baked per-cell unit flow direction down the gradient, so a whole crowd routes to a
  shared goal for the price of one search — the vector-field crowd technique Godot's per-agent
  NavigationServer lacks; the `flowfield` demo streams 90 agents around two barriers with a cost heat map
  + flow arrows; M112) — flow-field steering/avoidance blend + dynamic goal-move re-bake later
- [x] **Local collision avoidance** (`game::rvoVelocity`: reciprocal-velocity-obstacle candidate
  sampling scored on time-to-collision using the shared relative velocity `2·c − vA − vB` — Godot
  NavigationAgent2D avoidance; the `avoid` demo crosses 14 agents through a crowded centre; M98) —
  static-obstacle ORCA half-planes later
- [x] Particle system (CPU pool, burst emitters, color/size/alpha fade, gravity, drag) — `maz::fx`
- [x] Particle **attractor / vortex** affector (`setAttractor`: radial pull + tangential swirl; M49)
- [x] **Particle emitter resource** (`fx::Emitter`: emission **shape** (point/disk/ring/rect) +
  per-lifetime scale/alpha **curves** (`fx::Curve`) + multi-stop colour **gradient** (`fx::Gradient`) +
  direction/spread/speed + gravity + explosiveness; a deterministic `simulate(seed, t)` returns every
  live particle's pos/size/colour — Godot CPUParticles2D; the `emitter` demo shows a gravity fountain, a
  ring burst, and rect rain from three authored resources; M120) — GPU sim + trails + sub-emitters later
- [x] **World-space 3D particles** (camera-facing additive billboards, depth-tested; M28)
- [ ] GPU-simulated particles, affectors/attractors
- [x] **Procedural generation: noise** (`maz::core::Noise`: seeded Perlin `noise2` + fractal-Brownian
  `fbm2`; deterministic, 0 at lattice points, ~[-1,1]; the `noise` demo builds a terrain heightmap; M85)
- [x] **Procedural caves + tilemap autotiling** (`game::CellularCave` seeded cellular-automata cavern +
  `game::autotileMask4` 4-bit edge bitmask (out-of-bounds = solid) — Godot TileMap terrain sets; both
  deterministic pure logic; the `cave` demo generates a cave + autotiles its wall borders; M96)
- [x] **TileSet resource + per-tile collision** (`game::TileSet`: `TileDef` maps a tile id to an atlas
  source cell + a None/Full/**Box** (sub-cell) collision — Godot TileSet, where a single grid mixes full
  walls with half-height ledges the Tilemap's binary solid bit can't express; `collectSolids` →
  world-space collision boxes, `solidAt` point test, `dropY` drop-to-ground; the `tileset` demo drops
  probe balls that rest on full tiles and mid-cell on ledges; M119) — slope/polygon tile shapes +
  one-way platforms + a real atlas texture bind later
- [x] Save/load via `KeyValueStore` (ORB RUN high score persists across runs)
- [x] **Camera juice**: trauma-based screen shake (`maz::game::Shake`, deterministic; M39)
- [x] **Scene / game-state stack** (`core::SceneStack`: push/pop/replace + enter/pause/resume/exit
  lifecycle, modal + transparent-overlay support, deferred mutation; the `scenes` demo runs a
  menu→game→pause flow; M73)
- [ ] Full game-state serialization + checkpoints
- [x] **Time scheduler + sequences** (`maz::core::Scheduler`: after/every/cancel timers; `core::Sequence`:
  ordered wait/call/span script with looping; deterministic on the fixed-step clock; the `fireworks`
  demo spawns and explodes rockets on timers; M83)
- [x] **Deterministic RNG** (`maz::core::Random`: xoshiro256** + SplitMix64 seeding; nextU32/64,
  float/double, inclusive int range + float range, chance, weighted pick, Fisher-Yates shuffle,
  gaussian, angle; same seed → same stream; the `scatter` demo generates a seeded token field; M84)
- [x] **Localization + string tables** (`io::parseCsv` — RFC-4180 CSV reader: quoted fields, embedded
  commas/newlines, `""` escapes, CRLF/LF; + `io::TranslationTable` — a Godot-style translation CSV
  (key + per-locale columns) with `setLocale` + `tr(key)` and empty-cell→source / unknown-key→key
  fallback — Godot `Translation`; the `locale` demo renders one menu in four languages from one CSV;
  M133) — plural forms / message contexts / `%s` argument interpolation + OS-locale detection + a global
  auto-consulted TranslationServer later
- [ ] Deterministic time / date utilities

## Phase 11 — Editor & tooling
- [ ] Standalone editor app (engine + ImGui docking)
- [ ] Viewport with gizmos (translate/rotate/scale), grid, snapping
- [ ] Scene hierarchy panel, reflection-driven entity inspector, asset browser
- [ ] Play-in-editor, undo/redo (command stack), multi-select
- [ ] Content-pipeline UI, build / package button
- [ ] Profiler + log panels

## Phase 12 — Cross-cutting quality
- [x] **Unit tests** (M50) + **golden-image render tests** (`tools/golden.sh`, per-app RMSE
  tolerance, ctest-integrated, self-skips without a GPU; M51)
- [ ] CI gates / cross-platform build matrix
- [x] **Render interpolation** (`core::Interpolated<T>`: a previous/current pair pushed once per fixed step
  and blended by `Clock::interpolationAlpha()` each render frame, so motion stays smooth when the display
  rate doesn't divide the fixed rate — the missing consumer of the clock's alpha; `lerpAngle` shortest-arc +
  a `Transform2DState` pose blend — Godot physics interpolation; the `interp` demo ghosts previous+current
  poses with the interpolated pose between for four motions; M151) — auto-wiring into the ECS/TransformGraph
  so every moving node interpolates for free + 3D quaternion transform interpolation later
- [ ] Deterministic fixed-step simulation, replay
- [ ] Performance budgets + profiling dashboards
- [ ] Docs site, API docs (Doxygen), tutorials / samples
- [ ] Packaging / installers per platform, asset signing
- [ ] Opt-in telemetry / crash reporting

## Phase 13 — Demos & a first complete sample game
- [x] `sandbox`: window + animated clear color (proves the loop + renderer)
- [x] `sandbox`: bouncing textured sprites (proves 2D)
- [x] `apps/cube`: lit, depth-tested spinning cube + 2D HUD (proves 3D, and 2D+3D together)
- [x] `apps/scene3d`: a 3D scene (ground + ring of shapes) driven by the ECS, orbiting camera
- [x] `apps/world`: a solid, walkable first-person 3D collect-em-up (collision, pickups, HUD)
- [x] `sandbox`: top-down tile world — WASD movement, wall/water collision, camera follow
- [x] `sandbox`: HUD overlay — title, controls, animated health bar (pixel-space text)
- [x] **ORB RUN** (`apps/orbs`) — a complete original arcade game: title → play → win/lose →
      restart, score + timer HUD, collectibles, hazards
- [x] **VILLAGE QUEST** (`apps/village`) — a first-person 3D game built on a data-loaded glTF
      scene: solid house collision, coins to collect against the clock, a win state, and a
      best-time saved across runs (scene loading + collision + audio + save composed; M20)
- [x] **Data-driven scene** (`apps/data`) — every sprite (shape, position, size, tint, bob/spin) plus
      the clear color and title come from an embedded JSON document parsed at runtime; proves the
      engine can be driven by human-editable data, not just code (M75)
- [x] **On-disk JSON level** (`apps/level`) — reads `assets/levels/arena.json` from disk into a
      `game::Tilemap` (tile rows + palette + solidity) plus pickups, renders it top-down, and
      round-trips the level back to the save directory; the editable-content pipeline end to end (M76)
- [x] **Config / CVars** (`apps/config`) — registers typed tunables, applies a JSON config, and draws
      a scene whose orb count/speed/hue/brightness/grid are all cvar-driven, beside a live cvar table (M77)
- [x] **CPU profiler** (`apps/profiler`) — feeds a fixed synthetic frame into `core::Profiler` and draws
      the nested zone tree as an indented bar chart (inclusive vs self time per zone) (M78)
- [x] **ECS save/load** (`apps/ecsave`) — builds an entity world, serializes it to JSON, reloads that
      JSON into a fresh world, and renders the reload — proving the round-trip (M79)
- [x] **Input actions** (`apps/actions`) — an avatar driven by named actions (MoveX/MoveY axes,
      Fire/Dash buttons) bound to keyboard + gamepad, with a live action-state HUD (M80)
- [x] **Transform hierarchy** (`apps/solar`) — a solar system (sun → planets → moons) where only pivot
      rotations are set and the scene graph sweeps the whole tree into place (M81)
- [x] **Follow camera** (`apps/camera`) — a large world with a moving avatar the camera tracks with a
      deadzone, smoothing, and world-bounds clamp (M82)
- [x] **Fireworks** (`apps/fireworks`) — timers spawn rockets that each explode into particle bursts
      after a delay; the whole show is scheduler-driven (M83)
- [x] **Particle emitters** (`apps/emitter`) — a gravity fountain, an omnidirectional ring burst, and
      angled rect rain, each an authored `fx::Emitter` resource simulated deterministically (M120)
- [x] **Procedural scatter** (`apps/scatter`) — a seeded RNG generates a token field with weighted
      rarity (Common/Uncommon/Rare/Epic) and a distribution legend; same seed → same field (M84)
- [x] **Procedural terrain** (`apps/noise`) — a heightmap from fbm noise, colored by a terrain ramp
      with hillshade; same seed → same continent (M85)
- [x] **UI layout** (`apps/uilayout`) — a responsive app UI (top bar + sidebar + content + modal) from
      anchors and containers (M86)
- [x] **Nine-patch StyleBox** (`apps/stylebox`) — differently-sized themed panels + a button row from
      one style; fixed corners, stretching edges/center (`ui::ninePatch`) (M103)
- [x] **StyleBoxFlat + Theme** (`apps/theme`) — a dark theme drawing a button in each state
      (normal/hover/pressed/disabled) + a gallery of rounded/bordered/shadowed/pill/tab panels
      (`ui::StyleBoxFlat` + `ui::Theme`) (M109)
- [x] **Tree widget** (`apps/tree`) — a project file tree in a StyleBoxFlat panel: indented rows, fold
      arrows, two collapsed folders, and a selected-row highlight (`ui::Tree`) (M115)
- [x] **Area2D sensors** (`apps/area2d`) — agents stream through a circular aura + a box gate; each zone
      shows live membership + enter/exit counts, agents inside a zone lit + ringed (`game::Area2D`) (M116)
- [x] **Collision layers** (`apps/layers`) — player/enemy/pickup species stream through a hurtbox watching
      only enemies + a magnet watching only pickups; matches light up, ignored overlaps go dashed
      (`game::CollisionLayers`) (M118)
- [x] **Behavior-tree blackboard** (`apps/blackboard`) — a sentry's tree drawn twice (patrol vs engage),
      each node coloured by live status as one blackboard flag flips the branch (`game::bt`) (M104)
- [x] **GOAP planner** (`apps/goap`) — a survival agent plans "make fire" from an action library; the
      computed optimal plan is drawn as a flow with the world-state changing fact-by-fact until fire
      lights, and the expensive shortcut dimmed as skipped (`game::goap`) (M108)
- [x] **Animation state machine** (`apps/statemachine`) — a locomotion machine's active-state weights as
      stacked cross-fading colour bands over a scripted timeline (`anim::AnimStateMachine`) (M105)
- [x] **Navigation mesh** (`apps/navmesh`) — an agent path routed around a pillar with A* + funnel
      string-pulling (M87)
- [x] **Filled polygons** (`apps/vectors`) — regular N-gons, a 64-gon circle, and translucent
      overlapping triangles via `drawConvexPolygon` (M88)
- [x] **2D lights + shadows** (`apps/lights2d`) — a dark room lit by three colored lights, each a
      visibility polygon (`game::Visibility2D`) rendered as a gradient fan, with solid occluder boxes
      casting real hard-edged shadows; lights composite additively so overlaps brighten (M89, M90)
- [x] **2D rotation** (`apps/tumble`) — tilted rectangles dropped into a bin that fall, tumble on
      their corners, and settle into a leaning pile via the oriented rigid-body solver (M91)
- [x] **Animation blend space** (`apps/blendspace`) — a grid of stick-figure skeletons whose pose is
      blended across a 2D parameter space from four corner poses (`anim::BlendSpace2D`) (M92)
- [x] **Animation blend tree** (`apps/blendtree`) — a stick figure driven by a nested node graph: a gait
      blend-space, an additive wave layer, cross-faded into a jump; a grid sweeps gait × air
      (`anim::BlendTree`) (M117)
- [x] **Physics joints** (`apps/joints`) — a pin-jointed rope bridge sagging into a catenary + masses
      hung from damped springs of increasing stiffness (`game::Joint2D`) (M93)
- [x] **Groove/slider joints** (`apps/groove`) — three boxes pinned to tilted rails, each sliding down
      its incline (not straight down) and settling against a stop (`game::Joint2D::Groove`) (M102)
- [x] **Stable box stacks** (`apps/stack`) — two identical five-box towers dropped side by side; with
      two-point manifolds on the tower stays square, with them off it topples (`solveManifolds`) (M110)
- [x] **Positional audio** (`apps/spatial2d`) — a listener + sound sources with per-source distance
      attenuation and stereo pan visualized as halos + L/R bars + a master meter (`audio::spatialize`) (M94)
- [x] **Text input form** (`apps/form`) — an editable account-settings form: click/Tab to focus a
      field (accent border + caret), type to edit (`ui::TextField` + `ui::FocusChain`) (M95)
- [x] **Procedural cave** (`apps/cave`) — a seeded cellular-automata cavern with autotiled wall borders
      (`game::CellularCave` + `game::autotileMask4`) (M96)
- [x] **TileSet & per-tile collision** (`apps/tileset`) — one grid mixing full ground/wall tiles with
      half-height ledges, every tile's collision box drawn, and probe balls resting on what they hit
      (`game::TileSet`) (M119)
- [x] **Inverse kinematics** (`apps/reach`) — a grid of 2-bone arms whose elbows are solved so each hand
      reaches its target, with out-of-reach targets shown extended (`anim::solveTwoBoneIK`) (M97)
- [x] **RVO avoidance** (`apps/avoid`) — 14 agents crossing a circle to antipodal goals, their trails
      bulging around the crowded centre as reciprocal velocity obstacles route them apart
      (`game::rvoVelocity`) (M98)
- [x] **Audio DSP buses** (`apps/bus`) — one plucked-sawtooth note scoped as four stacked waveforms:
      source, low-pass, high-pass, and a low-pass→delay bus (filtered note + echoes) (`audio::Biquad`
      / `audio::Delay` / `audio::Bus`) (M99)
- [x] **Keyframe timeline** (`apps/timeline`) — an arrow driven by keyed x/y/rotation/scale/colour
      tracks, shown as an onion-skin trail plus an editor track panel with keyframe dots + a playhead
      (`anim::Timeline`) (M100)
- [x] **Trigger / method tracks** (`apps/sequencer`) — a four-lane drum machine whose kick/snare/hat/clap
      markers fire as one playhead sweeps the loop, with fire counts + a recent-fires strip
      (`anim::MethodTimeline`) (M113)
- [x] **Reverb/distortion/compressor** (`apps/reverb`) — one note scoped through a Schroeder reverb, a
      tanh distortion, and a compressor as stacked waveforms (`audio::Reverb/Distortion/Compressor`) (M106)
- [x] **ADSR envelope** (`apps/envelope`) — pluck/pad/stab presets, each as an attack/decay/sustain/release
      curve + the sine tone shaped by it (`audio::ADSR`) (M114)
- [x] **FABRIK IK chains** (`apps/tentacle`) — a row of 8-bone chains reaching for targets; reachable
      ones curl to touch (green), out-of-reach ones straighten and point (red) (`anim::solveFabrik`) (M107)
- [x] **Soft 2D shadows** (`apps/softshadow`) — the same box+light drawn hard (point light, crisp edge)
      vs soft (area light, 24 samples) so the shadow feathers into a penumbra (`game::SoftShadow2D`) (M101)
- [x] **Normal-mapped lighting** (`apps/normalmap`) — a field of dome bumps lit by three coloured point
      lights, each dome shaded on the side facing a light so it reads as 3D relief (`game::shadeSurface`) (M111)
- [x] **Flow-field pathfinding** (`apps/flowfield`) — a cost heat map + baked flow arrows + 90 agents
      streaming around two barriers to a shared goal from one Dijkstra (`game::FlowField`) (M112)
- [x] **CATCHER** (`apps/catcher`) — a complete 2D game composed from the engine's own systems:
      the scene stack (menu → play → game-over), the event bus (catch/miss events fan out to
      scoring, particle bursts, and screen-shake), 2D contact tests (paddle vs. falling coins /
      hazards), the pooled particle system, screen-shake juice, and a high score persisted across
      runs via the KeyValueStore — with a deterministic attract-mode AI so the golden is stable (M74)
- [ ] More sample scenes: pong, platformer, top-down adventure

> Scope note: Maz Engine is a general-purpose engine and is **not** tied to any specific game.
> The unrelated *ZOMBOID: ANCHORAGE* browser game that also lives in this repo is **not** an
> engine target or dependency. Sample games built to exercise the engine are original and
> genre-neutral.

---

### How to pick the next task
1. Finish **Phase 3** rendering (VMA → pipeline → sprite batch) — it unblocks everything visual.
2. In parallel, stand up **Phase 4 ECS** — it unblocks scenes and gameplay.
3. Then **Phase 5 assets** so you can load real textures/models.
4. Everything after is genre-driven; a small original sample game (Phase 13) is the forcing
   function that keeps the API honest.
