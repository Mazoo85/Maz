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
- [ ] Text input / IME, clipboard, drag-and-drop
- [ ] vsync toggle, frame pacing, present-mode selection
- [ ] Filesystem abstraction, virtual paths, save-directory resolution
- [x] **Thread pool + job system** (`core::JobSystem`: worker pool, `submit`/`parallelFor`/
  `parallelRanges`; the `jobs` demo shows a ~3.8x fractal speedup; M65) — lock-free queues later

## Phase 2 — Core utilities
- [x] Math via GLM (vectors, matrices, quaternions) re-exported under `maz::math`
- [ ] Transform helpers, AABB/OBB, ray, plane, frustum
- [ ] Easing / interpolation, deterministic RNG (PCG/xoshiro)
- [ ] Memory: linear / stack / pool / frame allocators, arenas
- [ ] Handles / generational indices, object pools
- [ ] Containers: `small_vector`, sparse set, ring buffer
- [ ] String interning / `StringId` (hashed), fixed strings
- [x] **Event bus / signals** (`core::EventBus`: type-safe subscribe/emit/unsubscribe, per-type
  isolation, re-entrancy-safe snapshot dispatch; the `events` demo fans one event to 3 subscribers; M64)
- [ ] Minimal reflection (type ids, property registration) for serialization + editor
- [x] **Serialization** (`maz::io` ByteWriter/ByteReader: POD/string/vector, versioned magic
  headers, bounds-checked reads + file IO; the `persist` demo round-trips a scene to disk; M61)
- [x] **JSON / text format** (`maz::io::JsonValue` + never-throwing recursive-descent `parseJson`
  with line/column errors + compact/pretty `dump`; insertion-ordered objects for stable round-trips;
  the `data` demo builds an entire scene from an embedded JSON document; M75)
  + **JSON file IO** (`parseJsonFile`/`writeJsonFile`/`readTextFile`/`writeTextFile`); the `level`
  demo loads `assets/levels/arena.json` from disk into a tilemap + pickups and round-trips it back
  to the save directory — the editable-content pipeline end to end (M76)
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
- [x] Text rendering (TTF baked to an atlas via stb_truetype, tinted glyph sprites)
- [ ] **2D:** line/shape debug draw, chunked tilemap streaming, sprite sorting / layers
- [ ] SDF text for crisp scaling, text layout/wrapping
- [ ] VMA (Vulkan Memory Allocator) to replace the manual allocator
- [ ] Text rendering (bitmap + SDF fonts, glyph atlas, layout)
- [x] Mesh renderer (indexed position/normal/color, MVP+model push constants) — `MeshRenderer`
- [x] **3D:** perspective camera (`math::perspective`) + depth buffer in the shared render pass
- [x] Procedural mesh primitives (box / sphere / plane) — `render::shapes`
- [x] First-person fly camera controller (`maz::game::FlyCamera`) + relative-mouse look
- [x] **Dynamic meshes** (`createDynamicMesh` + `updateMesh`, per-frame-in-flight vertex buffers;
  the `water` demo animates a summed-sine grid; M35)
- [x] **Instanced rendering** (`drawMeshInstanced`: per-instance model matrix via a second vertex
  binding, one `vkCmdDrawIndexed` for N copies; the `instances` demo draws 484 cubes in one call; M55)
- [ ] Back-face cull toggle, ortho 3D camera
- [x] Material groundwork: base-color texture, tangent-space normal map, **emissive** term
  (`drawMeshEmissive`; M37) + **specular/roughness** (`Material` + `drawMeshMaterial`, Blinn-Phong;
  M42)
- [x] **Texture mipmaps** (blit-generated chain, trilinear min sampling, NEAREST mag; M52)
- [x] **Transparency** (`drawMeshTransparent`: alpha-blended, depth-tested/no-write, back-to-front
  sorted; the `glass` demo layers 3 panes over opaque pillars; M56)
- [ ] Full PBR (energy-conserving metallic/roughness, IBL)
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
- [ ] FXAA/TAA, lens dirt / bloom-dirt mask
- [x] **MSAA** (multisampled color+depth + resolve, ≤4×; M16)
- [x] Render-to-texture (offscreen scene color target for post-processing; M27)
- [x] **Wireframe debug draw** (`setWireframe`, `VK_POLYGON_MODE_LINE` mesh pipeline; F4 in `world`; M34)
- [x] **Debug line draw** (`drawLine` / `drawAabb`, world-space `LINE_LIST` pipeline; F5 collider
  overlay in `world`; M36)
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
  — prefabs / blueprints layered on this later
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
- [ ] Audio asset loading (wav / ogg), font import, shader assets
- [ ] Asset cooking / packing pipeline, pak archives, streaming
- [ ] Import settings + dependency graph + reimport

## Phase 6 — Physics & collision
- [~] 2D: tile-grid AABB collision with axis-separated sliding (done in demo)
- [ ] 2D: circle, broadphase (grid / sweep-and-prune), general resolution
- [x] **2D physics** (`game::PhysicsWorld2D`: circle + **box** rigid bodies, gravity, impulse +
  **Coulomb friction** + positional correction, static-box bounce; the `physics` demo stacks 45
  balls, `boxes` stacks mixed boxes/balls on ledges; M69, M72) — rotation / oriented boxes later
- [x] 3D: AABB collision with axis-separated sliding (`maz::game::Collision`) + camera collision
- [x] **Broadphase: uniform spatial grid** (`maz::game::SpatialGrid`, X/Z hash + `slideMove`; M40)
- [x] **Ray vs AABB queries** (`raycastAabb` / `raycast` nearest-hit, slab method; look-at targeting
  in `world`; M53)
- [ ] Other collision shapes, sphere/capsule casts, triggers / overlaps
- [ ] 3D physics integration (Jolt or Bullet), character controller
- [ ] Continuous collision, layers / masks, physics materials
- [x] Collider / grid debug visualization (`world` F5 colliders, F6 broadphase grid; M36/M40)

## Phase 7 — Audio
- [x] Audio device + real-time float mixer (SDL3, on the audio thread)
- [x] Synthesized voices (sine/square/triangle/noise), envelope, frequency glide, master volume
- [x] Looping arpeggio music bed; thread-safe `play`; graceful with no device
- [ ] WAV/OGG loading, buses / categories, per-sound pitch
- [ ] 2D panning + 3D spatialization, attenuation, doppler
- [ ] DSP effects (reverb, filter), ducking

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
- [x] **Tween / easing curves** (`maz::anim`: 15 easing functions + a once/repeat/ping-pong Tween
  with generic `sample`; the `tween` demo compares curves side by side; M59) — timelines later

## Phase 9 — UI
- [x] Font rendering (`ui::Font`) + a pixel-space HUD (text + health bar) in the demo
- [ ] Dear ImGui integration for tools / debug overlays
- [x] **Immediate-mode game-UI** (`ui::Context`: panel/label/button/toggle/slider with hot/active
  tracking; the `menu` demo is an interactive settings screen; M60) — layout/anchoring/text input later
- [ ] Text input, focus / navigation, controller UI nav
- [ ] Nine-slice, fonts, localization-aware text

## Phase 10 — Scripting & gameplay framework
- [x] Game-state machine (title / play / win / lose / restart) in the ORB RUN sample
- [ ] Scripting VM (Lua via sol2, or C# hosting) + engine bindings
- [ ] Script hot-reload, sandboxing
- [x] **AI steering** (`game::Steering`: seek/flee/arrive/separation/path-follow + integrate; the
  `crowd` demo flocks 14 agents through the maze; M58)
- [x] **Finite state machines** (`game::StateMachine`: enter/update/exit + guarded/any transitions;
  the `guard` demo runs patrol/chase/return AI; M62)
- [x] **Behavior trees** (`game::bt`: reactive Sequence/Selector/Inverter + Action/Condition leaves;
  the `behavior` demo runs a flee/chase/patrol priority tree; M71) — blackboard / parallel nodes later
- [x] **Pathfinding** (`game::NavGrid`: 8-directional A* over a walkable/blocked grid, octile
  heuristic, no corner-cutting, world↔cell mapping; the `maze` demo re-plans a walker's route; M57)
  — navmesh later
- [x] Particle system (CPU pool, burst emitters, color/size/alpha fade, gravity, drag) — `maz::fx`
- [x] Particle **attractor / vortex** affector (`setAttractor`: radial pull + tangential swirl; M49)
- [x] **World-space 3D particles** (camera-facing additive billboards, depth-tested; M28)
- [ ] GPU-simulated particles, affectors/attractors
- [x] **Procedural generation: noise** (`maz::core::Noise`: seeded Perlin `noise2` + fractal-Brownian
  `fbm2`; deterministic, 0 at lattice points, ~[-1,1]; the `noise` demo builds a terrain heightmap; M85)
  — tilemap tools / cave carving later
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
- [ ] Localization + string tables, deterministic time

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
- [x] **Procedural scatter** (`apps/scatter`) — a seeded RNG generates a token field with weighted
      rarity (Common/Uncommon/Rare/Epic) and a distribution legend; same seed → same field (M84)
- [x] **Procedural terrain** (`apps/noise`) — a heightmap from fbm noise, colored by a terrain ramp
      with hillshade; same seed → same continent (M85)
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
