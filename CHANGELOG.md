# Changelog

All notable changes to the Maz Engine are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); the engine version is defined in
`engine/include/maz/core/Version.hpp` (`maz::engineVersion()`) and mirrored in the top-level
`CMakeLists.txt`.

## [Unreleased]

### Flagship game
- **ZOMBOID** (`apps/zomboid`) — a top-down twin-stick zombie **shooter** whose entire simulation is
  written in `maz::script` and driven on a `scene::SceneTree`. Mouse-aim + hold-to-fire pulls rounds
  from an object-pooled bullet system (a script can't spawn nodes, so bullets and zombies are fixed
  pools the scripts activate/recycle); zombies have health and die; a wave Director spawns endless,
  escalating hordes on a ring; loot, hunger→health survival pressure, and the day/night rage ramp
  remain. Score / wave / kills HUD, a hurt-tint on wounded zombies, an aim tracer and crosshair.
  100% of the rules are verified headless in CI (`testZomboidSim`: pools build, wave 1 auto-spawns,
  firing respects the ~6/s cadence, a bullet kills a zombie and scores, cleared waves escalate); the
  render app steps a deterministic fixed timestep under `--headless`/`--frames` (a 30 s autopilot run
  reaches wave 4 / 36 kills with no GPU).

### Scripting & scene
- `maz::script` — a from-scratch, header-only scripting language (the GDScript competitor): values,
  collections, stdlib+RNG, closures, classes + inheritance + `super`, host binding + lifecycle,
  signals + deferred dispatch, safety budgets + stack traces, hot reload, gradual typing, modules +
  introspection + debugger hooks.
- `scene::SceneTree` / `SceneNode` — unified node hierarchy with 2D transform composition, attached
  scripts, groups, path lookup, and `get_node`/`has_node` script natives; plus `.tscn`-style
  text serialization (`scene::saveTree`/`loadTree`).

### Asset pipeline
- `core::AssetServer<T>` — asynchronous threaded loading with status/progress, dedup + ref-counting,
  and stamp-based reimport / hot reload (Godot's `ResourceLoader` threaded API).

### Infrastructure
- `platform::CrashHandler` — fatal-signal backtrace dump to stderr + a crash-log file, with symbol
  demangling (Godot's `CrashHandler`).
- `tools/package.sh` — one-command self-contained game export with a launch-verify step (Godot's
  "Export Project").
- API docs: `tools/gen_api_docs.py` → `docs/API.md` (144 headers) + a `Doxyfile`.
- `core::Telemetry` — opt-in, privacy-first analytics buffer (off by default, no built-in network
  transport, JSONL, no PII).

### Core systems
- `core::Replay<T>` — deterministic input record/replay with RLE serialization.
- `core::Checkpoints` — named save slots + a rolling rewind ring (rewind mechanics / rollback).
- `core::LinearArena` + `core::PoolAllocator` — frame arena + fixed-size pool allocators.
- `core::SmallVector<T,N>` + `core::SparseSet<T>` — inline-buffer vector + O(1) sparse set.
- `core::TypeDesc<T>` — minimal, type-safe reflection (member-pointer props + serialize).
- `core::DateTime` + `core::GameClock` — deterministic epoch↔calendar + in-game clock.
- `core::Version` — semantic version type (parse/compare) + build macros.

### Spatial & IO
- `game::Quadtree` + `game::Octree` — 2D/3D bounded-region spatial partitioning for broadphase
  culling and range queries.
- `io::VirtualFileSystem` — `res://` / `user://` scheme paths with normalization and a
  traversal-escape guard.

### Build & CI
- Cross-platform GitHub Actions CI (Linux build+test; macOS/Windows unit tests).
- Builds pass `-rdynamic` so crash backtraces resolve symbols.

---

_This project is under active development toward parity-or-better with the Godot game engine. Where a
capability can't be matched honestly (e.g. platform-specific packaging, GPU features requiring a real
device), it is documented as pending rather than claimed._
