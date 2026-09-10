# Maz Engine — Roadmap ("the massive list")

A native **C++20 + Vulkan + SDL3** game engine, built **2D-first but architected so 3D drops
in later** (pluggable renderer, camera abstraction, scene graph).

This document is the master to-do list. It's organized into phases; **each bullet is a
buildable task**. Phases are ordered roughly by dependency, but many items inside a phase can be
done in parallel. Status legend: `[ ]` todo · `[~]` in progress · `[x]` done.

> **Current milestone: M0 — walking skeleton.** Window opens, fixed-timestep loop runs, Vulkan
> clears the screen, clean shutdown. Everything marked `[x]` below is done in the initial
> scaffold; everything else is the road ahead.

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
- [ ] Config system (CVars / ini / json), persisted settings
- [ ] Crash handler / stack-trace dump, structured log sinks (file, console)
- [ ] Semantic-version header, `CHANGELOG.md`

## Phase 1 — Platform layer
- [x] Window creation, resize, close (SDL3)
- [x] Event pump; quit/resize handling
- [x] Keyboard + mouse state, just-pressed / just-released edge detection
- [x] Fixed-timestep clock (accumulator) + frame delta
- [ ] Fullscreen / borderless, multi-monitor, DPI / content scaling
- [ ] Focus / minimize / occlusion handling (pause when unfocused)
- [ ] Gamepad / controller support + haptics (rumble)
- [ ] Action-mapping layer (bind abstract actions like "Jump" to keys/buttons/axes)
- [ ] Text input / IME, clipboard, drag-and-drop
- [ ] vsync toggle, frame pacing, present-mode selection
- [ ] Filesystem abstraction, virtual paths, save-directory resolution
- [ ] Thread pool + job/task system, lock-free work queues

## Phase 2 — Core utilities
- [x] Math via GLM (vectors, matrices, quaternions) re-exported under `maz::math`
- [ ] Transform helpers, AABB/OBB, ray, plane, frustum
- [~] Easing / interpolation, deterministic RNG (PCG/xoshiro) — PCG32 (`zb::Rng`) shipped with the
      Zomboid sim; easing/interp still todo
- [ ] Memory: linear / stack / pool / frame allocators, arenas
- [ ] Handles / generational indices, object pools
- [ ] Containers: `small_vector`, sparse set, ring buffer
- [ ] String interning / `StringId` (hashed), fixed strings
- [ ] Event bus / signals, delegates / typed callbacks
- [ ] Minimal reflection (type ids, property registration) for serialization + editor
- [ ] Serialization (binary + JSON), versioned schemas
- [ ] Profiling: scoped timers, frame markers, Tracy integration
- [ ] Unit-test framework wiring (doctest/Catch2)

## Phase 3 — Rendering (Vulkan)
- [x] Instance + validation layers (debug), debug messenger
- [x] Surface, physical-device selection, logical device + queues
- [x] Swapchain, image views, render pass, framebuffers
- [x] Command pool/buffers, sync (semaphores/fences), frames-in-flight
- [x] `beginFrame` / clear / `endFrame` present loop (clear color)
- [x] Swapchain recreation on resize / out-of-date
- [x] Graceful degrade when no GPU/ICD present (headless safe)
- [ ] VMA (Vulkan Memory Allocator), buffer/image helpers, staging uploads
- [ ] Graphics pipeline + descriptor-set management, push constants, dynamic state
- [ ] Shader module loading from SPIR-V + reflection + hot reload
- [ ] **2D:** sprite batch renderer, texture atlas, `Camera2D`, line/shape debug draw
- [ ] **2D:** tilemap renderer (chunked), sprite sorting / layers
- [ ] Text rendering (bitmap + SDF fonts, glyph atlas, layout)
- [ ] Mesh renderer (indexed draw), vertex layouts, instancing
- [ ] **3D:** `Camera3D`, perspective/ortho, depth buffer, back-face cull
- [ ] Materials + PBR groundwork, texture sampling / mipmaps
- [ ] Lighting: directional / point / spot; forward+ or deferred path
- [ ] Shadow maps, skybox / image-based lighting
- [ ] Post-processing stack (tonemap, bloom, FXAA/TAA), HDR
- [ ] Render-to-texture, multiple viewports, MSAA
- [ ] GPU profiling, keep validation-clean baseline

## Phase 4 — Scene & ECS
- [ ] Entity Component System (sparse-set or archetype), entity handles
- [ ] Core components: `Transform`, `Hierarchy/Parent`, `Name`, `Tag`
- [ ] Scene graph, world-transform propagation, dirty flags
- [ ] System scheduler (ordered + parallel execution)
- [ ] Scene serialization (save/load), prefabs / blueprints
- [ ] Spatial partitioning (grid / quadtree / octree / BVH) for culling + queries

## Phase 5 — Asset pipeline
- [ ] Asset manager: async load, ref counting, GUIDs, hot reload
- [ ] Image loading (stb_image), compressed textures (KTX2), mipmaps
- [ ] Model import (glTF via cgltf/tinygltf; optional assimp)
- [ ] Audio asset loading (wav / ogg), font import, shader assets
- [ ] Asset cooking / packing pipeline, pak archives, streaming
- [ ] Import settings + dependency graph + reimport

## Phase 6 — Physics & collision
- [ ] 2D: AABB / circle, broadphase (grid / sweep-and-prune), resolution
- [ ] 2D physics integration (custom or Box2D)
- [ ] 3D collision shapes, raycasts / queries, triggers / overlaps
- [ ] 3D physics integration (Jolt or Bullet), character controller
- [ ] Continuous collision, layers / masks, physics materials
- [ ] Collider / contact debug visualization

## Phase 7 — Audio
- [ ] Audio device + mixer (SDL audio / miniaudio / OpenAL)
- [~] Sound instances, buses / categories, volume / pitch / loop — a headless chiptune synth
      (`zb::audio`) renders the Zomboid SFX + music to PCM/WAV deterministically; a real-time device
      to stream them is still todo
- [ ] Streaming music vs one-shot SFX
- [ ] 2D panning + 3D spatialization, attenuation, doppler
- [ ] DSP effects (reverb, filter), ducking

## Phase 8 — Animation
- [ ] Sprite / flipbook animation, frame events
- [ ] Skeletal animation (glTF skins), GPU skinning, blend trees
- [ ] Animation state machine, transitions, IK (later)
- [ ] Tween / timeline system, curves

## Phase 9 — UI
- [ ] Dear ImGui integration for tools / debug overlays
- [ ] Retained/immediate game-UI: widgets, layout, anchoring, scaling
- [ ] Text input, focus / navigation, controller UI nav
- [ ] Nine-slice, fonts, localization-aware text

## Phase 10 — Scripting & gameplay framework
- [ ] Scripting VM (Lua via sol2, or C# hosting) + engine bindings
- [ ] Script hot-reload, sandboxing
- [ ] Gameplay: state machines, behavior trees, AI steering
- [ ] Pathfinding (A* / nav grid; navmesh later)
- [ ] Particle system (CPU + GPU), emitters, affectors
- [ ] Tilemap tools, procedural generation utilities
- [~] Save/load game state, checkpoints — versioned binary save/load shipped for the Zomboid sim
      (`Sim::saveState`/`loadState`, bit-exact, continuation-deterministic); a generic engine
      serializer is still todo
- [ ] Localization + string tables, deterministic time / RNG

## Phase 11 — Editor & tooling
- [ ] Standalone editor app (engine + ImGui docking)
- [ ] Viewport with gizmos (translate/rotate/scale), grid, snapping
- [ ] Scene hierarchy panel, reflection-driven entity inspector, asset browser
- [ ] Play-in-editor, undo/redo (command stack), multi-select
- [ ] Content-pipeline UI, build / package button
- [ ] Profiler + log panels

## Phase 12 — Cross-cutting quality
- [ ] Unit + golden-image render tests, CI gates
- [ ] Deterministic fixed-step simulation, replay
- [ ] Performance budgets + profiling dashboards
- [ ] Docs site, API docs (Doxygen), tutorials / samples
- [ ] Packaging / installers per platform, asset signing
- [ ] Opt-in telemetry / crash reporting

## Phase 13 — Demos & the first real game
- [x] `sandbox`: window + animated clear color (proves the loop + renderer)
- [ ] `sandbox`: textured sprite + rotating cube (proves 2D + 3D)
- [ ] Sample scenes: pong, platformer, top-down shooter
- [~] **Port ZOMBOID: ANCHORAGE** natively onto Maz Engine — the flagship proof the engine ships a
      full game (the existing `index.html` / `js/` browser version is the design reference).
      **Done:** the deterministic simulation core is ported to C++20 as the `zomboid` library
      (`game/`) — Anchorage tile worldgen, item DB + themed loot tables, player needs/stats decay,
      melee + ranged combat, zombie AI + hordes, day/night + wave spawning, seeded-reproducible via
      `zb::Rng`. Runs headless (`apps/zomboid`) and is unit-tested in CI (`tests/zomboid_tests`),
      no GPU required. A dependency-free **software reference rasterizer** (`zomboid/render/`) draws
      the neon scene + a world-map overview to a CPU framebuffer and out to PNG (`--render`), so the
      port is visually verifiable headless (and seeds Phase 12 golden-image tests). **Next:** bridge
      the same draw intent to the Vulkan sprite/tilemap renderer + SDL input + audio so it's playable
      in a window. Versioned **save/load** (`Sim::saveState`/`loadState`) is also done — bit-exact and
      continuation-deterministic (a loaded game continues identically to one never saved). The port
      is already **playable in a terminal** (`apps/zomboid-tui`) — keyboard input into the sim, each
      frame drawn as truecolor half-block ANSI + a text HUD, no GPU. The chiptune **audio** is ported
      too (`zb::audio` renders all SFX + music to WAV headless; `apps/zomboid-audio`). See
      `docs/ZOMBOID_PORT.md`.

---

### How to pick the next task
1. Finish **Phase 3** rendering (VMA → pipeline → sprite batch) — it unblocks everything visual.
2. In parallel, stand up **Phase 4 ECS** — it unblocks scenes and gameplay.
3. Then **Phase 5 assets** so you can load real textures/models.
4. Everything after is genre-driven; the Zomboid port (Phase 13) is the forcing function that
   keeps the API honest.
