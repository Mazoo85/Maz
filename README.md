# Maz Engine

A native **C++20 + Vulkan + SDL3** game engine — 2D-first, architected so 3D drops in later
(pluggable renderer, camera abstraction, scene-friendly modules).

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
./build/bin/sandbox --headless      # CI: init, run, exit cleanly with no display/GPU
ctest --test-dir build              # headless smoke tests (all apps)
```

Layout: the engine library is `engine/`; sample apps are under `apps/` (`sandbox`, `orbs`,
`swarm`).

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

## Controls

- **Sandbox** (top-down demo): WASD / arrows to move · Esc to quit
- **ORB RUN**: Space to start / restart · WASD / arrows to move · Esc to quit
