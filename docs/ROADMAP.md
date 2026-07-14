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
- [~] CI matrix (Linux/Windows/macOS) running the headless smoke test — **Linux CI landed**
      (`.github/workflows/ci.yml`: installs the Vulkan SDK + glslang, builds under `-Werror`,
      runs ctest headless). Windows/macOS still to add.
- [~] Config system (CVars / ini / json), persisted settings — **`maz::core::CVarRegistry` landed**
      (typed bool/int/float/string console variables with defaults + descriptions; type-checked get/set
      where a wrong-type/missing name yields a fallback/false and never throws; `setFromString` parsing
      for console/config text; `reset` to default; change callbacks firing only on an actual value
      change; `StringId`-keyed; header-only, unit-tested); ini/json file load/save + persistence still TODO
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
- [x] Action-mapping layer (bind abstract actions like "Jump" to keys/buttons/axes) — **`maz::input::ActionMap` landed**
      (device-neutral `InputId`; OR-combined digital buttons + digital/analog axis contributions summed then
      clamped to [-1,1]; press/release edge detection; templated `update(Sampler)` so it builds/tests under
      MAZ_CORE_ONLY, decoupled from `maz::platform::Input`; header-only, unit-tested); action contexts/layers,
      analog deadzone/response curves, and binding serialization still TODO
- [ ] Text input / IME, clipboard, drag-and-drop
- [ ] vsync toggle, frame pacing, present-mode selection
- [ ] Filesystem abstraction, virtual paths, save-directory resolution
- [ ] Thread pool + job/task system, lock-free work queues

## Phase 2 — Core utilities
- [x] Math via GLM (vectors, matrices, quaternions) re-exported under `maz::math`
- [~] Transform helpers, AABB/OBB, ray, plane, frustum — **Aabb + Ray + Plane + intersections + Frustum culling + affine Transform landed** (`maz::math::Geometry`/`Frustum`/`Transform`, unit-tested); **`maz::math` quaternion rotation helpers landed** (`Rotation.hpp`: `fromAxisAngle`/`fromEuler` (documented YXZ, Godot-default)/`rotate`/`angleBetween` (unsigned, double-cover-safe)/`slerp` (shortest-path)/`rotateTowards` (angle-clamped step)/`lookRotation` (local -Z→forward, camera convention), header-only, unit-tested); OBB TODO
- [x] Easing / interpolation, deterministic RNG (PCG/xoshiro) — PCG32 `maz::core::Rng` + `maz::math` interpolation/easing (lerp/inverseLerp/remap/smoothstep/moveToward/lerpAngle + Penner `Easing`), unit-tested; **`maz::core` sampling utilities landed** (`Sampling.hpp`: `shuffle` in-place Fisher-Yates, `weightedIndex` loot-table draw proportional to non-negative weights, `sampleWithoutReplacement` k distinct indices via partial Fisher-Yates, `gaussian` Box-Muller normal sample — all built on `Rng`, deterministic per seed, header-only, unit-tested)
- [~] Memory: linear / stack / pool / frame allocators, arenas — **`maz::core::LinearAllocator` landed**
      (fixed-capacity bump-pointer arena: aligned `allocate`, `reset`, stack-style `marker`/`rewindTo`;
      header-only, unit-tested); dedicated stack / pool / frame allocators still TODO
- [~] Handles / generational indices, object pools — **`maz::core::Handle` + `Pool<T>` landed**
      (generational stale-reference detection à la Godot's RID owner; header-only, unit-tested);
      linear/stack/frame arena allocators (above) still TODO
- [x] Containers: `small_vector`, sparse set, ring buffer — **all three landed**
      (`maz::core::RingBuffer<T>` fixed-capacity FIFO à la Godot's `RingBuffer`; `maz::core::SparseSet<T>`
      dense/sparse key→value map with O(1) insert/remove/contains and packed iteration — the ECS
      component-storage structure; `maz::core::SmallVector<T,N>` inline-storage vector à la Godot's
      `LocalVector` / llvm `SmallVector`, spilling to the heap only past N; all header-only, unit-tested)
      — plus **`maz::core::LruCache<K,V>` landed** (bounded least-recently-used cache à la Godot's
      resource cache; MRU-ordered intrusive list + key→node map, O(1) put/get/peek/erase with
      evict-after-insert LRU eviction over capacity; header-only, unit-tested)
- [~] String interning / `StringId` (hashed), fixed strings — **`maz::core::StringId` landed** (constexpr FNV-1a 64, `_sid` UDL, unit-tested) + **`maz::core::str` string utilities landed** (ASCII/locale-independent split/join, borrowing trim, startsWith/endsWith/contains, toLower/toUpper/equalsIgnoreCase, replaceAll, from_chars parseInt/parseFloat; header-only, unit-tested); interning/original-string storage + fixed strings TODO
- [x] Event bus / signals, delegates / typed callbacks — **`maz::core::EventBus<Event>` + `Delegate<R(Args...)>` landed**
      (`EventBus`: Godot-signal-style named channels keyed by `StringId`, `std::function` callbacks,
      generational `Connection` tokens for safe disconnect, re-entrancy-safe emit — the multicast side;
      `Delegate`: a single-target, ZERO-HEAP fast delegate à la Godot's `Callable`, binding free/member
      functions via C++20 `auto` NTTP into two raw pointers; both header-only, unit-tested)
- [~] Minimal reflection (type ids, property registration) for serialization + editor — **compile-time
      type ids landed** (`maz::core::type_id<T>()`/`type_hash<T>()`/`type_name<T>()`/`type_info<T>()`:
      run-stable `StringId` type ids from the compiler-spelled name via `fnv1a64`, plus `TypeInfo`
      {id,name,size,alignment}; header-only, unit-tested); property/field registration still TODO
- [~] Serialization (binary + JSON), versioned schemas — **binary `ByteWriter`/`ByteReader` landed**
      (`maz::core` endian-safe little-endian POD/string/StringId round-trip with a bounds- and
      overflow-safe fail-safe reader; header-only, unit-tested + ASAN/UBSan-fuzzed); plus
      `maz::core` base64 (RFC 4648 standard alphabet, '=' padding) + hex binary-to-text codecs
      (the Godot Marshalls analog, for embedding binary blobs in text config/scene files;
      encode->string, decode replaces out + fail-safe on malformed input; header-only,
      unit-tested); JSON + versioned schemas still TODO
- [~] Profiling: scoped timers, frame markers, Tracy integration — **`maz::core::Profiler` +
      `ScopedTimer` landed** (StringId-named scopes aggregating count/total/min/max ns, RAII
      steady_clock timer; header-only, unit-tested); frame markers + Tracy still TODO
- [x] Type-safe bit flags — **`maz::core::Flags<E>` landed** (zero-overhead constexpr wrapper over a
      scoped enum with unsigned underlying type; `| & ^ ~`/compound ops, `has`/`hasAny`/`hasAll`/
      `set`/`clear`/`toggle`, `MAZ_FLAGS_ENABLE` opt-in free operators; the typed answer to raw
      int/uint32 bitmasks; header-only, unit-tested incl. a uint8-underlying case)
- [x] Fixed-size bitset — **`maz::core::Bitset<N>` landed** (word-backed uint64 array; set/clear/flip/
      test, all/any/none/count, `& | ^ ~` + `contains` subset test for ECS signature matching, and
      fast set-bit iteration `findFirstSet`/`findNextSet`/`forEachSetBit` via C++20 `<bit>` — the
      value-add over `std::bitset`; unused tail bits held at zero; header-only, unit-tested +
      UBSan/ASan-checked across N boundaries)
- [ ] Unit-test framework wiring (doctest/Catch2)
- [x] Non-GPU `maz_core` lib + `MAZ_CORE_ONLY` build so core is unit-testable without the Vulkan SDK

## Phase 3 — Rendering (Vulkan)
- [x] Instance + validation layers (debug), debug messenger
- [x] Surface, physical-device selection, logical device + queues
- [x] Swapchain, image views, render pass, framebuffers
- [x] Command pool/buffers, sync (semaphores/fences), frames-in-flight
- [x] `beginFrame` / clear / `endFrame` present loop (clear color)
- [x] Swapchain recreation on resize / out-of-date
- [x] Graceful degrade when no GPU/ICD present (headless safe)
- [ ] VMA (Vulkan Memory Allocator), buffer/image helpers, staging uploads
- [~] Graphics pipeline + descriptor-set management, push constants, dynamic state — **pipeline +
      push constants + dynamic viewport/scissor landed** (MeshRenderer). Descriptor sets still to do.
- [~] Shader module loading from SPIR-V + reflection + hot reload — **SPIR-V load landed** (mesh
      shaders loaded from beside the exe). Reflection + hot reload still to do.
- [ ] **2D:** sprite batch renderer, texture atlas, `Camera2D`, line/shape debug draw
- [ ] **2D:** tilemap renderer (chunked), sprite sorting / layers
- [ ] Text rendering (bitmap + SDF fonts, glyph atlas, layout)
- [~] Mesh renderer (indexed draw), vertex layouts, instancing — **indexed draw + vertex layout
      landed** (`MeshRenderer` draws a `maz::assets::Model` with per-mesh vertex/index buffers).
      Instancing + staging-buffer uploads (currently host-visible) still to do.
- [~] **3D:** `Camera3D`, perspective/ortho, depth buffer, back-face cull — **perspective camera +
      depth buffer landed** (`maz::scene::Camera`; swapchain has a D32 depth attachment, pipeline
      depth-tests). Ortho + back-face cull (currently cull-none) still to do.
- [ ] Materials + PBR groundwork, texture sampling / mipmaps
- [ ] Lighting: directional / point / spot; forward+ or deferred path
- [ ] Shadow maps, skybox / image-based lighting
- [ ] Post-processing stack (tonemap, bloom, FXAA/TAA), HDR
- [ ] Render-to-texture, multiple viewports, MSAA
- [ ] GPU profiling, keep validation-clean baseline

## Phase 4 — Scene & ECS
- [~] Entity Component System (sparse-set or archetype), entity handles — **`maz::ecs::World` landed**
      (generational `Entity` handles with recycle + stale-detection; type-erased per-component `SparseSet`
      storage; per-entity `Bitset<64>` signatures; `add`/`get`/`has`/`remove`, `destroy` that clears an
      entity from all stores, single-component `each<T>` iteration, AND multi-component `view<Ts...>`
      queries — `signature.contains(mask)` filtering over the lead store, `fn(Entity, Ts&...)` by
      reference; header-only, unit-tested + ASAN/UBSan-clean). System scheduler still TODO
- [x] Core components: `Transform`, `Hierarchy/Parent`, `Name`, `Tag` — **`maz::ecs::Components` landed**
      (built-in component structs `LocalTransform`/`WorldTransform` wrapping iter5 `maz::math::Transform`,
      `Name` + `Tag` as hashed `StringId`, and a `Parent` entity link; free-function helpers
      `findByName`/`setParent` (upsert, no dup on reparent)/`parentOf`/`childrenOf` over `World`;
      header-only, unit-tested + ASAN/UBSan-clean). The transform-propagation SYSTEM
      (`LocalTransform`+`Parent`→`WorldTransform`) LANDED as **`maz::ecs::propagateTransforms`**
      (recursion up the parent chain with per-pass memoization so parents resolve before children in
      ANY dense-iteration order; `world = parentWorld × local` compose order; a null/dead/transform-less
      parent is treated as a root; idempotent; recursion-stack cycle guard so a constructed Parent cycle
      terminates; binds directly as a `SystemScheduler` `SystemFn`; header-only, unit-tested). A
      name→entity index is still a future refinement
- [~] Scene graph, world-transform propagation, dirty flags — **`maz::scene::SceneGraph` first slice landed**
      (node hierarchy with local/cached-world `maz::math::Transform` per node; lazy `getWorld` =
      parentWorld × local recompute; any `setLocal`/`setParent` marks the subtree dirty; `setParent`
      reparent with an ancestor cycle-guard; header-only, unit-tested + ASAN/UBSan-clean incl. a
      rotation-compose-order gate). Node destroy/recycle + ECS integration still TODO
- [~] System scheduler (ordered + parallel execution) — **`maz::ecs::SystemScheduler` landed**
      (register named systems — `std::function<void(World&)>`, so a lambda / free fn / member fn /
      `Delegate` all bind in — run every enabled system in registration order each tick; add/remove/
      setEnabled/isEnabled/has/size/clear keyed by `StringId`; header-only, unit-tested incl. a
      flagship movement-system end-to-end over `view<Position,Velocity>` + ASAN/UBSan-clean).
      Parallel/staged execution + before/after dependencies still TODO
- [~] Scene serialization (save/load), prefabs / blueprints — **`maz::scene::SceneSerializer` first slice landed**
      (registration-based, component-agnostic ECS scene (de)serializer over `World`: component TYPES opt in via
      `registerComponent<T>(tag, writeFn, readFn)`; `save` walks the union of registered-type `each<T>`, assigns
      deterministic ordinals (index-sorted, deduped) and emits per-entity tag + length-prefixed bodies; `load`
      re-creates entities and replays bodies. Entity-reference fields round-trip through `EntityRemap` ordinals so
      a `Parent` survives index/generation reassignment; unknown tags are SKIPPED via the length prefix
      (forward-compat); load is FAIL-SAFE (bad magic/version, bogus count, over-long body → returns false, no
      throw/OOB, huge-`n` alloc guard); composes iter16 `ByteWriter`/`ByteReader`; header-only, unit-tested +
      ASAN/UBSan-clean). Prefabs/blueprints, versioned migration, and a JSON/text format still TODO
- [~] Spatial partitioning (grid / quadtree / octree / BVH) for culling + queries —
      **`maz::spatial::SpatialHashGrid` landed** (sparse uniform hash grid: items are
      (id, `maz::math::Aabb`) rasterized into every integer cell they overlap, keyed
      by cell coordinate in a hash map so negative/unbounded coords need no preallocated
      volume; `std::floor` cell math for correct negative-coordinate binning; broad-phase
      cell candidate gather with multi-cell dedup + narrow-phase Aabb overlap;
      insert/remove/update/queryRegion/queryPoint/clear with empty-bucket pruning;
      header-only, unit-tested). **`maz::spatial::Octree<T>` landed** (adaptive
      hierarchical point partition: leaves subdivide into 8 octants past maxPerNode,
      bounded by maxDepth; AABB/sphere queries prune to overlapping octants then
      narrow-test each point — complements the uniform hash grid). Quadtree/BVH +
      World/SceneGraph culling integration still TODO

## Phase 5 — Asset pipeline
- [ ] Asset manager: async load, ref counting, GUIDs, hot reload
- [ ] Image loading (stb_image), compressed textures (KTX2), mipmaps
- [~] Model import (glTF via cgltf/tinygltf; optional assimp) — **Blender→glTF pipeline landed**:
      `maz::assets::loadModel` (cgltf) loads meshes/normals/UVs + bounds; Blender export helper in
      `tools/blender/`. See [`BLENDER_PIPELINE.md`](BLENDER_PIPELINE.md). Still to do: GPU upload +
      draw (needs the Phase 3 mesh renderer), materials/textures, async loading via the asset manager.
- [ ] Audio asset loading (wav / ogg), font import, shader assets
- [ ] Asset cooking / packing pipeline, pak archives, streaming
- [ ] Import settings + dependency graph + reimport

## Phase 6 — Physics & collision
- [ ] 2D: AABB / circle, broadphase (grid / sweep-and-prune), resolution
- [ ] 2D physics integration (custom or Box2D)
- [~] 3D collision shapes, raycasts / queries, triggers / overlaps — **Sphere primitive + sphere/ray-sphere + ray-triangle intersection tests landed** (`maz::math::Collision`: `Sphere` + `sphereSphere`/`sphereAabb`/`spherePlane`/`raySphere`/`closestPointOnAabb`, plus `rayTriangle` (Möller–Trumbore, barycentrics + optional backface cull) for mesh picking / raycasting, complementing iter2 Geometry Aabb/Ray/Plane; unit-tested); full collision shapes/queries/triggers TODO
- [ ] 3D physics integration (Jolt or Bullet), character controller
- [ ] Continuous collision, layers / masks, physics materials
- [ ] Collider / contact debug visualization

## Phase 7 — Audio
- [ ] Audio device + mixer (SDL audio / miniaudio / OpenAL)
- [ ] Sound instances, buses / categories, volume / pitch / loop
- [ ] Streaming music vs one-shot SFX
- [ ] 2D panning + 3D spatialization, attenuation, doppler
- [ ] DSP effects (reverb, filter), ducking

## Phase 8 — Animation
- [ ] Sprite / flipbook animation, frame events
- [ ] Skeletal animation (glTF skins), GPU skinning, blend trees
- [ ] Animation state machine, transitions, IK (later)
- [~] Tween / timeline system, curves — **`maz::anim::Tween` landed**: single-value float eased animation
      (from->to over duration, advanced by `update(dt)`) with Once/Loop/PingPong loop modes + `onComplete`
      (fires once on the Once-mode finish transition), Loop/PingPong driven by fmod phase math so any dt
      (incl. multi-cycle) is handled and a zero duration is div-by-zero-guarded; composes iter6
      `maz::math::Easing`/`ease`/`lerp`; header-only, unit-tested. `maz::math::CatmullRomSpline` landed:
      uniform Catmull-Rom through control points (evaluate/tangent/sample), Godot Curve3D/Path3D analog
      for camera/motion paths. A generic `Tween<T>` (vec3/color),
      sequences/timelines, start delay, speed scale, and per-frame-event tracks still TODO

## Phase 9 — UI
- [ ] Dear ImGui integration for tools / debug overlays
- [ ] Retained/immediate game-UI: widgets, layout, anchoring, scaling
- [ ] Text input, focus / navigation, controller UI nav
- [ ] Nine-slice, fonts, localization-aware text

## Phase 10 — Scripting & gameplay framework
- [ ] Scripting VM (Lua via sol2, or C# hosting) + engine bindings
- [ ] Script hot-reload, sandboxing
- [x] Gameplay: state machines, behavior trees, AI steering — **`maz::ai::StateMachine` landed**: event-driven FSM with onEnter/onUpdate(dt)/onExit callbacks + StringId states/events + start/fire/update/reset (transition fires onExit old -> onEnter new; self-transition fires both; unmatched event no-ops); header-only, unit-tested. **`maz::ai::BehaviorTree` landed**: Sequence/Selector composites with Running-resume (a Running child suspends the composite and resumes there next tick) + Inverter/Succeeder/Repeater decorators + Action leaves returning Success/Failure/Running, reset() restarts from the top; header-only, unit-tested. **`maz::ai::Steering` landed**: classic Reynolds seek/flee/arrive/pursue/evade returning a maxForce-truncated steering force (arrive ramps speed down inside slowRadius; pursue/evade seek/flee the linearly-predicted future position; safeNormalize/truncate guard zero-length vectors against NaN); pure functions over glm vec3, header-only, unit-tested. **`maz::ai::Blackboard` landed**: typed StringId-keyed shared-memory store (std::any-backed) that ties the AI systems together — type-checked `set`/`get<T>`/`has<T>` plus always-safe `getOr<T>(key, fallback)` (returns the fallback on an absent or wrong-typed key), last-write-wins overwrite incl. changing a key's stored type, erase/clear/size; header-only, unit-tested
- [~] Pathfinding (A* / nav grid; navmesh later) — **`maz::ai` A* grid pathfinding landed** (`GridMap` + `findPath` with integer 10/14 costs, admissible Manhattan/octile heuristic, corner-cutting-forbidden diagonals, deterministic (f,h,index) tie-break, cost-optimal reconstruction; header-only, unit-tested). **`maz::ai::AStarGraph` landed** (general weighted-graph shortest-path finder / Godot AStar3D analog complementing the grid pathfinder: nodes with optional 3D positions + non-negative weighted directed/bidirectional edges, `findPath(start, goal, heuristicScale)` where a Euclidean-distance heuristic scaled by heuristicScale degenerates to pure Dijkstra at 0 and is admissible A* for scale>0 when weights >= endpoint distance, deterministic (f,h,id) tie-break, cost-optimal reconstruction + `lastCost`; header-only, unit-tested); navmesh, weighted terrain, JPS, dynamic edge removal / point-disable still TODO
- [~] Timer / callback scheduler advanced by `update(dt)` — **`maz::core::TimerManager` landed**: the
      Godot SceneTreeTimer analog — `after(delay, cb)` fires a one-shot `std::function` callback once,
      `every(interval, cb)` fires repeatedly with catch-up (a single large dt spanning N whole intervals
      fires N times, carrying the remainder), `cancel(id)` stops a still-pending timer; safe under
      re-entrancy — a callback may cancel any timer (incl. itself) or schedule new timers (deferred to the
      next tick), timers fire in registration order within a tick; header-only, unit-tested + ASAN/UBSan-clean.
      Pause / time-scale and a SceneTree binding still TODO
- [ ] Particle system (CPU + GPU), emitters, affectors
- [~] Tilemap tools, procedural generation utilities — **`maz::math::PerlinNoise` landed** (Ken Perlin's improved gradient noise 2D/3D + fBm, seeded via `maz::core::Rng` so a given seed reproduces the same field across runs/platforms; output ~[-1,1], exactly 0 at integer lattice coords, noise2D is the z=0 slice of noise3D; the Godot FastNoiseLite analog; header-only, unit-tested). **`maz::scene::TileMap` landed** (2D multi-layer grid of `TileId` tiles, 0 == empty; the Godot TileMap analog for 2D level data, pairs with PerlinNoise by thresholding noise into tiles; row-major within a layer over stacked layers, `set`/`at`/`fill`/`fillRegion`/`clear` plus `countNonEmpty`/`empty`, `at` reads out-of-bounds x/y as empty for safe neighbor sampling while `fillRegion` clamps a partly-off-map rectangle; header-only, unit-tested); autotiling, tile atlases/metadata, infinite/chunked maps, simplex/cellular/domain-warp noise, and more procgen utilities still TODO
- [ ] Save/load game state, checkpoints
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
- [ ] **Port ZOMBOID: ANCHORAGE** natively onto Maz Engine — tilemap, entities, needs/stats,
      loot, hordes, audio — the flagship proof the engine ships a full game (the existing
      `index.html` / `js/` browser version is the design reference)

---

### How to pick the next task
1. Finish **Phase 3** rendering (VMA → pipeline → sprite batch) — it unblocks everything visual.
2. In parallel, stand up **Phase 4 ECS** — it unblocks scenes and gameplay.
3. Then **Phase 5 assets** so you can load real textures/models.
4. Everything after is genre-driven; the Zomboid port (Phase 13) is the forcing function that
   keeps the API honest.
