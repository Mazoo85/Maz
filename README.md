# Maz Engine

A native **C++20 + Vulkan + SDL3** game engine — 2D-complete with a working 3D path (a
pluggable renderer, 2D and 3D cameras, a depth buffer, and lit meshes alongside batched sprites).

See **[`docs/ROADMAP.md`](docs/ROADMAP.md)** for the full build plan (the "massive list") and
**[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** for the module map and design principles.

## Building

Requires a C++20 compiler, CMake ≥ 3.24, the **Vulkan SDK** (loader + `glslangValidator`), and
on Linux the X11 dev packages. SDL3 and GLM are fetched automatically.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/sandbox                 # top-down tile-world demo (needs a GPU + display)
./build/bin/orbs                    # "ORB RUN" — a complete sample arcade game
./build/bin/swarm                   # ECS demo — 800 entities
./build/bin/cube                    # 3D demo — a lit, spinning cube
./build/bin/scene3d                 # ECS + 3D — a scene of shapes, orbiting camera
./build/bin/world                   # explorable 3D — fly through with WASD + mouse-look
./build/bin/model                   # glTF demo — loads house.gltf, orbits with shadows + sky
./build/bin/village                 # VILLAGE QUEST — collect coins across the loaded village
./build/bin/village --demo          # autopilot playthrough (for capture)
./build/bin/water                   # dynamic-mesh demo — a rippling sine-wave water surface
./build/bin/sandbox --headless      # CI: init, run, exit cleanly with no display/GPU
ctest --test-dir build              # unit tests + headless smoke tests + golden-image check
tools/golden.sh capture             # (re)record golden reference images after an intended change
```

Layout: the engine library is `engine/`; sample apps are under `apps/` (`sandbox`, `orbs`,
`swarm`, `cube`, `scene3d`, `world`, `model`, `village`, `water`).

## What works today

Milestones so far, each verified end-to-end (on a software Vulkan device where no GPU is
available):

- **M0** — window + fixed-timestep loop + Vulkan clear-screen renderer
- **M1** — batched 2D sprite renderer (textured, tinted, rotated sprites; `Camera2D`)
- **M2** — top-down tile world: `Tilemap`, WASD/arrow movement, wall/water collision, follow camera
- **M3** — TrueType text (`ui::Font` via stb_truetype) + a pixel-space HUD over the world
- **M4** — `orbs` ("ORB RUN"): a complete arcade game with a title → play → win/lose → restart
  state machine, score + timer HUD, collectibles, and hazards
- **M5** — a synthesized audio system (`maz::audio::Audio`): sound effects + looping music
- **M6** — a particle system (`maz::fx::ParticleSystem`): player spark trail + pickup/win/lose bursts
- **M7** — save/load (`maz::core::KeyValueStore`): ORB RUN keeps a high score across runs
- **M8** — an entity-component system (`maz::ecs::World`): the `swarm` demo runs 800 entities
- **M9** — 3D rendering (depth buffer + `MeshRenderer`): the `cube` demo shows a lit 3D cube under a 2D HUD
- **M10** — procedural meshes (`render::shapes`) + an ECS-driven 3D scene (`scene3d`)
- **M11** — textured 3D (shared `TextureStore`): the cube is checkered and `scene3d` has a tiled floor
- **M12** — explorable 3D (`maz::game::FlyCamera` + mouse-look): fly through `world` with WASD
- **M13** — 3D collision (`maz::game::Collision`): `world` is a solid, walkable first-person collect-em-up
- **M14** — shadow mapping: 3D objects cast soft shadows on the ground (directional light + PCF)
- **M15** — gradient skybox: a real sky (horizon → zenith + sun glow) behind every 3D scene
- **M16** — MSAA: multisample anti-aliasing (up to 4×) smooths jagged edges in every scene, 2D and 3D
- **M17** — glTF model loading (`maz::render::loadGltf` via cgltf): the `model` demo loads a
  `house.gltf` from disk and renders it with shadows + sky
- **M18** — textured glTF: the loader decodes a model's base-color texture, so `house.gltf` shows
  brick walls, a shingle roof, a plank door, and glass windows
- **M19** — glTF scene loading (`maz::render::loadGltfScene`): a whole scene (ground + 6 houses +
  9 trees) loads from one `village.gltf` — a level as data
- **M20** — **VILLAGE QUEST**: that scene becomes a playable first-person game — solid house
  collision, coins to collect against the clock, a win state, and a best time saved across runs
- **M21** — point lights (`Renderer::setLighting`): the 3D path gains ambient + a shadow-mapped sun
  + up to 8 attenuated point lights; VILLAGE QUEST uses them for a dusk village with glowing lamps
- **M22** — distance fog: meshes fade into the sky with distance (`world`, `village`)
- **M23** — gamepad support (SDL3): sticks/buttons/triggers in `platform::Input`; `world`/`village`
  are playable with a controller
- **M24** — dynamic sky + day/night: sky colors + sun are lighting parameters; VILLAGE QUEST runs a
  full day→night cycle where the sky, fog, and house lamps all respond
- **M25** — spot lights: point lights gain an optional cone (direction + inner/outer angle); a
  searchlight sweeps the village at night
- **M26** — normal mapping: tangent-space normal maps (glTF `normalTexture`) give the brick walls
  and roof shingles real per-pixel surface relief under the lights
- **M27** — post-processing bloom: the scene renders to an offscreen target and a composite pass
  adds a threshold bloom (passthrough at strength 0); VILLAGE QUEST's night lamps and coins glow
- **M28** — world-space 3D particles: camera-facing additive billboards (`drawParticle3D`); VILLAGE
  QUEST has a bonfire whose glowing embers rise, fade, and bloom at night
- **M29** — debug stats overlay (`ui::DebugOverlay`): smoothed FPS, frame time, and per-frame draw
  counts (mesh/particle/sprite) from `Renderer::renderStats`; toggle with **F3** in `world`
- **M30** — frustum culling: meshes outside the camera frustum are skipped (world-AABB vs viewProj
  planes); the overlay shows the culled count (e.g. "MESH 36 (culled 51)")
- **M31** — pickup-burst particles: collecting a coin in VILLAGE QUEST sprays a burst of glowing
  golden sparkles that drift, fade, and bloom
- **M32** — alpha-blended smoke particles: a second particle blend mode (`drawParticle3D(..., false)`);
  the village bonfire billows a smoke column above its embers
- **M33** — text alignment: `Font::drawTextCentered` centers a string about an x; VILLAGE QUEST's win
  banner and ORB RUN's title are properly centered
- **M34** — wireframe debug draw: `Renderer::setWireframe(true)` renders meshes as line polygons (a
  second `VK_POLYGON_MODE_LINE` pipeline, needs the GPU's `fillModeNonSolid` feature); toggle with
  **F4** in `world` (or launch with `--wireframe`). The 2D HUD stays solid
- **M35** — dynamic meshes: `createDynamicMesh` + `updateMesh` re-stream a mesh's vertices every frame
  (one host-visible vertex buffer per frame-in-flight, so a write never races a frame still reading);
  the new `water` demo animates a grid with summed sine waves lit and fogged under the sun
- **M36** — debug line draw: `Renderer::drawLine` / `drawAabb` render world-space colored lines (a
  `LINE_LIST` pipeline, depth-tested so geometry occludes them); **F5** in `world` overlays every
  collision box in green — a visual check that colliders match the geometry
- **M37** — emissive materials: `Renderer::drawMeshEmissive` adds a self-illumination color after
  lighting (so an object glows on its own and feeds bloom); `world`'s collectible pickups now pulse
  as glowing gold
- **M38** — HDR scene + tonemap: the offscreen scene now renders to a **16-bit float** target so
  lighting/emissive/bloom can exceed 1.0; the composite pass applies an **ACES filmic tonemap** with
  exposure (`Renderer::setTonemap`) to roll highlights smoothly into the LDR image. Off by default
  (a faithful passthrough); the `water` demo opts in with an over-bright sun
- **M39** — camera shake (game feel): `maz::game::Shake` is a trauma-based camera shake (amount =
  trauma², decays on its own) built from layered sines — no RNG, deterministic. `world` jolts the
  camera each time a pickup is collected; the 2D HUD stays fixed
- **M40** — spatial grid broadphase: `maz::game::SpatialGrid` buckets static colliders into X/Z
  cells so movement only tests nearby boxes (a `slideMove` overload uses it); `world` collides
  against the grid and **F6** overlays the occupied cells in cyan
- **M41** — separable downsampled bloom: a `BloomChain` bright-passes + 2× downsamples the HDR scene,
  then blurs it separably (horizontal then vertical) at half resolution; the composite adds the
  result. Wider, smoother glow than the old single-pass bloom, and cheaper. Bloom-off apps stay
  pixel-identical
- **M42** — specular/roughness material: `Renderer::Material` + `drawMeshMaterial` add a Blinn-Phong
  sun highlight to meshes (albedo + normal + emissive + `roughness`/`specular`); matte defaults leave
  existing draws unchanged. The `cube` demo is now a glossy checker with a highlight that sweeps as
  it spins
- **M43** — persistent graphics settings: `world` saves its bloom (**F7**), tonemap (**F8**), and
  wireframe (**F4**) choices to a `settings.ini` via `KeyValueStore` and restores them on the next
  launch; a HUD line shows the live state
- **M44** — soft shadows: the shadow-map filter widened from 2×2 to a 5×5 PCF kernel (25 taps, wider
  spread) for a soft penumbra instead of blocky edges — every shadowed 3D scene benefits
- **M45** — color grade: the composite gained an opt-in `setColorGrade` (soft **vignette** +
  saturation + contrast); off by default (no regression). VILLAGE QUEST wears a cinematic grade that
  deepens at night
- **M46** — chromatic aberration: opt-in radial RGB split at the screen edges (`setChromaticAberration`,
  off by default) for a subtle lens-fringe look; VILLAGE QUEST enables a gentle amount
- **M47** — minimap: `world` draws a top-down corner radar (blocks, uncollected pickups, player) with
  2D sprites — a worked example of a screen-space HUD map over the 3D scene
- **M48** — film grain: opt-in animated hashed noise in the composite (`setFilmGrain(strength, time)`,
  off by default); VILLAGE QUEST adds a gentle moving grain for a filmic texture
- **M49** — particle attractor: `fx::ParticleSystem::setAttractor` pulls live particles toward a
  point with an optional tangential swirl (a vortex); ORB RUN's title screen swirls an ambient
  particle cloud behind the logo
- **M50/M51** — automated tests: a dependency-free unit suite (`maz_unit_tests`) for math/collision/
  grid/ECS/particles, plus a golden-image regression harness (`tools/golden.sh`) that diffs each app
  against committed references — both wired into `ctest`
- **M52** — texture mipmaps: a blit-generated mip chain with trilinear minification (magnification
  stays NEAREST) so distant surfaces stop shimmering while near ones and the 2D UI stay crisp
- **M53** — ray vs AABB: `game::raycastAabb` / `raycast` (slab method, nearest hit) for targeting and
  interaction queries; `world` outlines the block you're looking at in orange. Unit-tested
- **M54** — per-frame scene UBO: the mesh path's camera/light matrices moved into a set-2 uniform
  buffer, shrinking the per-draw push constant from 224 bytes (over the Vulkan limit) to 96 with
  named material fields — a portability fix and cleaner material model, verified pixel-identical
- **M55** — instanced mesh rendering: `drawMeshInstanced` draws one mesh many times in a single
  `vkCmdDrawIndexed` call, the per-instance model matrix supplied through a second vertex binding.
  The `instances` demo animates 484 bobbing/spinning cubes in one draw call. Golden-image guarded
- **M56** — transparent meshes: `drawMeshTransparent` renders translucent geometry (glass, water
  panes, ghosts) with alpha blending, depth-tested but not depth-writing, sorted back-to-front by
  camera distance so overlapping surfaces composite correctly. The `glass` demo blends three
  colored panes over opaque pillars. Opaque draws are byte-identical, so existing apps are unchanged
- **M57** — A* pathfinding: `game::NavGrid` is a header-only navigation grid with 8-directional A*
  (octile heuristic, no diagonal corner-cutting) and world↔cell mapping, for moving-AI routing. The
  `maze` demo walks an agent through an obstacle field, re-planning each leg; the route is drawn as
  a debug polyline. Unit-tested (open path, walls, unreachable goals, corner rule)
- **M58** — steering behaviors: `game::Steering` adds Reynolds-style `seek`/`flee`/`arrive`/
  `separation`/`followPath` forces plus `integrate`, turning pathfinding routes into smooth moving
  agents. The `crowd` demo drives 14 agents that each A*-route through the maze, follow the
  waypoints, and separate from their neighbors so the group flows without stacking. Unit-tested
- **M59** — tweening + easing: `maz::anim` provides 15 easing curves (`ease(Ease, t)`) and a
  `Tween` time-cursor with once/repeat/ping-pong looping and a generic `sample(from, to)` that
  interpolates any float/vector/color. The `tween` demo animates 8 markers on one shared clock to
  compare curves side by side. Cross-cutting animation for UI, cameras, platforms; unit-tested
- **M60** — immediate-mode UI: `ui::Context` provides IMGUI-style `panel`/`label`/`button`/
  `toggle`/`slider` widgets with hot/active tracking (a button only fires when press and release
  land on it; a slider keeps dragging off-track), drawn via the 2D sprite+font path. The `menu`
  demo is an interactive settings screen driven by a self-playing cursor (real mouse on a desktop).
  Interaction state machine + hit-testing + slider math unit-tested
- **M61** — binary serialization: `maz::io` provides `ByteWriter`/`ByteReader` (POD/string/vector,
  versioned magic headers, bounds-checked reads that fail cleanly on truncated/corrupt input) plus
  file read/write. The `persist` demo authors a 14-prop scene, saves it to disk, clears memory,
  reloads it from the file, and renders the reconstructed scene. Round-trip + truncation + bad-magic
  unit-tested — the backbone for save games and level files
- **M62** — finite state machine: `game::StateMachine<StateId>` gives states enter/update/exit
  callbacks and guarded transitions (plus any-state transitions), evaluated deterministically. The
  `guard` demo composes it with steering: guards patrol posts until an intruder nears (→ chase),
  then return when it escapes — color-coded green/red/amber by state. Transition logic unit-tested
- **M63** — sprite-sheet animation: `anim::SpriteAnim` plays a list of UV frames (built from a grid
  sheet via `gridFrames`) at a fixed rate with loop/one-shot modes, feeding `SpriteDesc`'s uv-rect.
  The `sprites` demo generates an 8-frame sheet at runtime and plays a phase-staggered grid so a
  wave of motion sweeps across it. Frame timing + grid UVs + loop/clamp unit-tested
- **M64** — event bus: `core::EventBus` is a type-safe publish/subscribe hub — `subscribe<T>`/
  `emit<T>`/`unsubscribe` with per-type isolation and snapshot dispatch (handlers may (un)subscribe
  or emit re-entrantly). The `events` demo fires one impact event that three independent subscribers
  react to — particle burst, score tally, expanding ring — none aware of the others. Subscribe/emit/
  unsubscribe/isolation/re-entrancy unit-tested
- **M65** — job system: `core::JobSystem` is a worker thread pool with `submit<F>` (returns a
  future), `parallelFor`, and `parallelRanges` (blocking range splits) — the engine's parallelism
  foundation. The `jobs` demo computes a Julia fractal single-threaded then via `parallelFor`,
  reporting a real ~3.8× speedup on 4 cores with byte-identical output. Parallel correctness +
  futures + edge cases unit-tested
- **M66** — resource cache: `core::ResourceCache<Key,T>` is a generic reference-counted asset cache —
  `acquire` loads-once/dedups by key and bumps a refcount, `release` evicts (with an unload
  callback) when the count hits zero, plus load/hit stats. The `assetcache` demo draws a 240-tile
  mosaic that resolves to only 8 GPU textures (232 cache hits, 97% saved). Dedup/refcount/evict
  unit-tested
- **M67** — skeletal animation: `anim::Skeleton` is a joint hierarchy that precomputes bind/inverse-
  bind globals and turns an animated pose into skinning matrices (`skin = globalPose · inverseBind`).
  The `skeleton` demo binds a tapered tube to an 8-bone chain and CPU-skins it (two-bone weight
  blend) through the dynamic-mesh path — no new vertex format, so the existing mesh pipeline is
  untouched. Rest-pose identity, transform chaining, and rigid skinning unit-tested
- **M68** — animation clips: `anim::AnimClip` samples per-joint TRS keyframe tracks at a time (vec3
  lerp, quaternion slerp) into a pose with looping, and `blendPoses` cross-fades two poses — the
  playback + blending layer on the skeleton. The `animclip` demo cross-fades two authored looping
  clips (a travelling "wave" and a "coil") to drive the skinned tube. Interpolation, loop-wrap, and
  blend unit-tested
- **M69** — 2D physics: `game::PhysicsWorld2D` gives circle bodies velocity/mass/restitution,
  integrates gravity, and resolves circle-circle collisions with a normal impulse + positional
  correction (so stacks don't sink) plus static-box bouncing — real rigid-body dynamics on top of
  the existing collision detection. The `physics` demo drops 45 balls that bounce, collide, and
  stack. Momentum conservation, elastic bounce, static bodies, wall reflection, and no-sink resting
  unit-tested
- **M70** — animation controller: `anim::Animator` holds named clips and `play(name, fade)` cross-
  fades from the current clip to a new one over a duration, sampling both continuously so the blend
  is smooth — the stateful layer a game drives (idle→walk→jump). The `animator` demo auto-cycles a
  skinned character through idle/wave/coil clips with timed cross-fades (HUD shows the live fade %).
  Snap-in, cross-fade progression, and no-restart-on-repeat unit-tested
- **M71** — behavior trees: `game::bt` is a reactive behavior tree — `Sequence`/`Selector`/
  `Inverter` composites over `Action`/`Condition` leaves, re-evaluated from the top each tick so a
  higher-priority branch pre-empts a running lower one. The `behavior` demo drives agents with a
  `selector(flee, chase, patrol)` tree that reactively switches behavior as an intruder nears/recedes
  (colored by the active leaf). Tick semantics, short-circuit, and reactive preemption unit-tested
- **M72** — box physics + friction: `PhysicsWorld2D` gains **axis-aligned box** bodies alongside
  circles, with box-box, circle-box, and circle-circle contact generation feeding a unified impulse
  + **Coulomb-friction** + positional-correction resolver, so crates settle squarely on ledges and
  stack instead of sliding. The `boxes` demo drops a mix of boxes and balls onto static shelves. The
  circle path stayed byte-identical (existing `physics` golden unchanged); box-box, circle-box, and
  friction unit-tested
- **M73** — scene stack: `core::SceneStack` is the app-framework layer — a stack of `Scene`s with an
  enter/pause/resume/exit lifecycle, `push`/`pop`/`replace`/`clear` (mutations during update are
  deferred so a scene can transition itself), top-down update that stops at the first modal scene,
  and overlay-aware render. The `scenes` demo runs a Menu → Game → transparent Pause overlay flow
  (the frozen game shows through). Lifecycle, update propagation, and deferred mutation unit-tested
- **M74** — **CATCHER** (`apps/catcher`): a complete little game assembled entirely from the engine's
  own systems — the scene stack drives menu → play → game-over, the event bus fans catch/miss events
  out to scoring + gold/red particle bursts + screen-shake, 2D contact tests decide paddle-vs-coin and
  paddle-vs-hazard, and the high score is saved across runs via the `KeyValueStore`. A deterministic
  attract-mode AI plays itself (seeded RNG, fixed timestep) so the render is golden-stable
- **M75** — JSON / text data format (`maz::io::JsonValue` + `parseJson`): a human-readable companion to
  the binary save format. A tagged value over the six JSON types with insertion-ordered objects (so a
  round-trip is diff-friendly), a hand-written recursive-descent parser that **never throws** — bad
  input returns a null value plus a line/column error — and a compact-or-pretty `dump`. The new `data`
  demo builds an *entire* scene (clear color + eight sprites with shape/position/tint/bob/spin) from an
  embedded JSON document, proving the engine can be driven by editable data files, not just code
- **M76** — on-disk JSON levels: JSON file IO (`parseJsonFile`/`writeJsonFile`) closes the data
  pipeline. The new `level` demo reads `assets/levels/arena.json` **from disk** at startup — a tile
  grid (rows of single-char codes), a color palette, which codes are solid, and a list of pickups —
  builds a `game::Tilemap` + pickups from it, renders it top-down, and round-trips the level back out
  to the save directory (proving the format saves as well as loads). Edit the file, and the level
  changes with no rebuild
- **M77** — CVar / config system (`maz::core::CVarRegistry`): a central registry of named, typed,
  self-describing tunables (bool/int/float/string) with descriptions and numeric range clamps — the
  Quake-style "cvars" every subsystem registers once and everyone reads. Values set programmatically
  (typed setters clamp) or coerced from strings (for CLI flags and text configs). The `io::Config`
  bridge loads and saves the whole set as JSON, so a `config.json` drives the engine while `core`
  stays zero-dependency. The new `config` demo runs a scene whose orb count, speed, hue, brightness,
  and background grid are all read from cvars a JSON document set, with the live cvar table beside it
- **M78** — hierarchical CPU profiler (`maz::core::Profiler`): named, nestable timing zones that
  answer "where did the frame go?". Each zone reports **inclusive** time (its whole span) and **self**
  time (inclusive minus its children) — the two numbers that actually locate a hotspot — plus call
  counts and an EMA-smoothed millisecond readout. The core is time-source-agnostic (begin/end take a
  monotonic timestamp), so tests and deterministic demos feed synthetic times; real code uses a
  `ScopedZone` RAII over `steady_clock`. The new `profiler` demo renders a sample frame's zone tree as
  an indented bar chart
- **M79** — ECS scene serialization (`maz::io::SceneSerializer`): save and load a live `ecs::World` as
  JSON. Since the ECS stores arbitrary component types, the app registers each component once with its
  to/from-JSON converters (reflection-lite); then `saveWorld` emits `{entities:[{id,components:{…}}]}`
  (entities in ascending-id order, so output is stable) and `loadWorld` rebuilds the world — the
  content backbone for save games, prefabs, and an editor. The new `ecsave` demo builds a world,
  serializes it, reloads the JSON into a *fresh* world, and renders that reload — so what you see is
  entirely reconstructed from serialized data
- **M80** — input action mapping (`maz::input::ActionMap`): gameplay asks "is Jump pressed?" / "what's
  MoveX?" instead of naming a scancode. Named button actions bind any number of keyboard/mouse/gamepad
  sources (down if *any* is down; with pressed/held/released edges); axis actions combine negative/
  positive key pairs with analog stick axes, clamped to −1..1 — so WASD and a thumbstick drive the same
  action, and bindings can be rebound freely. It's SDL-free (update takes sampler callbacks), so it's
  unit-tested with synthetic input. The new `actions` demo drives an avatar entirely through mapped
  actions, with a scripted self-play OR'd with real input so it's both reproducible and playable
- **M81** — 2D transform hierarchy / scene graph (`maz::scene::TransformGraph`): the structural
  backbone for composite objects. Each node has a *local* transform (position, rotation, scale)
  relative to its parent; `update()` propagates those into *world* transforms parent-first, so moving
  or rotating a parent carries its whole subtree — a moon around a planet around the sun, a turret on
  a tank, a health bar pinned to a unit. The new `solar` demo builds a sun → planets → moons tree and
  only ever sets each pivot's rotation; the graph sweeps the planets around the sun and the moons
  around the planets automatically
- **M82** — 2D follow camera (`maz::game::CameraController2D`): the camera every 2D game needs. It
  tracks a target with a **deadzone** (a box the target moves within without scrolling — kills
  jitter), frame-rate-independent **smoothing** (the view eases toward its focus), and **world-bounds**
  clamping (the visible rectangle never scrolls past the level edge; a too-small axis is centered),
  with an optional shake offset layered on top and a `worldToScreen` helper. It's math-only (the app
  reads `center()`/`zoom()` to fill a `Camera2D`). The new `camera` demo follows an avatar around a
  world far larger than the screen, drawing the scene through the follow camera and the HUD in a
  pixel-space pass
- **M83** — time scheduler + sequences (`maz::core::Scheduler`, `maz::core::Sequence`): the "do this
  later / on a beat / in order" primitive gameplay leans on constantly. `Scheduler` runs fire-and-forget
  timers — `after(delay)` once, `every(interval, count)` repeatedly (finite or forever), `cancel()` by
  handle — catching up if a big frame spans several intervals and staying safe when a callback schedules
  more timers. `Sequence` plays an ordered script of `wait` / `call` / `span(duration, progress)` steps
  with optional looping. Both run on the fixed-step clock, so they're deterministic. The new `fireworks`
  demo is entirely timer-driven: a rocket launches every 0.4s, each schedules its own explosion into a
  particle burst, and a finale volley fires every 2s
- **M84** — deterministic RNG (`maz::core::Random`): one seeded, reproducible source of randomness for
  gameplay and procedural generation, so a given seed always yields the same world/loot/spread (the
  basis for replays, tests, and shareable seeds). It's xoshiro256** seeded through SplitMix64, with
  floats in `[0,1)`, inclusive int ranges, `chance(p)`, weighted picks, Fisher-Yates `shuffle`, a
  Gaussian, and an angle — replacing the ad-hoc xorshift each demo used to hand-roll, and stdlib-only so
  `core` keeps zero dependencies. The new `scatter` demo generates a 520-token field with weighted
  rarity from a fixed seed, tallying the resulting distribution in a legend
- **M85** — procedural noise (`maz::core::Noise`): seeded Perlin gradient noise plus fractal Brownian
  motion (`fbm2`) — the smooth, reproducible spatial randomness behind terrain, cloud/marble textures,
  cave carving, and organic motion. It's 0 at integer lattice points, continuous everywhere, bounded to
  ~[-1,1], and deterministic per seed (permutation shuffled with `core::Random`). The new `noise` demo
  turns an `fbm2` field into a terrain heightmap — colored by a water → sand → grass → forest → rock →
  snow ramp with a slope-based hillshade — so the same seed always renders the same continent
- **M86** — retained UI layout (`maz::ui::LayoutNode`), toward Godot's Control system: the engine had
  only immediate-mode widgets with hand-typed pixel positions. This adds Godot's model — **anchors**
  (each edge pinned to a fraction of the parent) + **margin offsets**, plus **container** modes
  (`HBox`/`VBox`/`Center`) that arrange children automatically with spacing, padding, min-size, and an
  `expand` flag that shares leftover space. `layout()` walks the tree from the framebuffer rect and
  computes every node's rect, so the UI is **resolution-responsive** — no coordinate is hard-coded. The
  new `uilayout` demo builds a real app shell (top bar + sidebar button list + content panel + a
  center-pinned modal with its own stacked title/body/button-row) entirely from the layout tree
- **M87** — navigation mesh (`maz::game::NavMesh`), toward Godot's NavigationServer: polygon-based
  pathfinding, the step up from grid A*. The walkable area is a set of convex cells; `build()` finds
  their shared edges (portals), and `findPath()` A*-searches the cell graph then runs the **funnel
  algorithm** to string-pull the corridor into a short, smooth path that **hugs corners** instead of
  zig-zagging through cell centers like a grid. The new `navmesh` demo routes an agent around a central
  pillar, and the path bends tightly around its corner
- **M88** — filled 2D polygons (`Renderer::drawConvexPolygon`), toward Godot's `Polygon2D` /
  `draw_colored_polygon`: the renderer could only draw textured quads. This adds arbitrary **convex
  polygon fill** — the points are triangulated as a fan and streamed through the same batched 2D
  pipeline as sprites (flat-shaded via a 1×1 white texture), so vector shapes cost nothing extra and
  alpha-composite like everything else. The new `vectors` demo draws regular N-gons (triangle through
  octagon), a 64-sided "circle," and three overlapping translucent triangles. This also unblocks 2D
  lights/shadows (which need polygon light/occluder meshes) in a later loop
- **M89** — 2D lights + shadows, toward Godot's `Light2D` / `LightOccluder2D`: `game::Visibility2D`
  computes, from a point light, the polygon of everything it can see given a set of blocking segments
  (the classic angle-sweep algorithm — cast a ray toward each occluder corner, keep the nearest hit).
  A new per-vertex-color `Renderer::drawPolygonFan` renders that polygon as a gradient triangle fan:
  bright at the light, faded to nothing at the rim. The notches the algorithm carves out behind
  occluders **are** the shadows — hard-edged, geometrically exact. The new `lights2d` demo lights a
  dark room with three colored lights and four solid boxes that cast real shadows. The visibility
  geometry is pure 2D math, so it unit-tests without a GPU
- **M90** — additive blending in the 2D renderer, toward Godot's `Light2D` compositing: the sprite
  path could only alpha-blend ("over"), so overlapping M89 lights *averaged* instead of *brightening*.
  This adds a second blend pipeline (src·alpha ADDED to the destination) sharing the same shaders and
  layout, a `BlendMode` selector on `drawConvexPolygon` / `drawPolygonFan`, and per-blend-mode
  batching (each batch binds the matching pipeline). The `lights2d` demo now composites its light
  pools additively — where two lights overlap the colors sum toward white, exactly how real light
  accumulates. Additive is also the right mode for glows, fire, and energy effects generally
- **M91** — 2D rigid-body **rotation**, toward Godot's `RigidBody2D` angular dynamics: the physics
  bodies could translate but never spin — boxes slid around always axis-aligned. This adds real
  angular dynamics: a body carries an orientation + spin and a finite moment of inertia (derived from
  its shape and mass via `enableRotation()`), and contact impulses applied at the actual contact point
  produce torque. Collision uses oriented-box SAT (`game::PhysicsWorld2D`'s oriented solver) so a box
  landing on a corner tips and tumbles, and linear/angular damping lets a pile settle. Rotation is
  opt-in (inverse-inertia defaults to 0 = locked), so every existing physics scene is byte-identical.
  The new `tumble` demo drops a stack of tilted rectangles into a bin; they fall, tumble on their
  corners, and settle into a leaning heap. Verified by unit tests (inertia values, free-spin
  integration, damping, a tilted box toppling flat) plus the golden

The engine degrades gracefully with no GPU / display / audio device, so `--headless` still runs
in CI.

### Software rendering (CI / no GPU)

```
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ./build/bin/orbs --frames 90     # Mesa lavapipe + Xvfb
```

## Assets

The bundled HUD font is **DejaVu Sans** (`assets/fonts/`, Bitstream Vera / public-domain-style
license — see `DejaVuSans.LICENSE.txt`).

The sample models `assets/models/house.gltf` and `village.gltf` are generated by
`tools/make_house_gltf.py` and `tools/make_village_gltf.py` — self-contained glTF 2.0 files authored
in-repo, with their geometry *and* a base-color detail texture (a brick/shingle/plank/glass atlas)
base64-embedded in one buffer, so there are no third-party model assets to license.

## Controls

- **Sandbox** (top-down demo): WASD / arrows to move · Esc to quit
- **ORB RUN**: Space to start / restart · WASD / arrows to move · Esc to quit
- **VILLAGE QUEST**: WASD to move · mouse to look · collect every coin · Esc to quit
- **CATCHER**: ← / → (or A / D) to move the paddle · catch gold coins, dodge red hazards · plays itself in attract mode · Esc to quit
- **Data** (data-driven scene): no controls — the whole scene is parsed from an embedded JSON document · Esc to quit
- **Level** (on-disk JSON level): no controls — the level is loaded from `assets/levels/arena.json`; edit that file to change it · Esc to quit
- **Config** (cvar / config demo): no controls — the scene is driven by cvars a JSON config sets; the cvar table is shown live · Esc to quit
- **Profiler** (CPU profiler view): no controls — a sample frame's timing zones are drawn as an indented bar chart · Esc to quit
- **Ecsave** (ECS save/load): no controls — a world is serialized to JSON, reloaded into a fresh world, and that reload is rendered · Esc to quit
- **Actions** (input action map): WASD / arrows move · Space fires · Shift dashes (also plays itself via scripted input) · Esc to quit
- **Solar** (transform hierarchy): no controls — a sun → planets → moons scene graph animates from pivot rotations alone · Esc to quit
- **Camera** (2D follow camera): no controls — the camera tracks a moving avatar with deadzone + smoothing + world-bounds clamp · Esc to quit
- **Fireworks** (scheduler): no controls — timers launch and explode fireworks; the whole show is scheduler-driven · Esc to quit
- **Scatter** (procedural RNG): no controls — a seeded RNG scatters a token field with weighted rarity; same seed → same field · Esc to quit
- **Noise** (procedural terrain): no controls — an fbm-noise heightmap rendered as a terrain map; same seed → same continent · Esc to quit
- **UILayout** (retained UI): no controls — a responsive app UI (top bar + sidebar + content + modal) laid out by anchors and containers · Esc to quit
- **NavMesh** (navigation mesh): no controls — an agent path routed around a pillar with A* + funnel string-pulling · Esc to quit
- **Vectors** (filled polygons): no controls — regular N-gons, a 64-gon circle, and translucent overlapping triangles · Esc to quit
- **World** / **Village** (explore): WASD move · mouse look · Esc to quit
