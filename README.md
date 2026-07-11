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
ctest --test-dir build              # headless smoke tests (all apps)
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
- **World** / **Village** (explore): WASD move · mouse look · Esc to quit
