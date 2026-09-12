# Maz Engine

[![CI](https://github.com/Mazoo85/Maz/actions/workflows/ci.yml/badge.svg)](https://github.com/Mazoo85/Maz/actions/workflows/ci.yml)

A native **C++20 + Vulkan + SDL3** game engine — 2D-complete with a working 3D path (a
pluggable renderer, 2D and 3D cameras, a depth buffer, and lit meshes alongside batched sprites).

Continuous integration builds the engine and runs the full test suite (8900+ checks) on
**Linux, macOS, and Windows** on every push — see [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

---

## Also in this repository: MAZ ARCADE

Alongside the engine, a set of browser projects that run offline from a static
file server with no build step and no dependencies. The front door is
**[`index.html`](index.html)** — the hub — and every app carries a small MAZ pill
in its corner that leads back to it.

| | Project | What it is |
|---|---|---|
| 🎬 | **SCRIPT FORGE** — [open](film/) · [docs](film/README.md) | Type what your film is about and get the film: a formatted screenplay, a shot list, and an animated short with jointed characters, layered sets, weather, moving light and a composed score, that plays in the page and downloads. |
| 🎵 | **SONG FORGE** — [open](music/) · [docs](music/README.md) | Writes and plays complete songs — chords, bass, drums, arpeggio, melody — across 8 genres, with WAV and MIDI export. It is also the thing that scores a SCRIPT FORGE film. |
| 🎮 | **NEON CELLS** — [play](cells/) · [docs](cells/README.md) | Roguelite action-platformer in the shape of Dead Cells: procedurally built biomes proved reachable before you enter them, permadeath, three scroll colours, weapons with rolled affixes, skills, mutations and two bosses. Plays on a controller, a keyboard or a phone. |
| 🎮 | **ZOMBOID: ANCHORAGE** — [play](zomboid/) · [docs](zomboid/README.md) | Open-world zombie survival across a tile-built replica of downtown Anchorage, drawn as a 1990s SEGA arcade title. |
| 🎮 | **DEAD SECTOR** — [play](shooter/) · [docs](shooter/README.md) | Phone-first top-down twin-stick shooter in one self-contained HTML file. |
| ✍️ | **MADLIBS STORY FORGE** — [open](madlibs/) · [docs](madlibs/README.md) | Forges story ideas broken into scene beats, ready to seed a script. |
| 🕸️ | **maz-scrape** — [docs](scraper/README.md) | Recipe-driven scraper for static HTML: point it at a YAML recipe, get JSONL/CSV/SQLite. |
| 🧰 | **crew** — [docs](crew/README.md) | Small shared tooling. |
| ⚒️ | **The Forge** — [docs](docs/FORGE.md) | The repo's own nightly loop: reads the roadmap, CI and `TODO` markers, picks one task, hands it to Maz Crew, and opens a draft PR. Verifies against every project declared to depend on what it changed. Never pushes to the default branch, never merges. |

The arcade has its own CI — [`site-ci.yml`](.github/workflows/site-ci.yml) checks
that every cross-link resolves, that no project reaches into another without
declaring it, and drives the hub and each app in a real Chromium;
[`music-ci.yml`](.github/workflows/music-ci.yml) covers SONG FORGE,
[`scraper-ci.yml`](.github/workflows/scraper-ci.yml) maz-scrape,
[`crew-ci.yml`](.github/workflows/crew-ci.yml) Maz Crew, and
[`forge-ci.yml`](.github/workflows/forge-ci.yml) the Forge itself. None of them
touch the engine build. [`all-checks.yml`](.github/workflows/all-checks.yml)
runs every suite in the repository — engine included — behind one button, so
"is the whole repository healthy?" has a single answer.

See **[`docs/ROADMAP.md`](docs/ROADMAP.md)** for the full build plan (the "massive list") and
**[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** for the module map and design principles.

**API reference:** **[`docs/API.md`](docs/API.md)** is a browsable index of every module, type, and
free function, generated from the header doc-comments by `python3 tools/gen_api_docs.py` (no external
tools). For full HTML docs, contributors with Doxygen installed can run `doxygen Doxyfile`
(→ `docs/api-html/index.html`). See also **[`docs/SCRIPTING.md`](docs/SCRIPTING.md)** for the
`maz::script` language guide.

## Building

**New to this / no coding experience?** Start with **[`docs/EDITOR_GUIDE.md`](docs/EDITOR_GUIDE.md)** —
a plain-language, step-by-step guide to turning the source into the **Editor program** (a Godot-style
tool: place objects, tweak them, press Play). It uses the one-command helpers
`tools/build_editor.sh` (macOS/Linux) and `tools/build_editor.bat` (Windows), which check your tools
and build the Editor for you.

For the manual path:

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
- **M92** — animation **blend spaces**, toward Godot's `AnimationTree` BlendSpace1D/2D: the engine
  had clip sampling and 2-way pose crossfade, but no way to blend animations by a *parameter*.
  `anim::BlendSpace1D` blends the two neighbouring samples along a line; `anim::BlendSpace2D` places
  samples in a plane and returns barycentric weights over a triangulation (the same model Godot uses,
  where a query point picks its enclosing triangle). A new `anim::blendPosesWeighted` mixes N weighted
  poses into one. The new `blendspace` demo renders a grid of stick-figure skeletons, one per sampled
  (x,y) cell, so you watch a single skeleton's pose morph smoothly between four corner poses across
  the space — exactly how a Godot BlendSpace2D drives 8-way locomotion. Pure geometry, so the weights
  unit-test without a GPU
- **M93** — 2D physics **joints**, toward Godot's `PinJoint2D` / `DampedSpringJoint2D`: the rigid-body
  world could collide bodies but not *connect* them. `game::Joint2D` adds two constraints solved by
  sequential impulses inside the oriented step: a **Pin** forces two anchor points together (a hinge /
  rope link — a 2×2 effective-mass solve with Baumgarte drift correction), and a **Spring** pulls two
  anchors toward a rest length with stiffness + damping. Either end can instead be a fixed world point.
  The new `joints` demo builds a rope bridge — a chain of boxes pinned end-to-end between two posts
  that sags into a catenary — and a row of masses hung from springs of increasing stiffness. Verified
  by unit tests (a pin pendulum holds its arm length while it swings; a stretched spring returns to
  rest) plus the golden
- **M94** — 2D positional audio, toward Godot's `AudioStreamPlayer2D`: audio was a flat, non-spatial
  mixer. `audio::spatialize` computes, from a listener (position + a "right" axis) and a source
  position, the source's per-channel gain — a distance **attenuation** (linear or inverse-distance,
  silent past a max range) times a **constant-power stereo pan** (a source off to one side is louder in
  that ear). The SDL mixer is now stereo and applies a left/right gain per voice (`SoundDesc::leftGain`
  / `rightGain`), so blips actually move across the stereo field on a real device. The new `spatial2d`
  demo visualizes the field — each source's halo scales with its gain and a small L|R bar shows its pan,
  with the listener's range rings and a master stereo meter. The spatializer is pure math, unit-tested
  headlessly (attenuation curve, hard-left/right pan, silence past max range, constant-power invariant)
- **M95** — UI **text input + focus navigation**, toward Godot's `LineEdit` + Control focus system: the
  UI had buttons/toggles/sliders but no editable text. `ui::TextField` is a single-line edit model
  (caret + insert / backspace / delete / arrows / home / end + max length), `ui::FocusChain` is an
  ordered set of focusable widgets with Tab / Shift+Tab wraparound and click-to-focus, and a new
  `Context::textField` widget draws the box + text + caret and applies per-frame typed characters and
  editing keys to whichever field owns focus. Both models are pure logic, unit-tested headlessly. The
  new `form` demo is an account-settings form of four fields — click or Tab to move focus (the focused
  field shows an accent border + caret), type to edit, one field length-capped
- **M96** — procedural **cave generation + tilemap autotiling**, toward Godot's TileMap terrain sets:
  `game::CellularCave` grows an organic cavern by cellular automata (seeded random fill, then smoothing
  passes that keep a cell solid where its neighbours are mostly solid — the "4-5 rule", borders forced
  solid), and `game::autotileMask4` computes each wall cell's 4-bit edge mask (which N/E/S/W neighbours
  are also wall, out-of-bounds counted solid) — the value a terrain tileset keys on to pick a border
  tile. Both are pure logic (only `core::Random`), deterministic under a seed, and unit-tested. The new
  `cave` demo generates a seeded cave and renders each wall cell inset on the sides that face open floor
  (driven by its mask), so the walls round off into smooth cave borders
- **M97** — 2-bone **inverse kinematics**, toward Godot's `SkeletonModification2DTwoBoneIK`: given a
  fixed shoulder, two bone lengths, and a target, `anim::solveTwoBoneIK` places the elbow (via the law
  of cosines) so the hand lands on the target, with a `bendSign` choosing which way the elbow bends;
  when the target is out of reach the arm points straight at it, fully extended. Pure 2D math,
  unit-tested (the hand reaches the target with bone lengths preserved; the bend flips the elbow to the
  other side; overreach gives a straight arm). The new `reach` demo is a grid of arms each solving
  toward its own target — reachable targets ringed green, out-of-reach ones red with the arm extended
- **M98** — RVO **local collision avoidance**, toward Godot's `NavigationAgent2D` avoidance: given an
  agent's *preferred* velocity (usually "toward my goal") and its moving neighbours,
  `game::rvoVelocity` returns a nearby velocity that won't run into them. It samples candidate
  velocities (the preference, a full stop, and a fan of directions × speeds) and scores each on the
  soonest collision it would cause — using the **reciprocal** relative velocity `2·c − vA − vB`, so
  both agents share the dodge and a head-on pair peels apart instead of oscillating. Pure 2D math,
  unit-tested (no neighbours returns the preference exactly; a blocker dead ahead forces a sideways
  deviation; two agents on a head-on crossing both reach the far side without ever overlapping). The
  new `avoid` demo is the classic circle test — 14 agents each heading for the point opposite, their
  trails bulging outward around the crowded centre; the whole crossing is simulated once at startup so
  the render is deterministic.
- **M99** — audio **DSP effects + mix buses**, toward Godot's `AudioEffectFilter`/`AudioEffectDelay`
  and audio-bus layout: a header-only `audio::Biquad` (RBJ-cookbook low/high/band-pass filters), an
  `audio::Delay` (feedback echo), and an `audio::Bus` that chains effects in series with an output
  gain. All pure per-sample math — no device, no threads — so it unit-tests exactly (low-pass passes
  DC and low tones but kills highs; high-pass blocks DC; an impulse re-emerges after exactly one delay
  length, then a quieter echo at twice that; a bus chain preserves order) and drives a deterministic
  offline scope. The new `bus` demo plays one plucked-sawtooth note and draws four stacked waveform
  bands — the raw source, a low-pass (harmonics smoothed away), a high-pass (only the bright edges),
  and a low-pass→delay bus (filtered note + decaying echoes) — computed once at 44.1 kHz.
- **M100** — animation **keyframe timeline / sequencer**, toward Godot's `AnimationPlayer`: `anim::Timeline`
  is a set of named `Track`s, each a list of `Keyframe`s (time → value + per-segment easing) that
  `sample()` interpolates (holding the endpoints, no extrapolation), plus a playhead that advances over
  the clip length with a Once/Repeat/PingPong loop. Where a `Tween` animates one value A→B, a timeline
  animates many named properties through arbitrary keyed poses at once — the backbone of cutscenes and
  property animation. Pure math, unit-tested (endpoint hold, midpoint + eased interpolation, out-of-
  order key insertion stays sorted, independent tracks, loop wrap, ping-pong reflection). The new
  `timeline` demo drives one arrow from keyed x/y/rotation/scale/colour tracks, drawn as an onion-skin
  trail plus an editor-style track panel with keyframe dots and a playhead — all sampled at fixed times
  so the render is deterministic.
- **M101** — soft (penumbra) **2D shadows**, toward Godot's `Light2D` soft shadows: a point light casts
  a razor-sharp shadow, but a light with *size* has soft edges. `game::SoftShadow2D` models the light as
  a disc, spreads deterministic sample points across it (`diskSamples`, a Vogel spiral), and its
  `softVisibility(p, …)` returns the fraction of the light disc visible from a point — 1 fully lit, 0
  umbra, in between a penumbra. Pure 2D math, unit-tested (segment-intersection LOS, sample count/within-
  radius/determinism, no-occluder = 1, fully-blocked = 0, partial = penumbra). The new `softshadow` demo
  draws the same box+light twice — left a hard point light (crisp edge), right an area light (24 samples,
  composited as additive visibility fans) whose shadow feathers into a penumbra that widens with
  distance from the caster.
- **M102** — a **groove / slider joint** (`Joint2D::Groove`), completing Godot's 2D joint set
  (PinJoint2D, DampedSpringJoint2D, **GrooveJoint2D**): a body is pinned to a *line* (the groove) on
  another body — free to slide along it but held on the line. Solved with a single perpendicular impulse
  + Baumgarte bias inside the existing oriented step; purely additive, so the Pin/Spring paths (and
  every existing physics golden) are byte-identical. Unit-tested (an off-line slider is pulled onto the
  groove while its along-groove position stays free; a tilted groove holds the body on its line under
  gravity while it slides down the incline). The new `groove` demo drops three boxes onto tilted rails —
  each slides down its incline (not straight down) and settles against a stop block.
- **M103** — **nine-patch / StyleBox** UI, toward Godot's `StyleBoxTexture` theming: `ui::ninePatch`
  slices a destination rectangle into a 3×3 grid by fixed border insets and maps each cell to a matching
  source region — the four corners keep their exact size, the four edges stretch along one axis, and the
  center stretches both, so a bordered/rounded panel scales to any size without distorting its corner
  art. Pure geometry, unit-tested (corner sizes fixed across destination sizes, edges/center absorb the
  stretch, the nine cells tile source and destination exactly, sub-border sizes clamp non-negative). The
  new `stylebox` demo themes four differently-sized panels + a button row from one style — gold corners
  stay fixed while blue edges stretch and the dark center fills.
- **M104** — behavior-tree **blackboard + parallel/decorator nodes**, extending the reactive BT (M71)
  toward a full Godot-style BT AI: a `bt::Blackboard` (typed shared working memory — set/get/getOr),
  a `Parallel` composite (RequireOne/RequireAll, ticks every child), and `Repeater`/`AlwaysSucceed`/
  `AlwaysFail`/`Tap` decorators. Purely additive — the existing Sequence/Selector/Inverter/Action/
  Condition are untouched. Unit-tested (blackboard typing + safe fallback; parallel policies + all-
  children-ticked; repeater count + failure abort; decorator status mapping; a blackboard flag driving a
  reactive selector). The new `blackboard` demo draws a sentry's tree twice — with `visible` false
  (PATROL) and true (ENGAGE) — each node box coloured by the *real* per-tick status, so you can watch
  the selector switch branches from one blackboard flag and leave the unused branch un-ticked.
- **M105** — an animation **state machine** (`anim::AnimStateMachine`), toward Godot's
  `AnimationNodeStateMachine`: a graph of named states with **cross-fading** transitions (a fade time +
  an optional condition, plus explicit `travel()`), whose `active()` reports the current state(s) and
  blend weights that sum to 1 — the same shape as a blend space's weights, so a state can itself *be* a
  blend space (state weight × leaf weight) and the two compose. Pure logic, unit-tested (start state,
  auto-transition on condition, cross-fade weights partition to 1 with a real midpoint, instant fade-0
  switch, `travel()`, only-outgoing transitions fire, blend-space composition). The new `statemachine`
  demo steps a locomotion machine (idle/move/jump) over a scripted speed+jump timeline and draws the
  active-state weights as stacked colour bands — every cross-fade shows as one colour smoothly giving
  way to the next, and the `move` band is itself a walk→run blend space shaded by speed.
- **M106** — audio **reverb / distortion / compressor** effects, extending the DSP core (M99) toward
  Godot's `AudioEffectReverb`/`Distortion`/`Compressor`: `audio::Reverb` is a Schroeder/Freeverb (four
  parallel damped comb filters + two series allpasses, wet/dry), `audio::Distortion` a tanh soft-clip
  waveshaper (drive), and `audio::Compressor` a peak-envelope dynamic-range compressor (threshold/ratio/
  attack/release) — all drop into the `Bus` as effects. Purely additive to the DSP module. Unit-tested
  (distortion odd/monotonic/dynamics-compressing; compressor tames a loud signal while passing a quiet
  one; reverb is a pass-through at wet 0 and rings out a decaying tail otherwise; comb feedback repeats
  on schedule). The new `reverb` demo scopes one source (a loud note + a quiet note) through each effect
  as stacked waveforms — the reverb tail rings past the notes, the distortion fattens/clips them, and the
  compressor pulls the loud note down toward the quiet one.
- **M107** — multi-bone **FABRIK IK chains** (`anim::solveFabrik`), extending the closed-form 2-bone IK
  (M97) toward Godot's `SkeletonModification2DFABRIK`: given a chain of joints (the first fixed) and a
  target, Forward-And-Backward-Reaching IK iterates a backward pass (pin the tip to the target, drag the
  chain toward the base) and a forward pass (re-pin the base, push back out), keeping every bone length,
  until the tip reaches the target; a target past the chain's total length just straightens it. Pure 2D
  math, unit-tested (reaches a reachable target with all bone lengths preserved and the base fixed;
  straightens collinearly toward an unreachable target; single-bone + degenerate cases). The new
  `tentacle` demo anchors a row of 8-bone chains along the floor, each reaching for its own target —
  reachable ones curl to touch it (green), out-of-reach ones straighten and point (red).
- **M108** — a **GOAP planner** (`game::goap`), goal-oriented action planning that goes *beyond* the
  behaviour tree (M104) — a planner Godot ships no built-in equivalent for. Instead of hand-authoring what
  an agent does, you give it a GOAL (a desired world-state) and a LIBRARY of actions — each a
  precondition, effects, and cost — and it runs A* over world states (packed as a 64-bit fact bitmask) to
  return the *cheapest* action sequence that reaches the goal, re-planning automatically if you add a new
  action. Pure integer/graph search, unit-tested (finds the optimal plan and its cost; switches routes
  when costs change; empty plan when the goal already holds; reports unreachable goals). The new `goap`
  demo has a survival agent plan "make fire" from six actions; it draws the computed plan as a flow with
  the five world-facts turning green step-by-step until fire lights, and dims the pricey "scavenge wood"
  shortcut A* rejected.
- **M109** — **StyleBoxFlat + a Theme server** (`ui::StyleBoxFlat` / `ui::Theme`), the other half of
  Godot's theming (M103 gave the nine-patch `StyleBoxTexture`). A `StyleBoxFlat` is a procedurally-drawn
  panel — background fill, border, per-corner radius, and a soft drop shadow — with no texture asset;
  `roundedRectPolygon` builds the (convex) rounded-rect outline and `drawStyleBoxFlat` layers shadow →
  border → fill through the 2D polygon renderer. A `Theme` names those styles per control class + state
  ("Button/normal", "Button/hover", …) and resolves them with a fallback ("type/state" → "type/normal" →
  default), like Godot's Theme resource. Pure geometry + a registry, unit-tested (sharp vs rounded vertex
  layout, radius clamped to half the side, content-margin insets, and theme set/get/fallback). The new
  `theme` demo builds one dark theme and draws a button in each state through it, plus a gallery of the
  individual features — sharp, rounded, thick border, soft shadow, a max-radius pill, and a top-corners-
  only tab.
- **M110** — **2-point contact manifolds** for stable box stacks (`PhysicsWorld2D::solveManifolds`). The
  oriented rigid-body solver resolved each box pair at a single contact point — enough to stop overlap,
  but with no torque balance a stacked box slowly rotates off and the tower topples. This adds proper
  reference/incident-face clipping (Box2D-style) so a box resting on another is held at **two** points
  along the shared face, keeping the stack square. It is opt-in (default off) so every existing rotating
  scene keeps its exact single-point numerics; new scenes set `solveManifolds = true`. Unit-tested (a
  clipped manifold returns two points on the shared face with the right normal and penetration, a
  separated pair reports none, and a settled tower stays near-upright with manifolds on where the
  single-point solver lets it rotate away). The new `stack` demo drops two identical five-box towers side
  by side — with manifolds on the tower stands square, with them off the same tower topples.
- **M111** — **normal-mapped 2D lighting** (`game::PointLight2D` + `shadeSurface`), toward Godot's
  Light2D with a normal map. A flat 2D surface carries a normal map (a per-texel surface normal); a 2D
  point light then shades each texel by how squarely its normal faces the light (a 3D N·L Lambert term
  with the light taken at a height above the plane, plus smooth distance falloff), so bumps read as
  three-dimensional — lit on the side facing the light, shadowed on the far side. Earlier lights
  (M89/M101) were flat-coloured pools with occluder shadows but no surface relief; this adds it. Pure
  vector math, unit-tested (a flat texel under a light overhead is fully lit; a normal tilted toward the
  light beats one tilted away; back-facing and out-of-range texels get nothing; ambient keeps unlit areas
  from going black and strong light clamps to white). The new `normalmap` demo lights a field of dome
  bumps with three coloured point lights — each dome catches its nearest light on the facing side, and
  the warm / cool / magenta pools overlap and mix.
- **M112** — **flow-field (vector-field) pathfinding** (`game::FlowField`), the crowd-movement technique
  the per-agent A* of Godot's NavigationServer (and Maz's own `NavGrid`) doesn't provide. When many
  agents share one goal, you run a single Dijkstra *outward* from the goal to get an integration field
  (least cost-to-goal per cell), then bake a flow field — each cell stores a unit direction down that
  cost gradient. Every agent then navigates for free by reading the direction under its feet, so a whole
  crowd routes around obstacles to the goal for the price of one search. 8-connected Dijkstra (diagonal
  cost √2, no corner cutting), unit-tested (cost rises with distance and every cell flows toward the goal;
  a wall forces cells to route around rather than into it; a walled-off region is unreachable with zero
  flow; `sampleFlow` maps a world position to its cell). The new `flowfield` demo shows the integration
  field as a heat map, the baked flow as a grid of arrows, and 90 agents released on the left that stream
  around two staggered barriers to the goal, their trails tracing the flow lines.
- **M113** — **call-method / trigger tracks** (`anim::TriggerTrack` + `MethodTimeline`), the event half of
  Godot's AnimationPlayer (M100's Timeline gave *value* tracks that interpolate a property; this gives
  *method* tracks that **fire** at a keyframe time — the hook a clip uses to play a footstep on the plant
  frame or spawn a muzzle flash on the shoot frame). A `TriggerTrack` is a sorted list of timed markers;
  `MethodTimeline` plays one over a clip with a loop policy and reports which markers fired each `update`,
  with fire-once half-open semantics and correct loop-wrap (no double-trigger at the seam). Unit-tested (a
  forward sweep fires only the markers inside its interval; a marker exactly at the clip end fires on a
  Once clip; a single big step crosses several in order; a Repeat clip fires each marker once per pass; a
  zero-length clip is a safe no-op). The new `sequencer` demo is a four-lane drum machine (kick / snare /
  hat / clap) whose markers fire as one shared playhead sweeps the loop — markers behind the head glow
  ("just fired"), ahead stay dim ("pending"), with per-lane fire counts and a recent-fires strip.
- **M114** — an **ADSR envelope generator** (`audio::ADSR`), the amplitude contour that shapes a synth
  voice. On note-on the level ramps 0 → 1 over *attack*, falls to the *sustain* level over *decay*, and
  holds while the key is down; on note-off it ramps to 0 over *release*. Multiplying a raw oscillator by
  this level turns a flat buzz into a note with a shape. It's a tiny gated state machine advanced by
  `process(dt)`, unit-tested (attack halfway is ~0.5, decay settles at sustain and holds, release scales
  from the current level to 0 — including a release started mid-attack — and zero-length segments snap;
  sustain=1 makes decay a no-op). The new `envelope` demo shows three presets — a plucky blip, a
  slow-swelling pad, and a percussive stab — each as its ADSR curve and the tone shaped by it, so the same
  sine becomes three different notes.
- **M115** — a **Tree / TreeItem widget** (`ui::Tree`), Godot's most-used control (its scene dock,
  inspector, and FileSystem dock are all Trees). A `TreeItem` holds text, an id, a colour, a `collapsed`
  flag, and children; `Tree::visibleRows()` flattens the currently-expanded items depth-first into rows,
  each carrying its indent depth and whether it has children (so the view draws a fold arrow). Folding a
  branch hides its whole subtree in one flag. Pointer-stable (heap-owned children), unit-tested (row
  order/depth/hasChildren fully expanded; collapsing a branch drops exactly its subtree; a deep collapse
  hides only below it; a leaf collapse is a no-op; empty tree yields nothing). The new `tree` demo shows a
  project file tree inside a rounded StyleBoxFlat panel (composed with M109) — folders and files indented
  by depth with fold arrows, two folders collapsed, and the selected row highlighted.
- **M116** — **Area2D sensor / trigger regions** (`game::Area2D`), toward Godot's `Area2D`: a zone (circle
  or box) that doesn't push anything — it just detects which bodies *overlap* it and fires **enter / exit**
  events as they cross its boundary. `overlaps()` handles circle-circle, box-box (AABB), and the mixed
  circle-box case (closest-point on the box); an `AreaMonitor` diffs each frame's overlapping set against
  the last to emit exactly the agents that just entered and just exited (deduping repeats). Unit-tested on
  the overlap geometry (including exact-touch = not overlapping) and on multi-frame enter/exit diffing. The
  new `area2d` demo streams 14 agents across a circular "aura" and a box "gate" sensor: each zone shows how
  many agents are inside now and how many enter/exit events fired, and every agent currently inside a zone
  is drawn lit with a ring in that zone's colour.
- **M117** — an **animation blend tree** (`anim::BlendTree`), Godot's `AnimationNodeBlendTree`: the node
  graph that *nests* the blend space (M92) and cross-fades of a state machine (M105). Leaf `Input` nodes
  pull poses from an external table; interior `Blend2` (cross-fade), `Add2` (additive layer), and
  `BlendSpace1` (1-D blend space over child nodes) nodes combine them, each reading a named blend
  parameter — so "a walk/run blend space, layered with an additive wave, cross-faded into a jump by an
  air parameter" is one evaluable tree. Pure pose math, unit-tested (each node type + a nested tree +
  degenerate cases). The new `blendtree` demo drives a stick figure through exactly that tree on a grid:
  columns sweep gait (idle → walk → run), rows sweep air (grounded → jump).
- **M118** — **collision layers & masks** (`game::CollisionLayers`), Godot's `collision_layer` /
  `collision_mask`: the other half of the collision system. Overlap geometry answers "do these shapes
  touch?"; layers answer "should they even be considered?". Each object lives in some *layers* ("what I
  am") and scans some *mask* ("what I react to"), so a pickup magnet ignores enemies and an enemy
  hurtbox ignores coins even when they physically overlap. Pure 32-bit bitmask logic — a directional
  `detects()` (Area2D/ray-style), a symmetric `interact()` (physics-pair-style), a `CollisionObject2D`
  with per-bit editing, and a named-layer registry — all unit-tested. The new `layers` demo streams
  three species (player/enemy/pickup) through a hurtbox that watches only enemies and a magnet that
  watches only pickups; matching agents light up, overlapping-but-ignored ones get a faint dashed ring.
- **M119** — a **TileSet resource with per-tile collision** (`game::TileSet`), toward Godot's
  `TileMap`/`TileSet`: a Tilemap is just a grid of numbers; a TileSet gives each number *meaning* — which
  atlas cell to draw it with, and what collision it contributes. Godot's per-tile collision can be
  *sub-cell* (a half-height platform, a shelf), which the Tilemap's single "solid" bit can't express, so
  one grid can mix full walls with thin ledges. `TileDef` carries an atlas source cell + a None/Full/Box
  collision; free queries turn a `(Tilemap, TileSet)` pair into world-space collision boxes
  (`collectSolids`), a point-solidity test (`solidAt`), and a drop-to-ground helper (`dropY`). All pure
  geometry, unit-tested. The new `tileset` demo builds one level mixing solid ground/wall tiles with
  half-height ledges, draws every tile's collision box, and drops probe balls that rest exactly on
  whatever they hit — the ones on ledges sitting mid-cell.
- **M120** — a **particle emitter resource** (`fx::Emitter`), toward Godot's `CPUParticles2D`: the older
  `fx::ParticleSystem` emits point bursts with a linear start→end colour/size; a real emitter is a
  *resource* you author once and reuse, with an emission **shape** (point / disk / ring / rectangle),
  per-lifetime **curves** for scale and alpha (`fx::Curve`), and a multi-stop colour **gradient**
  (`fx::Gradient`). `fx::simulate(emitter, seed, t)` runs the whole thing **deterministically** and
  returns every live particle's position/size/colour, so it unit-tests headlessly and renders a
  golden-stable snapshot. The new `emitter` demo stands up three resources at once — a gravity fountain
  (point + fire gradient), an omnidirectional burst (ring, all-at-once), and angled rect rain.
- **M121** — **3D spatial audio** (`audio::Spatial3D`), toward Godot's `AudioStreamPlayer3D`: M94 gave 2D
  pan/attenuation on a plane; a 3D source heard by a 3D listener needs three more things. A `Listener3D`
  carries a **forward/up basis** (an orientation), so `panPosition` places the source left/right relative
  to where the listener is *facing* (turn around and the pan flips); there are four Godot **attenuation
  models** (`None` / `Linear` / `Inverse` / `InverseSquare`); and `dopplerPitch` raises pitch when source
  and listener close on each other, lowers it when they part. `computeSpatialMix` returns one voice's
  left/right gain + pitch. It's pure math (no device), so it unit-tests headlessly; the new `spatial3d`
  demo is a top-down **radar** — six sources around one listener, each a disc sized by loudness, tinted
  warm (approaching) or cool (receding), with a velocity arrow and an L/R stereo meter.
- **M122** — **auto-layout containers** (`ui::Container`), toward Godot's `Container` nodes: M86 gave an
  anchor tree with a *simple* box where every expander took an equal slice. Godot's real container model
  is what you actually need for a resizable UI — per-axis **size flags** (each child picks Fill / Expand /
  Shrink for its horizontal and vertical axis), **stretch ratios** (two expanders at 1 and 3 split leftover
  1:3, not 50/50), a true **GridContainer**, plus `MarginContainer` / `CenterContainer`, and bottom-up
  minimum-size helpers so nested containers size right. All pure rectangle math (`hbox`/`vbox`/`grid`/
  `margin`/`center`). The new `containers` demo lays out four labelled cards — every tile's rectangle is
  computed by the container, no hand-typed coordinates.
- **M123** — **parallax scrolling backgrounds** (`game::Parallax`), toward Godot's `ParallaxBackground`:
  several background layers that scroll at *different rates* relative to the camera so a flat 2D scene
  reads as deep (distant mountains barely move, near trees race past), each layer **mirror-tiled** so a
  small motif covers an unbounded scroll. `layerOffset` slides a layer by its `motionScale`; `firstTile` /
  `tileCount` / `pmod` place the seamless tiles. The new `parallax` demo shows the same five-layer scene in
  three strips at different scrolls — look down a column and the near layers sweep while the far ones
  barely budge.
- **M124** — a **tween sequencer / property animator** (`anim::TweenPlayer`), toward Godot's
  `SceneTreeTween`: `anim::Tween` (M59) is a single time-cursor you read yourself; this is the "create a
  tween, chain a few property animations, fire and forget" runtime that *drives bound values itself*.
  `appendProperty` / `appendInterval` / `appendCallback` chain steps in sequence, `parallelProperty` runs
  steps together, `setLoops` replays the whole thing, and `update(dt)` writes each property's eased value
  through its setter. The new `choreo` demo snapshots five choreographies (sequential, parallel, delay,
  loop, bounce) at a fixed time — every dot placed by its player, nothing hand-positioned.
- **M125** — **prefabs / instancing** (`scene::Prefab`), toward Godot's `PackedScene`: a reusable node-tree
  *template* you author once and instantiate many times, each instance applying per-node property
  *overrides* so every copy differs without duplicating the definition. A `PrefabNode` holds a name, an
  exported `PropBag` (Float/Int/Bool/Vec2/Color/Text) and children; `instantiate(prefab, overrides)`
  deep-copies the tree and overlays overrides by node path, returning a fully independent instance. The new
  `prefab` demo authors one turret prefab (chassis → turret → barrel) and stamps out six instances — each
  with a different body colour, turret tint, barrel length, and body width.
- **M126** — a **2D polyline / Line2D renderer** (`render::buildPolyline`), toward Godot's `Line2D`: the
  renderer could fill convex polygons, but a *stroke* thickens a path into a ribbon and shapes its corners
  (**joints**: miter / bevel / round) and ends (**caps**: none / box / round) so it reads as one line —
  the primitive behind trails, curves, graphs, and outlines. It returns a triangle soup fed straight to
  `drawConvexPolygon`. The new `line2d` demo strokes a zig-zag under each joint mode, a bar under each cap
  mode, an 80-point sine curve, and a closed star loop.
- **M127** — **`.tscn`/`.tres` text resources** (`io::savePrefabText` / `io::loadPrefabText`), toward
  Godot's text scene format: M125 gave prefabs an in-memory template; this makes them a *disk resource* you
  can read, diff, and version-control as plain text. A prefab serializes to `[node name="…" parent="…"]`
  sections with typed `key = TYPE value` lines and parses straight back into an identical tree. The new
  `restext` demo authors an "Enemy" prefab, renders its serialized text, and confirms the round-trip
  (parsed back, 3 nodes, re-serialize identical).
- **M128** — **per-object named signals** (`core::Signal`), toward Godot's `signal`/`connect`/`emit`: the
  `EventBus` (M64) is a *global by-type* bus; Godot signals are *per-object named channels* others connect
  to. `Signal<Args...>` is a typed channel an object owns; `connect`/`disconnect`/`emit` do the obvious,
  and it adds Godot's two staples — **one-shot** connections (fire once, auto-disconnect) and **deferred**
  connections (queued on emit, run later at `flushDeferred`). The new `signals` demo wires a
  Button→Player→died scenario, draws the connection graph, and logs a run showing immediate, one-shot, and
  deferred behavior.
- **M129** — **WAV audio load/save** (`audio::decodeWav` / `audio::encodeWav`), toward Godot's
  `AudioStreamWAV`: every prior Maz sound was procedurally synthesized — this reads (and writes) the actual
  bytes of a `.wav` file. `decodeWav` parses a RIFF/WAVE stream (8-bit unsigned + 16-bit signed PCM,
  mono/multi-channel) into float samples; `encodeWav` writes them back out. The new `wav` demo synthesizes
  a decaying tone, encodes it to `.wav` bytes, decodes them back, and draws the reconstructed waveform on
  an oscilloscope beside the parsed header.
- **M130** — a **3D reference grid + RGB gizmo axes builder** (`render::buildGrid` / `render::buildWireBox`),
  toward Godot's Node3D editor viewport: Maz could draw lit meshes and debug lines but had no builder for the
  spatial-reference primitives every 3D editor shows — a world-space ground grid and the X=red/Y=green/Z=blue
  origin gizmo. `buildGrid` emits the XZ-plane grid (center axis lines brighter) plus the RGB origin axes;
  `buildWireBox` emits the 12 edges of a placeable wireframe box. Pure geometry (colored line segments) drawn
  through the existing `drawLine` path — no shared shader change. The new `grid3d` demo draws the grid, gizmo,
  and a wire box under a fixed editor-style camera.
- **M131** — **2D physics-space queries** (`game::queryRay` / `querySegment` / `queryPoint`), toward
  Godot's `PhysicsDirectSpaceState2D`: Maz had rigid-body *dynamics* but no way to ask the collider set a
  spatial question — the primitive behind hitscan weapons, line-of-sight, ground probes, and mouse
  picking. `queryRay` returns the nearest hit (distance, point, normal, id) among circle + oriented-box
  shapes filtered by a collision mask; `queryPoint` reports which shapes contain a point. The new
  `rayquery` demo fans hitscan rays that pass through a "glass" layer and stop on "solid", with contact
  normals drawn and a point-picked shape highlighted.
- **M132** — **string interning / `StringId`** (`core::StringTable` + `core::fnv1a32`), toward Godot's
  `StringName`: a game refers to the same names (tags, signals, actions, tracks) constantly; interning
  stores each unique string once and hands back a small integer id, so comparison is an int compare and
  hashing is trivial. `intern` dedups (same text → same id), `find` looks up without inserting, `str`
  reverses an id to its text, `hash` exposes the FNV-1a hash. The new `strtable` demo interns a stream of
  repeated tags and shows the reference stream (name → id) beside the deduplicated pool (id → text → hash).
- **M133** — a **CSV parser + localization table** (`io::parseCsv` + `io::TranslationTable`), toward
  Godot's `Translation` / CSV import: a shippable game needs its text in more than one language.
  `parseCsv` is a robust RFC-4180 reader (quoted fields, embedded commas/newlines, `""` escapes,
  CRLF/LF); `TranslationTable` loads a Godot-style translation CSV (key + one column per locale) and
  answers `tr(key)` in the active locale with fallback (empty cell → source language, unknown key → the
  key itself). The new `locale` demo renders one game menu in four languages from a single CSV.
- **M134** — a **text layout engine** (`ui::layoutText` + `ui::TextLayout`), toward Godot's `Label`
  autowrap: the `Font` could draw/measure one line but couldn't fit a paragraph into a box. `layoutText`
  greedily word-wraps text to a max width (honoring explicit `\n` hard breaks) and aligns each line
  left/center/right, returning positioned lines the caller draws one-per-line. It's renderer-independent
  via an injected measure callback. The new `textwrap` demo wraps one paragraph three ways plus a
  hard-break quest log.
- **M135** — **additive/layered pose blending** (`anim::additiveBlend`), toward Godot's
  `AnimationNodeAdd2`: the existing blend cross-fades whole poses (idle↔walk); additive blending layers a
  *difference* (an additive clip relative to a reference pose) on top of any base at a weight, so a joint
  the additive clip doesn't move leaves the base untouched — the way you stack a "wave" or "aim" onto
  locomotion. The new `addblend` demo layers an elbow-bend onto a fixed arm pose at rising weights: the
  shoulder holds while only the elbow folds.
- **M136** — **procedural mesh primitives** (`render::shapes::makeCylinder` / `makeCone` / `makeTorus` /
  `makeCapsule`), toward Godot's CylinderMesh/CapsuleMesh/TorusMesh: Maz shipped only box/sphere/plane;
  these four add a barrel, a spike, a ring, and a rounded capsule as procedural solids with outward
  normals + UVs, dropping straight into the existing lit-mesh path (the original three are untouched). The
  new `primitives` demo renders all four as a lit gallery under a fixed camera.
- **M137** — a **generational-handle slot-map** (`core::SlotMap<T>`), toward Godot's `RID` / stable
  handles: an object pool that hands out `SlotHandle{index, generation}` ids, recycles freed slots, and
  **detects a stale handle** whose slot was reused (the dangling-handle / ABA bug) by bumping the slot's
  generation on free. `get` returns null for a stale handle. The new `slotmap` demo drives a real
  `SlotMap` through insert/free/reuse and shows which handles are live vs stale.
- **M138** — a **2D convex polygon collider** (`game::satOverlap` / `polyContains`), toward Godot's
  `ConvexPolygonShape2D`: Physics2D collided circles + boxes; this adds arbitrary convex shapes
  (triangle, pentagon, hull) via the Separating-Axis Theorem, returning overlap + the minimum-translation
  vector (push direction + depth) to separate them, plus point-in-polygon. The new `polycollide` demo
  tests a probe pentagon against a ring of shapes, drawing overlaps red with the MTV arrow and clear ones
  green.
- **M139** — a **sample-playback mixer** (`audio::SampleMixer`), toward Godot's `AudioStreamPlayer` over an
  `AudioStreamWAV`: Maz could decode a `.wav` and synthesize tones, but had no way to *play a decoded clip*.
  This is a self-contained offline stereo mixer — `play(clip, gain, pan, loop, speed)` starts a voice,
  reads are linearly interpolated (smooth pitch/resampling), voices are summed into one interleaved buffer,
  non-looping voices auto-stop at the end and looping ones wrap. The new `sampler` demo plays two decoded
  clips (a tone panned left, a noise blip panned right), mixes them, and draws the resulting L/R
  oscilloscopes.
- **M140** — a **binary resource-pack archive** (`io::packResources` / `io::ResourcePack`), toward Godot's
  `.pck` / `PackedData`: a game ships one archive, not a loose file tree. `packResources` bundles named blobs
  (level JSON, sounds, prefab text, raw data) into a single byte stream with a magic header and a
  `(path, offset, size)` directory; `ResourcePack::load` reads them back by path with full bounds checking so
  a corrupt/foreign archive fails cleanly. The new `respack` demo packs four resources, loads the archive
  back, and draws its directory table, a header hex dump, and a per-resource round-trip check.
- **M141** — a **BBCode rich-text parser** (`ui::parseBBCode` / `ui::stripBBCode`), toward Godot's
  `RichTextLabel`: plain text has one style, but BBCode mixes styles within a string — `[b]bold[/b]`,
  `[i]/[u]`, `[color=#ff0000]red[/color]`, `[size=32]big[/size]`. The parser turns markup into a flat list of
  styled runs (resolved bold/italic/underline/colour/size), lenient like Godot (nested tags stack, unclosed
  runs to the end, unknown tags pass through literally). The new `richtext` demo shows six BBCode strings each
  above its formatted result.
- **M142** — a **cubic Bézier path** (`math::Curve2D`), toward Godot's `Curve2D` / `Path2D`: an authored
  smooth curve through points with in/out control handles, plus **arc-length baking** so a follower travels
  it at constant speed no matter how it bends (`sampleBaked(distance)`). Also `sample`/`tangent`/`length`.
  The new `curve` demo draws the spline, its handles, the evenly-spaced baked points, and a traveller with
  its tangent arrow.
- **M143** — a **2D multi-mesh** (`render::MultiMesh2D`), toward Godot's `MultiMeshInstance2D`: one base
  convex shape plus a compact per-instance buffer (position, rotation, scale, colour) stamped many times.
  `transformedPolygon(i)` gives one instance's world polygon; `bakeTriangles()` flattens all instances into
  one triangle soup. The new `multimesh` demo stamps a single dart 540 times into a colour-swirled field.
- **M144** — **3D billboard modes** (`render::buildBillboard`), toward Godot's `SpriteBase3D` /
  `GeometryInstance3D` billboards: build the model matrix that turns a flat quad to face the camera —
  `Enabled` (full-facing, for smoke/impostors), `YBillboard` (yaws but stays upright, for trees/characters),
  or `Disabled`. The new `billboard` demo shows all three modes as rows of cards under an elevated camera.
- **M145** — **one-way platforms** (`game::resolveOneWayPlatform`), toward Godot's `one_way_collision`: a
  ledge that is solid only from above. A swept resolve lands a body that crosses the surface from above while
  descending, and lets a body launched from below (or already under it) pass straight through.
  `resolveOneWayPlatforms` picks the topmost ledge a faller lands on. The new `oneway` demo drops three balls
  onto ledges while a fourth is launched up through one — its trail crosses the bar as the others rest on top.
- **M146** — **chorus / flanger / phaser** (`audio::Chorus` / `Flanger` / `Phaser`), toward Godot's
  `AudioEffectChorus` / `AudioEffectPhaser`: the three LFO-swept "time-modulation" effects — detuned delayed
  copies that thicken a tone (chorus), a swept feedback comb (flanger), and a swept all-pass notch sweep
  (phaser) — all slotting onto a `Bus` like the other DSP effects. The new `modfx` demo runs one sustained
  note through each and draws the four waveforms side by side.
- **M147** — **root motion** (`anim::RootMotionTrack`), toward Godot's AnimationMixer root-motion track: a
  locomotion clip drives the character's travel — `advance()` reads the root's per-step displacement out of
  the clip and applies it in the character's facing direction (with a loop-seam-aware `delta`), so the feet
  don't slide. The new `rootmotion` demo walks a character along a swept arc with footprints planted on it.
- **M148** — **node groups** (`scene::GroupRegistry`), toward Godot's SceneTree groups: tag any node into
  named groups and ask the registry for "everything in group X" (`nodesInGroup`) or broadcast to them
  (`call`), instead of keeping per-system lists. The new `groups` demo tags a grid of nodes and drives two
  live group queries — ringing one set and stamping a warning on another.
- **M149** — **Rect2** (`math::Rect2`), toward Godot's `Rect2`: an axis-aligned rectangle with the full
  geometry set — `hasPoint`, `intersects`/`intersection` (clip), `merge` (union), `encloses`, `grow`,
  `expand`, `abs`. The new `rects` demo draws overlapping rectangles with their intersection, union bounds,
  grow halo, and point tests, all from `Rect2`.
- **M150** — **Range / ProgressBar** (`ui::Range` / `ui::ProgressBar`), toward Godot's `Range` + `ProgressBar`:
  the shared clamped/stepped value model (min/max/step/page → a 0..1 ratio) behind bars, sliders, and
  scrollbars, plus a progress bar exposing `fillFraction`/`percent`. The new `progress` demo shows six bars —
  plain fills, a ratio-tinted health bar, a custom-range mana bar, and a step-snapped bar.
- **M151** — **render interpolation** (`core::Interpolated<T>` + `core::interpolate`), toward Godot's physics
  interpolation: keep a body's previous + current fixed-step pose and blend them by the frame's
  `interpolationAlpha()` so motion stays smooth when the display rate doesn't divide the physics rate
  (rotation blends the shortest arc). The new `interp` demo ghosts the endpoint poses with the interpolated
  pose between them for four motions.
- **M152** — **area gravity fields** (`game::GravityArea2D` / `gravityAt`), toward Godot's Area2D gravity
  override: rectangular zones that change the gravity a body feels — directional (wind / updraft) or a point
  pull with inverse-square falloff — combined by priority with Replace/Add modes. The new `gravzones` demo
  drops balls through a wind field, an updraft, and an attractor, their trails bending accordingly.
- **M153** — **orthographic 3D camera** (`math::orthographic` / `orthographicSize`), toward Godot's `Camera3D`
  Orthogonal projection: a parallel projection with no perspective divide, so objects keep the same on-screen
  size at every depth and parallel edges never converge — the isometric look. The new `ortho3d` demo renders
  an iso field of lit cube columns.
- **M154** — **base64** (`io::base64Encode` / `base64Decode`), toward Godot's `Marshalls`: carry binary data
  through text — embed a blob inside JSON, a `.tres` resource, or a URL — RFC 4648 encode/decode with
  whitespace-tolerant, validating decode. The new `base64` demo shows text and byte buffers with their
  encodings and a round-trip check.
- **M155** — **float Curve** (`anim::Curve`), toward Godot's `Curve` resource: a keyframed `y = f(x)` value
  profile (Constant / Linear / Cubic-Hermite with per-point tangents, range-clamped) — the shape behind
  particle size/alpha over lifetime, audio fades, and custom easing. The new `floatcurve` demo plots a linear,
  an ease-in-out, an ease-out, and a multi-point particle-size curve.
- **M156** — **concave polygon fill** (`render::triangulatePolygon`), toward Godot's `Polygon2D`: ear-clipping
  triangulation tiles an arbitrary *simple* polygon — concave included — into triangles the convex-fill path
  can draw, where a triangle fan only works for convex shapes. The new `polyfill` demo fills a star, a block
  arrow, a plus/cross, and a thick C-ring, each with the triangle mesh + outline overlaid.
- **M157** — **analog-stick deadzone** (`input::analogVector` / `input::applyDeadzone`), toward Godot's
  `Input.get_vector`: a radial deadzone on the whole stick vector, magnitude rescaled so the deadzone edge is
  0 and full tilt is 1, and clamped to the unit circle so diagonals aren't faster — killing rest-drift and the
  faster-diagonal bug. The new `deadzone` demo shows a stick field + the 1-D response curve.
- **M158** — **audio stream randomizer** (`audio::StreamRandomizer`), toward Godot's `AudioStreamRandomizer`:
  a weighted clip pool with Random / Random-No-Repeat / Sequential pick modes plus per-trigger pitch and
  volume jitter, so repetitive one-shots stop sounding robotic. Seeded and deterministic. The new `randomizer`
  demo shows a pick histogram, a pitch×volume scatter, and a no-repeat tick strip.
- **M159** — **2D kinematic character controller** (`game::moveAndSlide`), toward Godot's
  `CharacterBody2D.move_and_slide`: sweeps a velocity-driven AABB against the static world so it never tunnels,
  slides the leftover motion along contacts, and reports on-floor / on-wall / on-ceiling — the movement core
  of every platformer. The new `kinematic` demo runs a character through an obstacle course, colouring its
  path by contact state.
- **M160** — **2D geometry helpers** (`math::Geometry2D`), toward Godot's `Geometry2D`: segment-vs-segment
  intersection, closest point on a segment, point-in-polygon (concave-safe), and segment-vs-circle — the
  workhorse queries behind AI line-of-sight, mouse picking, and trigger zones. The new `geometry` demo shows a
  segment web with intersections, a point-in-polygon grid, and closest-point projections.
- **M161** — **ItemList control** (`ui::ItemList`), toward Godot's `ItemList`: a scrollable box of selectable
  rows with single (radio) or multi selection, disabled rows, fixed-row geometry (`itemRect`/`itemAtPoint`/
  `ensureVisible`/`visibleRange`) and keyboard nav that skips disabled entries — the list behind file dialogs,
  inventories, and level-select menus. The new `itemlist` demo draws a scrolled single-select saved-games list
  with a scrollbar thumb and a multi-select loadout with checked rows.
- **M162** — **colour Gradient resource** (`anim::Gradient`), toward Godot's `Gradient`: sorted colour stops
  sampled over a 0..1 ramp with constant / linear / cubic (Catmull-Rom) interpolation, plus `bake(N)` for a
  GradientTexture — the colour ramp behind particle colour-over-lifetime, sky ramps, and health/heat tints.
  The new `gradient` demo shows a spectrum under all three modes plus fire / health / ocean ramps and a baked
  swatch strip.
- **M163** — **Camera3D projection** (`render::Camera3D`), toward Godot's `Camera3D`: project a 3D world point
  to screen pixels (unproject), cast a world-space pick ray from a screen pixel, and test points/spheres
  against the view frustum — the maths behind 3D mouse picking, floating world-space labels, and aim rays. The
  new `camera3d` demo projects a grid, axes, and wireframe cube to 2D, colours points by frustum containment,
  and unprojects a centre-screen ray onto the ground.
- **M164** — **fixed-capacity RingBuffer** (`core::RingBuffer<T>`), toward Godot's `RingBuffer`: a circular
  buffer that serves both a rolling window (overwrites the oldest once full) and a bounded FIFO queue — the
  container behind frame-time graphs, input/replay buffers, and moving averages. The new `ring` demo plots a
  96-slot frame-time history bar graph (with a budget line and rolling average) and an 8-slot input buffer.
- **M165** — **INI ConfigFile** (`io::ConfigFile`), toward Godot's `ConfigFile`: an INI-style `[section]` +
  `key=value` settings store with typed get/set, lenient parsing (comments, quotes, a global section) and a
  stable round-tripping `encode()` — the hand-editable format behind project settings and options files. The
  new `inifile` demo parses a settings.cfg into a table, edits it, and shows the re-encoded text.
- **M166** — **audio spectrum analyzer / FFT** (`audio::SpectrumAnalyzer`), toward Godot's
  `AudioEffectSpectrumAnalyzer`: a radix-2 FFT that turns audio samples into a frequency spectrum, with
  per-bin magnitudes and a `magnitudeForRange` band query — the maths behind rhythm games, VU/equalizer
  visualizers, and beat-reactive effects. The new `spectrum` demo plots a chord's spectrum as a frequency bar
  graph with bass/mid/treble band meters.
- **M167** — **2D affine Transform2D** (`math::Transform2D`), toward Godot's `Transform2D`: the 2×3 affine
  matrix behind every Node2D — place/rotate/scale/skew, convert points between local and world space
  (`xform`/`xformInv`), compose parent×child, and read back rotation/scale/skew. The new `xform2d` demo draws
  one arrow under a gallery of transforms (rotate / scale / skew / mirror) with basis-vector gizmos.
- **M168** — **PopupMenu control** (`ui::PopupMenu`), toward Godot's `PopupMenu`: the item list behind
  right-click context menus and dropdowns — checkbox/radio items, disabled rows, separators, submenu arrows,
  accelerator hints, hover navigation, and activation (toggle/switch/return id). The new `popupmenu` demo
  draws an open context menu with a hovered row, a checked item, a radio dot, and a submenu arrow.

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
