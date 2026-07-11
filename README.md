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
- **World** / **Village** (explore): WASD move · mouse look · Esc to quit
