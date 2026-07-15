# Maz Engine — API Reference

> Auto-generated from the engine headers by `tools/gen_api_docs.py`. Each module's summary is its header's own doc comment; the type and function lists are its public surface. This is a map — read the header for full signatures and semantics.

_144 headers across 16 subsystems._

## Contents

- [Core — foundation (time, jobs, events, RNG, resources, config)](#core)
- [Math — vectors, matrices, transforms, curves, geometry](#math)
- [Platform — window, input, filesystem, crash handling](#platform)
- [Render — Vulkan renderer, sprites, meshes, shapes, cameras](#render)
- [Scene — node tree, transforms, serialization](#scene)
- [Script — the maz::script language VM + engine binding](#script)
- [ECS — entity/component/system world](#ecs)
- [Game — physics, collision, AI, pathfinding, tilemaps](#game)
- [Anim — skeletons, clips, blending, tweening, curves](#anim)
- [Audio — mixer, DSP effects, spatialization, synthesis](#audio)
- [UI — controls, layout, theming, text](#ui)
- [FX — particles and force fields](#fx)
- [IO — JSON, config, serialization, resource packs](#io)
- [Input — action maps, analog helpers](#input)
- [Editor — scene model, gizmos, inspector](#editor)
- [`(root)`](#root)

<a name="core"></a>
## Core — foundation (time, jobs, events, RNG, resources, config)

### `Assert`
<sub>`engine/include/maz/core/Assert.hpp`</sub>

### `AssetServer`
<sub>`engine/include/maz/core/AssetServer.hpp`</sub>

maz::core::AssetServer — an asynchronous, reference-counted asset loader, Maz's answer to Godot's ResourceLoader (load_threaded_request / load_threaded_get_status / load_threaded_get) plus the .import reimport pipeline, in one header.  The problem it solves: a game must not stall the frame decoding a texture or parsing a model. So loading is split in two, exactly like Godot: 1. request(path)  — enqueues a decode job on a background JobSystem thread and returns an AssetId immediately. The frame keeps running. 2. poll()         — called once per frame on the game thread; it harvests any jobs that finished on workers, moves their results into the cache, and fires onLoaded callbacks. This is where a real engine would do the GPU upload — on the main thread, off the worker — which is why finalization is a separate step. status(id) reports Queued / Loading / Loaded / Failed; progress() gives a loaded/total pair for a loading screen. Identical paths dedupe to one entry with a reference count, so requesting the same texture from a hundred places loads it once (the ResourceCache guarantee, now async).  Reimport / hot reload: each entry remembers the "stamp" of its source (a file mtime or content hash you supply). reimportChanged() re-decodes every entry whose stamp moved and bumps its version(), so a renderer can notice `version` changed and re-upload the new bytes — the runtime half of Godot's reimport. All CPU, thread-based, no GPU: templated on your decoded payload type T, so it unit-tests deterministically by driving request()->poll() to completion.

**Types:** `AssetId`, `AssetServer`

### `CVars`
<sub>`engine/include/maz/core/CVars.hpp`</sub>

Config variables ("cvars"): a central registry of named, typed, self-describing tunables that any subsystem registers once and everyone can read or override — the engine's single source of truth for settings (render exposure, gameplay speed, UI scale, debug toggles). Each cvar carries a type, a default, a human-readable description, and — for numbers — an optional [lo, hi] clamp. Values can be set programmatically (typed setters clamp) or coerced from a string (for command-line flags and text configs), and the whole set can be iterated for a config UI or a settings file. This header is deliberately dependency-free (std only); JSON load/save lives in the io layer (io/Config.hpp) so core keeps zero dependencies.

**Types:** `CVarRegistry`

### `Config`
<sub>`engine/include/maz/core/Config.hpp`</sub>

Startup configuration, populated from command-line arguments.

**Types:** `AppConfig`

### `Events`
<sub>`engine/include/maz/core/Events.hpp`</sub>

A type-safe publish/subscribe event bus — the decoupling glue between systems. A gameplay system emits an event value (any type) and every subscriber registered for THAT type is invoked, without emitter and listener knowing about each other (damage -> audio + particles + score + UI, all independent). Subscription returns a token you can later unsubscribe. Dispatch snapshots the listener list, so a handler may safely subscribe/unsubscribe or emit further events during a call. Header-only; no GPU. Not thread-safe — intended for the single game thread.

**Types:** `EventBus`

### `Expression`
<sub>`engine/include/maz/core/Expression.hpp`</sub>

Runtime math-expression parser + evaluator — Godot's Expression class. Parse a formula string ONCE (e.g. "sin(x*3) * amp + 0.5"), naming the free variables, then execute() it repeatedly with different variable values. This is the workhorse behind data-driven design: damage/difficulty/economy formulas in a config file, procedural-parameter curves, spawn weights, tool sliders — anything you'd otherwise hard-code and recompile. Recursive-descent parser -> a flat AST node pool (copyable, no owning pointers) -> a pure recursive evaluator. Deterministic and dependency-free, so it unit-tests exactly and drives a function-plot golden.  Grammar (standard precedence; '^' is right-associative and binds tighter than unary minus, so -2^2 == -(2^2) == -4, and 2^2^3 == 2^(2^3)): expr   := term (('+'|'-') term)* term   := unary (('*'|'/'|'%') unary)* unary  := ('+'|'-') unary | power power  := primary ('^' unary)? primary:= number | const | ident | ident '(' [expr (',' expr)*] ')' | '(' expr ')'  Constants: pi, tau, e. Functions: sin cos tan asin acos atan exp log log2 sqrt abs floor ceil round sign frac (1-arg); pow atan2 min max mod (2-arg); clamp lerp (3-arg). Unknown names / bad arity / syntax errors set an error string and make parse() return false.  Honest scope vs Godot's Expression: this is the numeric subset — doubles in, one double out. It does NOT evaluate Variant types, strings, booleans/comparisons, array/dictionary literals, or method calls on an arbitrary base object; those are tied to Godot's Variant/Object model and are out of scope here.

**Types:** `Expression`

### `Interpolate`
<sub>`engine/include/maz/core/Interpolate.hpp`</sub>

Fixed-timestep render interpolation — Godot's physics interpolation. Physics runs on the fixed step, but the display refreshes at its own (usually higher, non-dividing) rate; drawing the raw physics state makes motion stutter. The fix is to keep the PREVIOUS and CURRENT physics values and, each render frame, blend them by the leftover accumulator fraction (`Clock::interpolationAlpha()`), so a body glides smoothly between steps. `Interpolated<T>` is that snapshot pair; `push` is called once per fixed step, `sample(alpha)` once per render frame. Pure math, header-only, deterministic.

**Types:** `Interpolated`, `Transform2DState`

**Functions:**

- `inline double clampAlpha(double a)`
- `inline double interpLerp(double a, double b, double t)`
- `inline float interpLerp(float a, float b, double t)`
- `inline math::vec2 interpLerp(const math::vec2& a, const math::vec2& b, double t)`
- `inline math::vec3 interpLerp(const math::vec3& a, const math::vec3& b, double t)`
- `inline float lerpAngle(float a, float b, double t)`
- `inline Transform2DState interpolate(const Transform2DState& a, const Transform2DState& b, double alpha)`

### `Jobs`
<sub>`engine/include/maz/core/Jobs.hpp`</sub>

A fixed-size worker thread pool with a task queue — the engine's parallelism foundation. submit() runs a callable on a worker and hands back a std::future for its result; parallelFor()/ parallelRanges() split an index range across the workers and block until the whole range is done, which is the common shape for data-parallel work (fractal/image gen, particle and transform updates, culling, batched pathfinding). The pool is created once and reused; the destructor drains outstanding tasks and joins. Not re-entrant: don't call parallelFor from inside a task (the caller thread blocks on the results, so nesting can starve the pool). Single-owner, single- submitter model — intended to be driven from the game thread.

**Types:** `JobSystem`

### `KeyValueStore`
<sub>`engine/include/maz/core/KeyValueStore.hpp`</sub>

A tiny persistent key/value store backed by a line-based `key=value` text file. Standard- library only (no SDL/JSON dependency) — the caller supplies the full file path (see platform::prefPath for a cross-platform writable location). Handy for settings, high scores, and simple progress.

**Types:** `KeyValueStore`

### `Log`
<sub>`engine/include/maz/core/Log.hpp`</sub>

### `Noise`
<sub>`engine/include/maz/core/Noise.hpp`</sub>

Procedural noise: smooth, seeded, reproducible pseudo-randomness over space — the primitive behind terrain heightmaps, cloud/marble textures, cave carving, and organic motion. This is classic Perlin gradient noise: a per-seed permutation table (shuffled with core::Random, so the same seed always gives the same field) plus fade/lerp interpolation of lattice gradients, yielding a value in about [-1, 1] that is 0 at integer lattice points and continuous everywhere. fbm2 layers octaves of it (fractal Brownian motion) for natural detail, normalized back into [-1, 1]. Header-only.

**Types:** `Noise`

### `Profiler`
<sub>`engine/include/maz/core/Profiler.hpp`</sub>

Hierarchical CPU profiler: named, nestable timing zones that answer "where did the frame go?". A zone is opened with begin(name) and closed with end(); zones nest, so the tree records both inclusive time (the whole span) and self time (inclusive minus the direct children) — the two numbers that actually locate a hotspot. Per frame each distinct zone name aggregates its call count, inclusive, and self microseconds; across frames an exponential moving average smooths the display so the numbers are readable instead of flickering.  The core is time-source-agnostic: begin/end take a monotonic microsecond timestamp, so tests and deterministic demos feed synthetic timestamps (no wall clock) and get reproducible output, while real code uses nowMicros() (steady_clock) or the ScopedZone RAII helper. Header-only, std only.

**Types:** `Profiler`

**Functions:**

- `inline uint64_t nowMicros()`

### `Random`
<sub>`engine/include/maz/core/Random.hpp`</sub>

Deterministic random-number generator: one seeded, reproducible source of randomness for gameplay and procedural generation, so a given seed always produces the same world/loot/spread — essential for replays, tests, and shareable "seed" content. Replaces the ad-hoc xorshift each demo used to hand-roll. The core is xoshiro256** (fast, high quality) seeded through SplitMix64 so even a small or zero seed fills the state well. Everything derives from next(): floats in [0,1), inclusive int ranges, weighted picks, Fisher-Yates shuffles, a Gaussian, and an angle. std-only (no glm) so core keeps zero dependencies. Header-only.

**Types:** `Random`

### `Resources`
<sub>`engine/include/maz/core/Resources.hpp`</sub>

A generic reference-counted resource cache — the core of an asset manager. Resources are keyed (typically by path or a string id); the first acquire() of a key builds the value via a loader callback and stores it, and every later acquire() of the same key returns the SAME instance and bumps a reference count. release() drops a reference and, when the count reaches zero, evicts the entry (optionally running an unload callback to free a GPU/file handle first). This is what lets a game request the same texture/mesh/sound a hundred times but load it once. Header-only, no GPU; works for any key/value pair, so it unit-tests without a renderer.  std::unordered_map is used for storage: references/pointers to a value stay valid across inserts and erases of OTHER keys, so a T& returned by acquire() remains valid until that key is evicted.

**Types:** `ResourceCache`

### `RingBuffer`
<sub>`engine/include/maz/core/RingBuffer.hpp`</sub>

RingBuffer<T> — a fixed-capacity circular buffer, the workhorse behind rolling histories (frame-time / FPS graphs, moving averages), input buffers (a fighting game's last-N button presses, jump "coyote" windows), replay traces, and streaming audio/network queues. It serves two idioms from one structure: * a ROLLING WINDOW — `push` always succeeds; once full it overwrites the OLDEST element, so the buffer always holds the most recent `capacity` items (the frame-time graph pattern). * a bounded FIFO QUEUE — `pushBack` reports whether it accepted (rejecting when full) and `popFront` drains oldest-first. Indexing is logical: `at(0)` is always the oldest live element, `at(size()-1)` the newest, regardless of where they physically wrap. Godot keeps a `RingBuffer` for exactly these jobs. Pure container, header-only, deterministic — it unit-tests exactly and drives a golden (a scrolling history plot).

**Types:** `RingBuffer`

### `SceneStack`
<sub>`engine/include/maz/core/SceneStack.hpp`</sub>

The application-framework layer: a stack of game "scenes" (menu, gameplay, pause, game-over) with a proper lifecycle. Pushing a scene pauses the one below and enters the new one; popping exits it and resumes the one revealed. Scenes can be transparent overlays (a pause menu drawn over the frozen game) via blocksUpdate()/blocksRender(). This is what ties menus and gameplay into a real game instead of a single-screen demo. render() is a hook (default no-op) so the core stays renderer-agnostic and unit-tests headless; the app overrides it. Stack mutations requested during update() are DEFERRED and applied afterwards, so a scene can safely pop or replace itself.

**Types:** `Scene`, `SceneStack`

### `Scheduler`
<sub>`engine/include/maz/core/Scheduler.hpp`</sub>

Time-based scheduling: the "do this later" and "do this on a beat" primitive nearly all gameplay needs — spawn a wave every few seconds, fire a callback after a delay, run a cooldown, drive a scripted sequence. Two pieces: * Scheduler — fire-and-forget timers: after(delay) runs a callback once; every(interval, count) runs it repeatedly (a finite count or forever); cancel() stops a pending one by handle. update() advances all timers and fires whatever came due, catching up if a big dt spans several intervals, and staying safe when a callback schedules or cancels timers mid-update. * Sequence — an ordered script of steps played over time: wait(seconds), call(fn), and span(duration, fn(progress 0..1)) for animated stretches; optionally loop. Built on the same fixed-step dt the rest of the engine runs on, so it's fully deterministic. Header-only.

**Types:** `Scheduler`, `Sequence`

### `Signal`
<sub>`engine/include/maz/core/Signal.hpp`</sub>

Per-object named signals — Godot's `signal` / `connect` / `emit`. The engine already has a global, by-TYPE publish/subscribe bus (`core::EventBus`), but Godot's signals are a different, more granular pattern: each OBJECT owns its own named channels ("this button's `pressed`", "this health's `changed`") that carry typed arguments, and other objects connect callbacks to a SPECIFIC emitter's signal. On top of plain connect/emit it adds the two flavours Godot leans on constantly — ONE-SHOT connections (fire once, then auto-disconnect) and DEFERRED connections (the call is queued and run later at a flush point instead of re-entrantly mid-emit). Header-only, type-safe, no allocation beyond the connection list; unit-tests headlessly.

**Types:** `Signal`

### `SlotMap`
<sub>`engine/include/maz/core/SlotMap.hpp`</sub>

Generational-index slot-map / object pool — the data structure behind stable, safe handles (Godot's RID, an ECS's entity ids). The problem it solves: you want to hand out lightweight IDs to pooled objects, REUSE storage when an object is freed, and still DETECT a stale ID that refers to a slot whose original occupant is long gone (the classic "ABA" dangling-handle bug). A `SlotMap` stores values in a dense-ish slot array; each slot carries a GENERATION counter. `insert` returns a `SlotHandle{index, generation}`; freeing a slot bumps its generation, so any handle minted before the free no longer matches and `get` returns null — even after the slot is reused by a brand-new object. Freed slots are recycled through a free list, so memory doesn't grow unbounded. Header-only, std-only, unit-testable.

**Types:** `SlotHandle`, `SlotMap`

### `StringId`
<sub>`engine/include/maz/core/StringId.hpp`</sub>

Interned strings — Godot's StringName. A game refers to the same names constantly (node names, signal names, animation tracks, input actions, entity tags), and comparing/hashing those as raw std::strings is slow and allocation-heavy. INTERNING each unique string once, into a table, turns every later reference into a small integer HANDLE: comparison is an int compare, hashing is trivial, and the original text is one reverse lookup away. Maz had no such facility — every subsystem hand-hashed or string-compared. This adds a `StringTable` (own the pool) + a lightweight `StringId` handle, plus the FNV-1a hash the table uses internally. std-only, header-only, no engine deps.

**Types:** `StringId`, `StringTable`, `hash`

**Functions:**

- `inline uint32_t fnv1a32(std::string_view s)`

### `Time`
<sub>`engine/include/maz/core/Time.hpp`</sub>

Fixed-timestep clock. Decouples the deterministic simulation step from render frame rate using the classic accumulator pattern (default step = 1/60 s).  Usage per frame: clock.beginFrame(); while (clock.consumeFixedStep()) { update(clock.fixedDelta()); } render(clock.interpolationAlpha());

**Types:** `Clock`


<a name="math"></a>
## Math — vectors, matrices, transforms, curves, geometry

### `Curve2D`
<sub>`engine/include/maz/math/Curve2D.hpp`</sub>

Cubic Bézier path — Godot's Curve2D / the spline a Path2D holds and a PathFollow2D walks. Maz had easing curves (anim) for scalar interpolation, but no *spatial* path: an authored smooth curve through a set of points that something can travel along at constant speed. That is what Curve2D provides. Each point carries a position plus `in`/`out` control handles (offsets relative to the point, exactly like Godot's Curve2D), and consecutive points are joined by a cubic Bézier. `sample`/`tangent` evaluate the geometric curve; `bake` walks it and lays down points spaced evenly by ARC LENGTH, so `sampleBaked(distance)` moves along the path at uniform speed (naive Bézier `t` bunches up where the curve bends). Header-only, math-only (no renderer), so it unit-tests headlessly; the app draws the curve, its handles, and the constant-speed points.

**Types:** `CurvePoint2D`, `Curve2D`

**Functions:**

- `inline vec2 cubicBezier(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t)`

### `Geometry2D`
<sub>`engine/include/maz/math/Geometry2D.hpp`</sub>

2D computational-geometry helpers — Godot's Geometry2D static class. These are the workhorse primitives behind AI line-of-sight, mouse/hit picking, path building, trigger zones, and collision pre-checks: does this segment cross that one, what is the nearest point on this edge, is this point inside that polygon. Maz had these scattered (triangle area in the triangulator, SAT in ConvexShape2D, ray/AABB in Collision); this consolidates the segment/polygon/circle set Godot exposes as one namespace. Pure math over vec2 — no allocation, no renderer — so it unit-tests exactly and drives a 2D golden.

**Types:** `SegmentHit`

**Functions:**

- `inline SegmentHit segmentIntersect(vec2 a, vec2 b, vec2 c, vec2 d)`
- `inline vec2 closestPointOnSegment(vec2 p, vec2 a, vec2 b)`
- `inline float distanceToSegment(vec2 p, vec2 a, vec2 b)`
- `inline bool pointInPolygon(vec2 p, const std::vector<vec2>& poly)`
- `inline bool segmentIntersectsCircle(vec2 a, vec2 b, vec2 center, float radius)`

### `Math`
<sub>`engine/include/maz/math/Math.hpp`</sub>

**Functions:**

- `inline mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar)`
- `inline mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar)`
- `inline mat4 orthographicSize(float verticalSize, float aspect, float zNear, float zFar)`
- `inline mat4 ortho2D(float width, float height)`

### `Rect2`
<sub>`engine/include/maz/math/Rect2.hpp`</sub>

Rect2 — Godot's Rect2. An axis-aligned rectangle given by `position` (the min corner) + `size`, with the full set of geometric operations that UI layout, view/camera culling, tilemap regions, and broadphase queries all reach for: point and overlap tests, intersection (clip), union (merge), containment (encloses), per-side grow/shrink, and expand-to-include-a-point. By convention (matching Godot) the right/bottom edges are EXCLUSIVE for point tests, and the operations assume a non-negative size — call abs() first if a size may be negative. Header-only, pure math, deterministic.

**Types:** `Rect2`

### `Transform2D`
<sub>`engine/include/maz/math/Transform2D.hpp`</sub>

Transform2D — Godot's Transform2D: the 2x3 affine matrix behind every Node2D. It stores two basis columns (`x`, `y`) plus an `origin` translation; a point maps as `x*p.x + y*p.y + origin`. This is the value type the engine's 2D world runs on — placing/parenting sprites, converting between local and world/screen space (xform / xformInv), composing a parent's transform with a child's (operator*), and reading back a node's rotation / scale / skew. It complements `scene::TransformGraph` (a hierarchy of decomposed TRS nodes): this is the flat matrix those nodes ultimately bake to. Pure math, header-only, deterministic — it unit-tests exactly and drives a golden (a shape drawn under several transforms).

**Types:** `Transform2D`


<a name="platform"></a>
## Platform — window, input, filesystem, crash handling

### `CrashHandler`
<sub>`engine/include/maz/platform/CrashHandler.hpp`</sub>

maz::platform::CrashHandler — a last-resort crash reporter, Maz's answer to Godot's CrashHandler. When the process hits a fatal signal (SIGSEGV / SIGABRT / SIGFPE / SIGILL / SIGBUS) it prints a labelled banner and a symbolized backtrace to stderr AND to a crash-log file, then restores the default handler and re-raises so the OS can still produce a core dump. That backtrace is often the only clue for a bug that only reproduces on a player's machine — the exact role Godot's handler plays.  Signal handlers may only call async-signal-safe functions, so the crash path deliberately uses raw write() + backtrace_symbols_fd() (both safe) rather than std::string formatting. The rich, std::string-based pieces — demangling a mangled frame into a readable C++ name, naming a signal, capturing the current stack — live as separate free functions used off the crash path (startup diagnostics, tests), so the whole module is verifiable without actually crashing the test runner.  POSIX (Linux/macOS) is fully supported via <execinfo.h>; on other platforms install() is a safe no-op and the helpers degrade gracefully, so engine code can call them unconditionally.

**Types:** `CrashConfig`, `CrashHandler`

**Functions:**

- `inline const char* signalName(int sig)`
- `inline std::string demangleSymbol(const std::string& line)`
- `inline std::vector<std::string> captureBacktrace(int maxFrames = 64, int skip = 1)`
- `inline std::string formatCrashBanner(const std::string& appName, const std::string& version,`

### `Input`
<sub>`engine/include/maz/platform/Input.hpp`</sub>

Gamepad axis/button ids. Values match SDL_GamepadAxis / SDL_GamepadButton ordering, so gameplay code can name inputs semantically without including SDL headers.

**Types:** `Input`

### `Paths`
<sub>`engine/include/maz/platform/Paths.hpp`</sub>

A writable, per-user, per-application directory with `file` appended (created if needed). Wraps SDL_GetPrefPath, e.g. ~/.local/share/<org>/<app>/<file> on Linux. Falls back to the bare filename (current directory) if the platform path can't be resolved.

### `Window`
<sub>`engine/include/maz/platform/Window.hpp`</sub>

**Types:** `WindowConfig`, `Window`


<a name="render"></a>
## Render — Vulkan renderer, sprites, meshes, shapes, cameras

### `AtlasPacker`
<sub>`engine/include/maz/render/AtlasPacker.hpp`</sub>

Rectangle bin packer for texture atlases — the layout step behind Godot's atlas/sprite-sheet importer and dynamic font glyph caches. Give it a bin of fixed width×height and a set of rectangle sizes; it places each without overlap and reports where (or that it did not fit). It uses the **Skyline Bottom-Left** heuristic (track the upper contour of what's placed; drop each rect at the position whose resulting top is lowest, ties broken to the left), which packs tightly and, crucially, is fully deterministic — same inputs always give the same layout — so it unit-tests exactly and renders a golden-stable atlas. `pack()` height-sorts a batch first (the standard heuristic) while returning placements in the caller's original order.  Honest scope: single-bin, axis-aligned, no rotation and no inter-rect padding (add spacing to your sizes if you need a gutter). It does NOT auto-grow the bin, pack across multiple pages, or do MaxRects / guillotine variants — those remain follow-ups.

**Types:** `PackSize`, `Placement`, `AtlasPacker`

### `Billboard`
<sub>`engine/include/maz/render/Billboard.hpp`</sub>

Billboarding — Godot's SpriteBase3D / GeometryInstance3D billboard modes. A billboard is a flat quad that turns to face the camera every frame, so a 2D image reads as a 3D object: trees, grass, smoke, health bars, distant-object impostors. This builds the model matrix that orients such a quad. Godot exposes three modes and so does this: `Disabled` (no turning — an ordinary placed quad), `Enabled` (full billboard — the quad's plane always squarely faces the camera), and `YBillboard` (fixed-Y — the quad yaws to face the camera horizontally but stays perfectly upright, the right choice for trees/characters that shouldn't tip back when you look down at them). It reads the camera's basis straight out of the view matrix, so it needs no camera object — just the same `view` you feed the renderer. Header-only, math-only; unit-tests the matrix without a GPU.

**Types:** `BillboardMode`

**Functions:**

- `inline math::mat4 buildBillboard(const math::vec3& position, const math::vec3& scale,`

### `Camera3D`
<sub>`engine/include/maz/render/Camera3D.hpp`</sub>

Camera3D — the projection helper behind Godot's Camera3D: it turns a view + projection matrix (built from a look-at + perspective, or supplied directly) plus a viewport size into the screen↔world queries games lean on every frame: * worldToScreen   — project a 3D world point to 2D screen pixels        (Godot unproject_position) * screenToRay     — unproject a screen pixel to a world-space pick ray  (Godot project_ray_origin/normal) * screenToWorld   — a world point a given distance down that ray        (Godot project_position) * frustum / isPointVisible / isSphereVisible — the six view planes + containment tests (Godot is_position_in_frustum) These drive mouse picking, world-space UI labels / health bars over 3D units, off-screen culling, and look-at aiming. It uses the engine's Vulkan clip convention (`math::perspective`: y-down NDC, depth 0..1), so screen coordinates have a top-left origin and match what the renderer draws. Pure matrix math, header-only, deterministic — it unit-tests exactly and drives a golden via a 2D projection of a 3D scene.

**Types:** `Ray3`, `Projected`, `FrustumPlanes`, `Camera3D`

### `Grid3D`
<sub>`engine/include/maz/render/Grid3D.hpp`</sub>

3D reference grid + gizmo axes — the ground grid and RGB axis marker every 3D editor viewport draws (Godot's Node3D editor). Maz could draw lit meshes and debug lines, but had no builder for the two spatial-reference primitives you constantly want when placing things in 3D: a WORLD-SPACE GROUND GRID (so you can read scale and position on the XZ plane) and an ORIGIN GIZMO (the X=red / Y=green / Z=blue axes that show which way is which). This is pure geometry — it emits a list of colored line segments — so it has no renderer dependency and unit-tests headlessly; the app draws each segment through the existing debug-line path (Renderer::drawLine).

**Types:** `Line3`, `GridSpec`

**Functions:**

- `inline std::vector<Line3> buildGrid(const GridSpec& spec)`
- `inline std::vector<Line3> buildWireBox(math::vec3 mn, math::vec3 mx, math::vec4 color)`

### `Line2D`
<sub>`engine/include/maz/render/Line2D.hpp`</sub>

2D polyline stroking — Godot's Line2D. Maz can FILL a convex polygon (drawConvexPolygon), but a polyline is a *stroke*: a path of points thickened to a ribbon of a given WIDTH, with the corners (JOINTS) and the two ends (CAPS) shaped so the ribbon reads as one continuous stroke — the primitive behind trails, drawn curves, graphs, outlines, and lightning. This turns a point list into a triangle soup (groups of three math::vec2) that any 2D fill path can draw; it is pure geometry (no renderer dependency, no allocation beyond the output), so it unit-tests headlessly and is deterministic.

**Types:** `JointMode`, `CapMode`, `PolylineStyle`

**Functions:**

- `inline math::vec2 perp(math::vec2 d)`
- `inline math::vec2 normalized(math::vec2 v)`
- `inline void emitTri(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c)`
- `inline void emitQuad(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c, math::vec2 d)`
- `inline void emitFan(std::vector<math::vec2>& out, math::vec2 center, math::vec2 a, math::vec2 b,`
- `inline bool lineIntersect(math::vec2 p0, math::vec2 d0, math::vec2 p1, math::vec2 d1, math::vec2& out)`
- `inline std::vector<math::vec2> buildPolyline(const std::vector<math::vec2>& pts, const PolylineStyle& style)`

### `MeshTools`
<sub>`engine/include/maz/render/MeshTools.hpp`</sub>

Mesh post-processing — the "fill in the vertex attributes an importer or generator left blank" step, Godot's SurfaceTool.generate_normals() / generate_tangents(). A raw mesh is often just positions + indices (+ maybe UVs): a heightfield you built procedurally, a decimated collision hull, a glTF that shipped without a NORMAL/TANGENT stream. Lighting needs a per-vertex NORMAL, and normal mapping needs a per-vertex TANGENT frame; deriving them from the geometry is a standard, well-defined computation. Maz could build primitive shapes (which bake their own normals) and load glTF (which may carry them), but had no way to (re)generate these for arbitrary geometry. Pure vector math, header-only, deterministic — it unit-tests exactly (a flat mesh yields the plane normal; a shared ridge yields the averaged normal) and drives a golden.  computeNormals uses AREA-WEIGHTED face accumulation: each triangle adds its un-normalized cross product (whose magnitude is twice the triangle area) to its three vertices, so larger faces pull a shared vertex more — the same default SurfaceTool uses, and it gives smooth results on curved meshes while degenerate (zero-area) triangles contribute nothing. computeTangents uses Lengyel's method (the one Godot/most engines use): accumulate per-triangle tangent/bitangent from the UV gradient, then Gram-Schmidt-orthonormalize against the normal and store handedness in .w so the shader can rebuild the bitangent as cross(normal, tangent.xyz) * tangent.w.  Scope note (honest): these operate on a single indexed triangle stream with fully shared vertices (smoothing groups are "every face that shares a vertex index"). They do not split vertices along hard edges / UV seams, weld a soft threshold, or triangulate polygons — a full SurfaceTool with index/dedup/seam handling remains a follow-up.

**Functions:**

- `inline float length3(const math::vec3& v)`
- `inline math::vec3 safeNormalize3(const math::vec3& v)`
- `inline std::vector<math::vec3> computeNormals(const std::vector<math::vec3>& positions,`
- `inline std::vector<math::vec4> computeTangents(const std::vector<math::vec3>& positions,`

### `Model`
<sub>`engine/include/maz/render/Model.hpp`</sub>

A glTF model loaded into CPU data: merged geometry plus its base-color texture (if any).

**Types:** `ModelData`, `SceneNode`, `SceneData`

### `MultiMesh2D`
<sub>`engine/include/maz/render/MultiMesh2D.hpp`</sub>

2D multi-mesh instancing — Godot's MultiMesh / MultiMeshInstance2D. Drawing a thousand grass blades, stars, or bullets as a thousand separate polygons means a thousand transform setups; a MultiMesh stores ONE base shape once plus a compact per-instance buffer (position, rotation, scale, colour) and stamps the shape at every instance. This is the 2D data structure for that: a convex base polygon in local space plus a list of `Instance2D`s. `transformedPolygon(i)` returns one instance's world-space polygon (for a per-instance coloured draw), and `bakeTriangles()` flattens EVERY instance into one triangle-fan soup ready to hand to a single batched draw / vertex upload. Header-only, math-only (no GPU state), so the transform math unit-tests headlessly; the app draws the baked instances.

**Types:** `Instance2D`, `MultiMesh2D`

**Functions:**

- `inline math::vec2 transformInstance(const Instance2D& inst, const math::vec2& local)`

### `PolyTriangulate`
<sub>`engine/include/maz/render/PolyTriangulate.hpp`</sub>

Ear-clipping triangulation of a SIMPLE polygon — the geometry behind Godot's Polygon2D fill.  Maz can already FILL a *convex* polygon (drawConvexPolygon triangulates it as a fan from vertex 0), but a fan only reads correctly when every interior angle is < 180 deg. Feed it a CONCAVE outline (a star, an arrow, an L / C / comb shape) and the fan spills triangles outside the shape. Godot's Polygon2D handles arbitrary simple polygons by triangulating them properly; this closes that gap.  triangulatePolygon() runs the classic O(n^2) ear-clipping algorithm: repeatedly find a "convex ear" (a vertex whose triangle with its two neighbours points outward and contains no other vertex) and snip it off, until a single triangle remains. It works on any simple polygon (no self-intersections, no holes) of EITHER winding — the winding is detected via signed area and normalised to CCW so the convexity test is consistent. The result is a flat list of vertex INDICES into `poly`, three per triangle, each triangle wound the same way as the (normalised CCW) input. Every triangle is convex, so it can be drawn by the existing convex-fill path. Pure geometry: no renderer dependency, no allocation beyond the output, deterministic and headlessly unit-testable.

**Functions:**

- `inline float triSignedArea2(const math::vec2& a, const math::vec2& b, const math::vec2& c)`
- `inline float polygonSignedArea2(const std::vector<math::vec2>& poly)`
- `inline float polygonArea(const std::vector<math::vec2>& poly)`
- `inline bool pointInTriangle(const math::vec2& p, const math::vec2& a, const math::vec2& b,`
- `inline std::vector<std::uint32_t> triangulatePolygon(const std::vector<math::vec2>& poly)`

### `Renderer`
<sub>`engine/include/maz/render/Renderer.hpp`</sub>

**Types:** `RendererConfig`, `Color`, `BlendMode`, `SpriteDesc`, `Point2`, `PolyVertex`, `Camera2D`, `MeshVertex`, `RenderStats`, `SceneLighting`, `Renderer`

### `Shapes`
<sub>`engine/include/maz/render/Shapes.hpp`</sub>

CPU geometry ready to hand to Renderer::createMesh. Normals point outward; every vertex is tinted `color`.

**Types:** `MeshData`

### `Shapes3D`
<sub>`engine/include/maz/render/Shapes3D.hpp`</sub>

**Functions:**

- `inline MeshVertex vtx(float px, float py, float pz, float nx, float ny, float nz, const Color& c,`
- `inline MeshData makeCylinder(float radius, float height, int sectors, const Color& color)`
- `inline MeshData makeCone(float radius, float height, int sectors, const Color& color)`
- `inline MeshData makeTorus(float majorRadius, float minorRadius, int majorSegs, int minorSegs,`
- `inline MeshData makeCapsule(float radius, float cylHeight, int sectors, int rings, const Color& color)`


<a name="scene"></a>
## Scene — node tree, transforms, serialization

### `GroupRegistry`
<sub>`engine/include/maz/scene/GroupRegistry.hpp`</sub>

Node groups — Godot's SceneTree groups (add_to_group / get_nodes_in_group / call_group / is_in_group). A group is a named tag you attach to any node; the registry answers "give me every node tagged X" (find all enemies, all save-points, everything to pause) and "run this on every node in X" — without the caller keeping and maintaining its own lists. Ids are plain integers, so this layers over ecs::World entities, scene::TransformGraph nodes, or an app's own handles. Membership is unique and kept in INSERTION ORDER, so queries and broadcasts are deterministic under the fixed timestep. A reverse index (node -> its groups) makes "which groups is this in?" and whole-node removal cheap. Header-only, no engine dependencies.

**Types:** `GroupRegistry`

### `Prefab`
<sub>`engine/include/maz/scene/Prefab.hpp`</sub>

Prefabs / instancing — Godot's PackedScene, the single most defining thing about Godot's workflow. A Prefab is a reusable TEMPLATE: a tree of named nodes, each carrying a bag of exported properties (position, colour, hp, …). You author it once and then INSTANTIATE it many times, each instance applying per-node OVERRIDES so every copy differs (a different spawn position, tint, or stat) without duplicating the template. Instancing produces a fresh, independent node tree — mutating one instance never touches the template or its siblings. Pure data (no GPU); header-only; unit-tests headlessly.

**Types:** `PropValue`, `PrefabNode`, `Prefab`

**Functions:**

- `inline const PropValue* findProp(const PropBag& bag, const std::string& key)`
- `inline void setProp(PropBag& bag, const std::string& key, const PropValue& value)`
- `inline float getFloat(const PropBag& bag, const std::string& key, float def = 0.0f)`
- `inline int getInt(const PropBag& bag, const std::string& key, int def = 0)`
- `inline bool getBool(const PropBag& bag, const std::string& key, bool def = false)`
- `inline math::vec2 getVec2(const PropBag& bag, const std::string& key, math::vec2 def = math::vec2(0.0f))`
- `inline math::vec4 getColor(const PropBag& bag, const std::string& key,`
- `inline PrefabNode* findNode(PrefabNode& root, const std::string& path)`
- `inline PrefabNode instantiate(const Prefab& prefab, const OverrideMap& overrides =`
- `inline int nodeCount(const PrefabNode& node)`

### `SceneSerialize`
<sub>`engine/include/maz/scene/SceneSerialize.hpp`</sub>

maz::scene text (de)serialization — Maz's answer to Godot's `.tscn` scene files. A whole node tree (structure + per-node transform, visibility, groups, and script class reference) round-trips to a small, human-readable, line-based text format. Scripts live in the script program (load them with SceneTree::loadScripts before loadTree); the scene only references classes by name.  std::string text = scene::saveTree(tree); SceneTree other; other.loadScripts(programSource); scene::loadTree(other, text);   // rebuilds the identical hierarchy, re-attaches scripts

**Functions:**

- `inline std::string num(double v)`
- `inline std::string pathOf(const SceneNode& node)`
- `inline std::string field(const std::string& line, const std::string& key)`
- `inline std::string writeNode(const SceneNode& node)`
- `inline std::string saveTree(SceneTree& tree)`
- `inline bool loadTree(SceneTree& tree, const std::string& text)`

### `SceneTree`
<sub>`engine/include/maz/scene/SceneTree.hpp`</sub>

maz::scene::SceneTree — the unified node hierarchy, Maz's answer to Godot's SceneTree + Node2D. It ties together the three things a real game needs in one model: 1. A tree of named nodes with parent/child relationships. 2. 2D transform hierarchy — a child's world transform composes with its parent's (position, rotation, and scale all propagate), so moving a parent moves its whole subtree. 3. Attached scripts — a node can carry a maz::script class whose _ready / _process(dt) / _physics_process(dt) hooks the tree drives in deterministic depth-first order, with the node's LOCAL transform exposed to the script as `self.node` (SC6 binding). Plus groups (broadcast to tagged nodes) and path lookup ("Player/Weapon"). This is the substrate a whole game — the ZOMBOID port included — is described and simulated on.

**Types:** `SceneTree`, `SceneNode`

### `TransformGraph`
<sub>`engine/include/maz/scene/TransformGraph.hpp`</sub>

A 2D transform hierarchy (scene graph): every node has a LOCAL transform (position, rotation, scale) relative to its parent, and update() propagates those into WORLD transforms parent-first. This is the structural backbone for composite objects — a turret on a tank, a moon around a planet, a hand on an arm, a health bar pinned to a unit: move or rotate the parent and the whole subtree follows. Composition is the standard decomposed TRS (world rotation = sum, world scale = product, world position = parent position + parent-rotated, parent-scaled local position), which is exact for uniform scale and the pragmatic norm for 2D engines. Header-only.

**Types:** `Transform2D`, `TransformGraph`


<a name="script"></a>
## Script — the maz::script language VM + engine binding

### `Script`
<sub>`engine/include/maz/script/Script.hpp`</sub>

maz::script — a small dynamically-typed scripting language with a tree-walking interpreter, the engine's answer to Godot's GDScript. Game logic can live in text scripts (hot-reloadable, no recompile) instead of compiled C++, and native C++ functions are exposed to scripts through a simple binding API. It is a pure, dependency-free, deterministic VM (no globals, no allocation surprises) — a deliberate edge over an embedded third-party runtime: a script can never reach outside the API the host hands it.  SC1: numbers / strings / bools / nil, the full operator set, variables, if/else, while, C-style for, functions + return, native host functions, line-numbered errors. SC2: arrays [..] and dictionaries {k: v} (reference semantics), indexing a[i] / d[k] / d.k with read + write, for-in over arrays / dict keys / ranges / string chars, break / continue, the `in` / membership operator, method calls (arr.append(x), dict.keys(), ...), len()/range(). SC3: string / math / conversion stdlib + a seedable deterministic RNG + assert. SC4: first-class functions — lambdas (func(x){...}), real closures capturing (and mutating) the scope they were defined in, and higher-order array methods (map/filter/reduce/any/all/ sort/sort_custom). Environments are heap-allocated (make_shared) so a returned closure keeps its captured scope alive after the enclosing call returns. SC5: classes — `class Foo { var fields; func methods }` with `self`, the `_init` constructor, `Foo.new(...)` / `Foo(...)` construction, single inheritance (`extends`) and `super` dispatch. Instances have reference semantics; methods read off an instance are bound callables. Classes are top-level, hoisted like functions. SC6: host-object binding — bindClass("T").property(get,set).method(fn) exposes a C++ type; makeNativeObject wraps a live host object behind a weak handle (touching a freed object is a catchable error, not a crash). instantiate()/objectHasMethod()/callOn() let the engine drive script instances' _ready / _process(dt) / _physics_process(dt) lifecycle hooks. SC7: signals — class-level `signal name;` fields + a standalone Signal() builtin; connect / disconnect / is_connected / emit / connection_count, one-shot connections, and sync-vs-deferred dispatch (emit_deferred queues; the host drains it via flushDeferred() for deterministic netcode ordering). Coroutine `await` is deferred to a VM-core pass. SC8: safety & diagnostics — stack traces on error (stackTrace()); an execution step budget (setStepBudget) and recursion limit (setRecursionLimit) that turn a runaway loop/recursion into a catchable error instead of a hang/crash; and a static warnings pass (warnings()) for variable shadowing and unreachable code. SC9: hot reload — reload(source) swaps in new code without a restart, updating global functions and class method bodies IN PLACE so live instances keep their field state while gaining the new behavior. A lex/parse failure leaves the previous version fully live. Old ASTs are retained so still-referenced closures stay valid. (Top-level statements are not re-run.) SC10: gradual typing — type hints (var x: int, func f(a: int) -> T, Array[int]) + inference (var x := ...); a static type-checker flags literal-level mismatches (typeErrors()); and setStrictTypes() promotes them to run() failures + enforces typed declarations at runtime. Untyped code stays fully dynamic. (Typed fast-path optimization is deferred.) SC11: modules & tooling — import "name" pulls a host-registered module's funcs/classes into scope (registerModule; transitive + cycle-safe, no filesystem access); introspection (has_method / call-by-name / get_property / set_property / has_property / class_name); and debugger hooks (onStep per statement + addBreakpoint/onBreakpoint). The SC1–SC11 roadmap is complete; a true coroutine `await` and a typed fast-path await a VM-core pass.  Everything is header-only to match the rest of maz::. The AST is owned by the Vm for the lifetime of a loaded program; runtime environments are reference-counted (shared_ptr) so closures capture.

**Types:** `FuncDef`, `Environment`, `ClassInfo`, `Instance`, `NativeClass`, `NativeObjectData`, `SignalData`, `Value`, `Tok`, `Token`, `ScriptError`, `Stmt`, `Expr`, `Parser`, `Vm`

**Functions:**

- `inline std::vector<Token> lex(const std::string& src, ScriptError& err)`
- `inline Value Value::newSignal(std::string name)`

### `ScriptSystem`
<sub>`engine/include/maz/script/ScriptSystem.hpp`</sub>

maz::script::ScriptSystem — the bridge that makes maz::script actually *drive the engine*. It attaches a script class (see SC5) to a game node and runs that instance's lifecycle hooks (_ready / _process(dt) / _physics_process(dt), see SC6) each frame, exposing the node's transform to the script through a bound "Node2D" host type (SC6 binding). This is the capstone that turns the scripting language from "a language" into "how you write Maz gameplay" — the same script-attached-to-node model as Godot, but sandboxed and deterministic.  ScriptSystem sys; sys.registerScript("Spinner", "func _process(dt) { self.node.rotation = self.node.rotation + dt; }"); auto node = sys.spawn("Spinner");   // returns a Node2D the host can read/move sys.process(0.016);                 // drives every attached _process(dt) float r = node->rotation;           // the script moved it

**Types:** `Node2D`, `ScriptInstance`, `ScriptSystem`


<a name="ecs"></a>
## ECS — entity/component/system world

### `World`
<sub>`engine/include/maz/ecs/World.hpp`</sub>

A lightweight entity-component system. Entities are ids; components live in per-type sparse sets. Iterate with each<T>() or view<A, B>(). Header-only so component types stay generic.  World w; Entity e = w.create(); w.add<Transform>(e, {0, 0}); w.add<Velocity>(e, {10, 0}); w.view<Transform, Velocity>([&](Entity, Transform& t, Velocity& v) { t.x += v.vx * dt; });

**Types:** `IPool`, `Pool`, `World`


<a name="game"></a>
## Game — physics, collision, AI, pathfinding, tilemaps

### `AStar2D`
<sub>`engine/include/maz/game/AStar2D.hpp`</sub>

General weighted-graph A* over arbitrary 2D points — Godot's AStar2D. Unlike NavGrid (a uniform walkable/blocked grid) and NavMesh (A* across convex mesh cells), this is a free-form graph: you place points at any position with any integer id, connect them however you like (roads, rails, waypoint webs, teleporters, skill/ability graphs), then query the least-cost route. Each point carries a `weightScale` that multiplies the cost of moving INTO it, so you can make some nodes expensive (mud, danger) without moving them. Edges may be one-way (bidirectional=false). Traversal cost between connected points is their Euclidean distance × the destination's weight scale; the A* heuristic is the straight-line distance to the goal (admissible when every weight ≥ 1). Points are held in an ordered map and each point's neighbours in an ordered set, so identical graphs produce identical paths — deterministic, header-only, GPU-free, so it unit-tests exactly.  Honest scope vs Godot's AStar2D: this covers the graph, weights, one-/two-way links, id/point paths, closest-point and closest-position-in-segment queries. It does NOT override _compute_cost / _estimate_cost via subclassing (the cost model is fixed to weighted Euclidean), nor does it expose AStar3D or AStarGrid2D — those remain separate.

**Types:** `AStar2D`

### `Area2D`
<sub>`engine/include/maz/game/Area2D.hpp`</sub>

Area2D — a sensor / trigger region. Unlike a rigid body (Physics2D) it pushes nothing; it just reports which things OVERLAP it and fires ENTER / EXIT as they cross its boundary. This is Godot's Area2D, and it's how nearly every game does pickups, hurt/hit boxes, checkpoints, doorways, and water / wind / gravity zones. It's pure geometry (circle & axis-aligned box overlap) plus set-diffing (this frame's members vs last frame's), so it's deterministic and unit-tests headlessly.

**Types:** `Area2D`, `AreaMonitor`

**Functions:**

- `inline math::vec2 closestOnBox(math::vec2 p, math::vec2 center, math::vec2 half)`
- `inline bool overlaps(const Area2D& a, const Area2D& b)`

### `AutoTile`
<sub>`engine/include/maz/game/AutoTile.hpp`</sub>

Procedural cave generation + tilemap autotiling — the pieces behind Godot's TileMap terrain (autotiling) sets and the classic cellular-automata cave generator. A grid stores 1 = solid (wall), 0 = open (floor). CellularCave grows organic caverns from seeded noise; autotileMask4 turns the grid into per-cell edge bitmasks so a renderer can pick the right border tile. Pure logic (only core::Random) — deterministic under a seed, so it unit-tests headlessly and a given seed always yields the same cave.

**Types:** `CellularCave`

**Functions:**

- `inline uint8_t autotileMask4(const std::vector<uint8_t>& solid, int w, int h, int x, int y)`
- `inline int autotileIndex4(uint8_t mask4)`

### `Avoidance`
<sub>`engine/include/maz/game/Avoidance.hpp`</sub>

Local collision avoidance — Godot's NavigationAgent2D avoidance (RVO). Each agent has a PREFERRED velocity (usually "toward my goal"); rvoVelocity nudges it to a nearby velocity that won't run into moving neighbours, by sampling candidate velocities and scoring each on time-to-collision plus how far it strays from the preference. It is RECIPROCAL: every agent runs the same rule and each takes half the avoidance (the velocity-obstacle is centred on the average velocity, `2·c − vA − vB`), so a pair on a head-on course peels apart smoothly instead of oscillating. Pure 2D math — deterministic, no GPU — so it unit-tests headlessly and a scene replays identically.

**Types:** `AvoidNeighbor`

**Functions:**

- `inline float timeToCollision(math::vec2 relPos, math::vec2 relVel, float r)`
- `inline math::vec2 rvoVelocity(math::vec2 pos, math::vec2 vel, math::vec2 prefVel, float radius,`

### `BehaviorTree`
<sub>`engine/include/maz/game/BehaviorTree.hpp`</sub>

Behavior trees — a scalable, reactive alternative to the finite state machine for AI decisions. A tree is ticked every frame; each node returns Success, Failure, or Running. Composites route the tick: a Sequence runs children until one is not Success (AND), a Selector until one is not Failure (fallback / priority OR). These composites are REACTIVE (memoryless): every tick re- evaluates from the first child, so a higher-priority branch (e.g. "flee") can pre-empt a running lower-priority one (e.g. "patrol") the instant its condition flips — the behaviour you want for reactive agents. Leaves wrap gameplay via std::function. Header-only, no GPU, so it unit-tests headless.

**Types:** `Node`, `Action`, `Condition`, `Sequence`, `Selector`, `Inverter`, `Blackboard`, `Parallel`, `Repeater`, `AlwaysSucceed`, `AlwaysFail`, `Tap`, `BehaviorTree`

**Functions:**

- `inline NodePtr action(std::function<Status()> fn)`
- `inline NodePtr condition(std::function<bool()> fn)`
- `inline NodePtr inverter(NodePtr child)`
- `inline NodePtr repeater(int count, NodePtr child)`
- `inline NodePtr alwaysSucceed(NodePtr child)`
- `inline NodePtr alwaysFail(NodePtr child)`
- `inline NodePtr tap(int* out, NodePtr child)`

### `CameraController2D`
<sub>`engine/include/maz/game/CameraController2D.hpp`</sub>

A 2D follow camera: the controller every 2D game needs but the engine only had the pieces for (a raw Camera2D data struct + a separate Shake). It tracks a target with three standard behaviors layered together: * Deadzone — a box around the current focus the target can move within WITHOUT scrolling the camera; only when the target leaves the box does the camera move (to put it back on the edge). Kills jitter from tiny target motion. * Smoothing — the camera eases toward its desired focus with a frame-rate-independent exponential approach (higher = snappier), so scrolling feels weighty instead of locked. * World bounds — the visible rectangle is clamped inside the level so the camera never shows past the edges; if the world is smaller than the view on an axis, that axis is centered. A shake offset can be added on top without feeding back into the follow position. This is math-only (no renderer dependency): the app reads center()/zoom() to fill a render::Camera2D. Header-only.

**Types:** `CameraController2D`

### `Collision`
<sub>`engine/include/maz/game/Collision.hpp`</sub>

Axis-aligned bounding box.

**Types:** `Aabb`, `RayHit`

**Functions:**

- `inline RayHit raycastAabb(const math::vec3& origin, const math::vec3& dir, const Aabb& box,`
- `inline RayHit raycast(const math::vec3& origin, const math::vec3& dir,`
- `inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,`

### `CollisionLayers`
<sub>`engine/include/maz/game/CollisionLayers.hpp`</sub>

Collision layers & masks — Godot's collision_layer / collision_mask. Overlap geometry (Area2D, Physics2D) answers "do these two shapes touch?"; layers answer the OTHER half every game needs: "should these two even be considered?". Each object lives in some LAYERS ("what I am": player, enemy, pickup, wall) and scans some MASK ("what I react to"). A pickup magnet scans only the pickup layer; an enemy hurtbox scans only the player layer; the player's bullets scan only enemies. Without this, every zone reacts to everything. It's pure bitmask logic — no allocation on the hot path — so it's deterministic and unit-tests headlessly. Godot 2D exposes 32 layers; LayerMask is a 32-bit set.

**Types:** `CollisionObject2D`, `LayerRegistry`

**Functions:**

- `inline constexpr LayerMask layerBit(int index)`
- `inline LayerMask layerMask(std::initializer_list<int> indices)`
- `inline bool detects(LayerMask observerMask, LayerMask targetLayer)`
- `inline bool interact(LayerMask aLayer, LayerMask aMask, LayerMask bLayer, LayerMask bMask)`

### `CombineMode`
<sub>`engine/include/maz/game/CombineMode.hpp`</sub>

How two bodies' per-body friction / restitution scalars combine into the effective pair value (Godot PhysicsMaterial / Box2D). GeometricMean = sqrt(a*b) is the physically-standard friction combine and the engine's historical default; Max is the usual restitution choice. Shared by the 2D and 3D physics solvers so the same material semantics apply in both.

**Functions:**

- `inline float combineValue(CombineMode mode, float a, float b)`

### `ConvexShape2D`
<sub>`engine/include/maz/game/ConvexShape2D.hpp`</sub>

Arbitrary 2D CONVEX POLYGON collision via the Separating-Axis Theorem (SAT) — Godot's ConvexPolygonShape2D / CollisionPolygon2D. Physics2D already collides circles and (oriented) boxes, and it uses SAT internally for the box-box case, but there was no way to collide an ARBITRARY convex shape: a triangle, a pentagon, a hexagonal bumper, a hand-authored hull. This adds a standalone convex-polygon type plus the two queries every 2D game wants against it — "do these two shapes overlap, and if so which way and how far do I push to separate them (the minimum translation vector)?" and "is this point inside this shape?" — as pure geometry with no simulation or renderer dependency, so it unit-tests headlessly.  SAT: two convex shapes are disjoint iff there exists a separating axis — a line onto which their projections don't overlap. The candidate axes are the face normals of both polygons. If every axis shows overlap, the shapes intersect, and the axis of MINIMUM overlap gives the MTV (push direction + penetration depth). Only valid for CONVEX polygons (decompose concave shapes into convex pieces first).

**Types:** `ConvexPoly2D`, `SatHit2D`

**Functions:**

- `inline void projectPoly(const ConvexPoly2D& p, const math::vec2& axis, float& mn, float& mx)`
- `inline math::vec2 polyCenter(const ConvexPoly2D& p)`
- `inline bool testAxes(const ConvexPoly2D& src, const ConvexPoly2D& a, const ConvexPoly2D& b,`
- `inline SatHit2D satOverlap(const ConvexPoly2D& a, const ConvexPoly2D& b)`
- `inline bool polyContains(const ConvexPoly2D& p, const math::vec2& pt)`
- `inline ConvexPoly2D makeRegularPoly(const math::vec2& center, float radius, int sides, float rotation = 0.0f)`
- `inline ConvexPoly2D makeBoxPoly(const math::vec2& center, const math::vec2& half, float angle = 0.0f)`

### `FlowField`
<sub>`engine/include/maz/game/FlowField.hpp`</sub>

Flow-field (vector-field) pathfinding — the crowd-movement technique the per-agent A* of Godot's NavigationServer (and Maz's own NavGrid, M57) doesn't provide. When MANY agents share ONE goal you don't path each of them: you run a single Dijkstra OUTWARD from the goal to get an INTEGRATION FIELD (least cost-to-goal for every cell), then bake a FLOW FIELD — each cell stores a unit direction pointing down that cost gradient toward the goal. Every agent then navigates for free: it just reads the direction under its feet and walks. So a thousand units route around walls to the goal for the price of one search, and the paths update in one pass when the goal moves. Pure grid math (8-connected Dijkstra, diagonal cost sqrt(2), no corner cutting), deterministic and GPU-free, so it unit-tests headlessly.

**Types:** `FlowField`

### `FlyCamera`
<sub>`engine/include/maz/game/FlyCamera.hpp`</sub>

A first-person "fly" camera: a position plus yaw/pitch, driven by move()/look(), producing a view matrix. Engine-agnostic (math only) — the app maps its input to the move/look axes.

**Types:** `FlyCamera`

### `Goap`
<sub>`engine/include/maz/game/Goap.hpp`</sub>

Goal-Oriented Action Planning — a step beyond the behaviour tree. Instead of an author hand-wiring what an agent does, the agent is given a GOAL (a desired world-state) and a LIBRARY of actions, each with preconditions and effects, and it PLANS backward-optimally the cheapest sequence of actions that carries the current world from where it is to the goal. This is the classic F.E.A.R. AI technique; it gives emergent, re-plannable behaviour (drop in a new action and every agent can use it, no tree edits) that Godot ships no built-in equivalent for. Pure integer/graph search — deterministic, headless- testable, and it replays identically every run.  The world is a set of boolean facts packed into a 64-bit word (bit i = fact i is true). A Condition is a PARTIAL state: `mask` marks which facts it constrains and `want` their required truth, so a goal or a precondition can care about three facts and ignore the other sixty-one. The planner is A* over world states: g = accumulated action cost, h = an admissible lower bound (you still need at least one more action, costing at least the cheapest action, whenever the goal is unmet), so the first plan it pops is guaranteed minimum-cost.

**Types:** `Condition`, `Action`, `Plan`

**Functions:**

- `inline constexpr State bit(int i)`
- `inline bool satisfied(State s, const Condition& c)`
- `inline State apply(State s, const Action& a)`
- `inline Plan plan(State start, const Condition& goal, const std::vector<Action>& library,`

### `GravityField2D`
<sub>`engine/include/maz/game/GravityField2D.hpp`</sub>

Area gravity fields — Godot's Area2D gravity override. A rectangular zone can change the gravity a body feels while inside it: a DIRECTIONAL field pushes a fixed way (wind, an updraft, a sideways conveyor of force), a POINT field pulls toward (or pushes from) a centre with inverse-square falloff (a planet, a black hole, a magnet). Zones carry a PRIORITY and a mode — REPLACE (this zone's gravity overrides what lower zones set, like Godot's "Replace") or ADD (accumulate on top, like Godot's "Combine"). `gravityAt` walks the zones low-priority-first and returns the final gravity vector at a point, starting from the world's default. Pure geometry + vector math, header-only, deterministic — it unit-tests exactly and a fixed-step sim drives a golden.

**Types:** `GravityArea2D`

**Functions:**

- `inline math::vec2 zoneGravity(const GravityArea2D& a, math::vec2 p)`
- `inline math::vec2 gravityAt(const std::vector<GravityArea2D>& areas, math::vec2 p, math::vec2 base)`

### `KinematicBody2D`
<sub>`engine/include/maz/game/KinematicBody2D.hpp`</sub>

2D kinematic character controller — Godot's CharacterBody2D.move_and_slide, the single most-used movement primitive for platformers and top-down games. Rigid bodies (Physics2D) are driven by forces; a *kinematic* character is driven directly by a velocity and must not tunnel through or stick into the static world. moveAndSlide sweeps an axis-aligned body along its motion, stops at the first contact, and SLIDES the leftover motion along the surface — repeating for a few iterations so a body can round a corner or run along a wall in one call. Each contact is classified against an `up` direction into floor / wall / ceiling (via a max floor angle), so gameplay can ask is_on_floor()/is_on_wall(). Pure geometry over a list of static AABBs — no allocation beyond the solids the caller owns — so it unit-tests exactly and drives a deterministic golden. (Discrete resolution against oriented/rotated shapes and moving platforms remain future work; this is AABB-vs-AABB swept collision.)

**Types:** `Aabb2`, `SweptHit`, `SlideResult`

**Functions:**

- `inline SweptHit sweptAabb(math::vec2 pos, math::vec2 half, math::vec2 motion, const Aabb2& solid)`
- `inline SlideResult moveAndSlide(math::vec2 pos, math::vec2 half, math::vec2 velocity, float dt,`

### `NavGrid`
<sub>`engine/include/maz/game/NavGrid.hpp`</sub>

A uniform 2D navigation grid over the X/Z ground plane with A* pathfinding. Cells are either walkable or blocked; findPath returns a least-cost route between two cells using 8-directional movement (orthogonal cost 1, diagonal cost sqrt(2)) with an octile-distance heuristic and corner-cutting disallowed (a diagonal step is blocked if either orthogonally-adjacent cell it squeezes past is solid). World<->cell mapping matches the engine's ground-plane convention: X and Z map to grid columns/rows, Y is ignored. Header-only and dependency-free so it unit-tests without a GPU.

**Types:** `NavGrid`

### `NavMesh`
<sub>`engine/include/maz/game/NavMesh.hpp`</sub>

Navigation-mesh pathfinding: polygon-based navigation, the step up from a uniform grid (NavGrid) and the analogue of Godot's NavigationServer / NavigationPolygon. The walkable area is described by a set of CONVEX polygon cells that share edges; build() finds those shared edges (portals) to form a cell graph. findPath() then A*-searches the graph for the corridor of cells between two points and runs the "simple stupid funnel" algorithm over the corridor's portals to string-pull a short, smooth path that hugs corners — instead of the staircase a grid produces. Header-only, dependency-free (2D math only), so it unit-tests without a GPU.

**Types:** `NavPoly`, `NavMesh`

### `NormalLight2D`
<sub>`engine/include/maz/game/NormalLight2D.hpp`</sub>

Normal-mapped 2D lighting — Godot's Light2D with a normal map. A flat 2D sprite carries a NORMAL MAP (a per-texel surface normal, +z pointing out of the screen); a 2D light then shades each texel by how squarely its normal faces the light, so a painted-flat brick wall or ground catches light directionally and reads as three-dimensional — bumps lit on the side facing the light, shadowed on the far side, the highlight sliding across as the light moves. Earlier lights (M89/M101) were flat-coloured pools with occluder shadows but no per-texel normal response; this adds it. It is pure vector math (a 3D point-light Lambert term + smooth distance attenuation), deterministic and headless-testable; a renderer just fills each shaded texel/cell with the returned colour.

**Types:** `PointLight2D`

**Functions:**

- `inline math::vec3 shadePointLight(math::vec2 p, math::vec3 n, math::vec3 albedo, const PointLight2D& L)`
- `inline math::vec3 shadeSurface(math::vec2 p, math::vec3 n, math::vec3 albedo,`
- `inline math::vec3 decodeNormal(math::vec3 rgb)`

### `OneWayPlatform`
<sub>`engine/include/maz/game/OneWayPlatform.hpp`</sub>

One-way platforms — Godot's `one_way_collision` on StaticBody2D / TileMap collision shapes. A one-way platform is solid only from ONE side: a character falling onto it lands, but a character jumping up from below passes straight through (and, standing on it, can drop through by tapping down). Maz's Physics2D collides solid boxes both ways; this adds the swept "solid-from-above" resolve a platformer needs. It is pure geometry over a horizontal surface: given a body's vertical span this frame (prevBottom -> curBottom) and its horizontal extent, `resolveOneWayPlatform` reports whether the body crossed the surface FROM ABOVE while descending (a landing) and the snapped resting height; a body moving up, or already below the surface, is never blocked. `resolveOneWayPlatforms` picks the topmost surface a falling body lands on this step. Header-only, no renderer/sim dependency, so it unit-tests headlessly; the app runs a fixed-step simulation and draws the settled result.

**Types:** `OneWayPlatform2D`, `OneWayResult`

**Functions:**

- `inline OneWayResult resolveOneWayPlatform(float prevBottom, float curBottom, float bx0, float bx1,`
- `inline OneWayResult resolveOneWayPlatforms(float prevBottom, float curBottom, float bx0, float bx1,`

### `Parallax`
<sub>`engine/include/maz/game/Parallax.hpp`</sub>

Parallax scrolling backgrounds — Godot's ParallaxBackground + ParallaxLayer. A staple of 2D games that Maz had no notion of: several background layers that scroll at DIFFERENT rates relative to the camera so the scene reads as having depth (distant mountains barely move, near foliage races past), each layer TILED/MIRRORED so a small motif covers an unbounded scroll. This is pure transform math — given the camera's scroll and a layer's motion scale, it produces the layer's on-screen offset and the tiling helpers to cover the viewport — so the app draws whatever art it likes at the returned positions. No renderer dependency; header-only; unit-tests headlessly.

**Types:** `ParallaxLayer`

**Functions:**

- `inline math::vec2 layerOffset(const ParallaxLayer& layer, math::vec2 cameraScroll)`
- `inline float pmod(float a, float period)`
- `inline float firstTile(float offset, float period)`
- `inline int tileCount(float extent, float period)`

### `Physics2D`
<sub>`engine/include/maz/game/Physics2D.hpp`</sub>

Impulse-based 2D rigid-body dynamics — the layer above collision *detection* (this module resolves collisions, not just reports them). Bodies are circles or axis-aligned boxes carrying velocity, an inverse mass (0 = immovable/infinite mass), restitution (bounciness), and friction; the world integrates gravity, then resolves overlaps with a normal impulse + Coulomb friction impulse + a positional correction (so stacks don't sink), and bounces bodies off a static box. Deterministic under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly. Coordinates are whatever the caller uses (the demos use screen pixels with +y pointing down).

**Types:** `Body2D`, `Bounds2D`, `ContactEvent`, `Joint2D`, `Manifold`, `Contact2`, `ContactConstraint`, `PhysicsWorld2D`

**Functions:**

- `inline Body2D makeWorldBoundary(math::vec2 normal, math::vec2 pointOnPlane)`
- `inline Body2D makePolyline(const std::vector<math::vec2>& points, float thickness = 0.0f)`
- `inline bool contactCircleCircle(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline bool contactBoxBox(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline bool contactCircleBox(const Body2D& c, const Body2D& x, math::vec2& nCircleToBox, float& pen)`
- `inline bool contact(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline void resolveContact(Body2D& a, Body2D& b, const math::vec2& n, float pen)`
- `inline bool collide(Body2D& a, Body2D& b)`
- `inline bool collideCircles(Body2D& a, Body2D& b)`
- `inline void collideBounds(Body2D& b, const Bounds2D& bounds)`
- `inline float cross2(math::vec2 a, math::vec2 b)`
- `inline math::vec2 crossSV(float s, math::vec2 v)`
- _…and 41 more_

### `Physics3D`
<sub>`engine/include/maz/game/Physics3D.hpp`</sub>

Impulse-based 3D rigid-body dynamics — the 3D sibling of Physics2D. This module resolves collisions (it doesn't just report them). Bodies carry a position, linear velocity, an inverse mass (0 = immovable/infinite mass), restitution (bounciness) and friction; the world integrates gravity with semi-implicit Euler, detects contacts, and resolves them with a rotation-aware normal impulse + Coulomb friction impulse + a positional correction (so resting bodies don't sink). Deterministic under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly.  D1 shipped dynamic Spheres + a static ground Plane with linear impulses. D2 (this milestone) adds Box (OBB) shapes and full angular dynamics: an inverse-inertia tensor per body, quaternion orientation integration, and contact impulses applied at the contact point (lever arms), so a tilted box dropped on the ground tumbles and settles flat, and a sphere landing off-centre on a box imparts spin. Rotation is opt-in via enableRotation() — a body left with a zero inverse-inertia tensor behaves exactly like the D1 translation-only body, so every D1 result is unchanged. Box-vs-box contact (3D SAT) and warm-started stacking arrive in later milestones.

**Types:** `Body3D`, `Contact3`, `Constraint3`, `Joint3D`, `RayHit3`, `PhysicsWorld3D`, `MoveResult3`

**Functions:**

- `inline Body3D makeSphere(math::vec3 pos, float radius, float mass = 1.0f)`
- `inline Body3D makeBox(math::vec3 pos, math::vec3 half, float mass = 1.0f)`
- `inline Body3D makeCapsule(math::vec3 pos, float radius, float halfHeight, float mass = 1.0f)`
- `inline Body3D makeGroundPlane(math::vec3 normal, math::vec3 pointOnPlane)`
- `inline Contact3 sphereSphere(int ia, const Body3D& a, int ib, const Body3D& b)`
- `inline Contact3 spherePlane(int is, const Body3D& s, int ip, const Body3D& p)`
- `inline Contact3 sphereBox(int is, const Body3D& s, int ib, const Body3D& box)`
- `inline void boxPlane(int ibox, const Body3D& box, int ip, const Body3D& p,`
- `inline void closestSegSeg3(const math::vec3& p1, const math::vec3& q1, const math::vec3& p2,`
- `inline std::vector<math::vec3> clipPoly3(const std::vector<math::vec3>& poly, const math::vec3& planeN,`
- `inline void capsuleSegment3(const Body3D& c, math::vec3& p0, math::vec3& p1)`
- `inline math::vec3 closestOnSeg3(const math::vec3& p, const math::vec3& a, const math::vec3& b)`
- _…and 16 more_

### `PhysicsQuery2D`
<sub>`engine/include/maz/game/PhysicsQuery2D.hpp`</sub>

2D physics-space queries — the "ask the world a spatial question" side of a 2D physics engine (Godot's PhysicsDirectSpaceState2D): cast a RAY and get the first collider it hits, test whether a POINT lands inside any collider, and cast a bounded SEGMENT. These are the primitives behind line-of-sight checks, hitscan weapons, ground/wall probes, and mouse picking — Maz had rigid-body dynamics + contact generation (Physics2D) but no way to *query* the collider set without stepping the simulation. This is pure geometry against static shape descriptions, so it has no renderer or simulation dependency and unit-tests exhaustively.  Shapes are circles or ORIENTED boxes (a box with a rotation), matching Physics2D's Body2D shapes. Every query takes a 32-bit collision MASK; a shape is only considered when `shape.layer & mask` is non-zero (Godot collision_mask semantics), so callers can probe "only walls" or "only enemies".

**Types:** `QueryShape2D`, `RayHit2D`

**Functions:**

- `inline bool rayCircle(const math::vec2& O, const math::vec2& D, const math::vec2& C, float r, float tMax,`
- `inline bool rayBox(const math::vec2& O, const math::vec2& D, const math::vec2& C, const math::vec2& half,`
- `inline bool pointInShape(const math::vec2& p, const QueryShape2D& s)`
- `inline RayHit2D queryRay(const math::vec2& origin, const math::vec2& dir,`
- `inline RayHit2D querySegment(const math::vec2& a, const math::vec2& b,`
- `inline std::vector<int> queryPoint(const math::vec2& p, const std::vector<QueryShape2D>& shapes,`

### `Shake`
<sub>`engine/include/maz/game/Shake.hpp`</sub>

Trauma-based camera shake (after Squirrel Eiserloh's "Juicing Your Cameras"). Impactful moments add "trauma" in [0,1]; the shake amount is trauma^2 so small hits barely wobble while big ones jolt, and trauma decays linearly so the shake settles on its own. The offset/rotation are pure functions of (trauma, time) built from layered sines, so no RNG state is needed and the motion is smooth and deterministic. Feed the offset into the camera position and the yaw/pitch into the look angles each frame.

**Types:** `Shake`

### `ShapeCast2D`
<sub>`engine/include/maz/game/ShapeCast2D.hpp`</sub>

Swept-shape casting — continuous collision detection, the "move a shape and find the first thing it hits" query behind Godot's ShapeCast2D and PhysicsDirectSpaceState2D.cast_motion. PhysicsQuery2D already answers ZERO-radius questions (a ray, a segment, a point); this answers the FINITE-radius one: sweep a circle of radius `r` from A to B through a set of static colliders and report the first contact — the fraction of the motion travelled, the world contact point, the surface normal, and the caster's centre at that instant. That is what stops a fast projectile or a character-controller step from TUNNELLING through a thin wall in a single frame (a discrete overlap test at A and at B misses a wall that sits entirely between them). It is also the primitive under a "how far can I move before I touch something" probe.  The obstacle set reuses PhysicsQuery2D's `QueryShape2D` (circles + oriented boxes) and its 32-bit collision MASK, so the same world description feeds rays, points, and now swept circles. The maths is the Minkowski sum: sweeping a circle of radius r against a shape is the same as sweeping a POINT (the circle's centre) against that shape GROWN by r — a circle of radius R+r, or a box rounded by r. So a circle-vs-circle sweep is a ray against an inflated circle, and a circle-vs-box sweep is a ray against the box's four r-offset faces plus four r-radius corner arcs. Pure geometry, header-only, deterministic — it unit-tests exactly (a hit leaves the caster exactly r from the surface) and drives a golden.  Scope note (honest): this casts a CIRCLE (the common character/projectile probe). Casting an arbitrary oriented BOX or convex polygon along a motion — Godot's full ShapeCast2D with any CollisionShape2D — is the follow-up; so is returning Godot's separate safe/unsafe fractions for a start-in-contact shape (here a caster that already overlaps reports contact at fraction 0 with a push-out normal).

**Types:** `ShapeCastHit2D`

**Functions:**

- `inline bool sweptCircleCircle(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,`
- `inline bool sweptCircleBox(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,`
- `inline ShapeCastHit2D shapeCastCircle(const math::vec2& from, const math::vec2& motion, float radius,`

### `SoftShadow2D`
<sub>`engine/include/maz/game/SoftShadow2D.hpp`</sub>

---- Soft (penumbra) 2D shadows via area-light sampling ---------------------------------------- A point light (game::Visibility2D) casts a razor-sharp shadow: every point is either lit or not. A real light has SIZE, so shadow edges are soft — an inner UMBRA that sees none of the light, an outer fully-lit region, and a PENUMBRA between them that sees only part of the light. Godot's Light2D approximates this with a shadow filter. Here we model the light as a small DISC and sample it: cast a hard shadow from each sample point and average. A point that can reach every sample is fully lit; one that reaches none is in umbra; the fraction it can reach IS the soft-shadow value. Pure 2D math (no GPU), so it unit-tests headless; the renderer approximates it by compositing one faint visibility fan per sample additively.

**Functions:**

- `inline bool segmentsIntersect(math::vec2 p1, math::vec2 p2, math::vec2 q1, math::vec2 q2)`
- `inline bool lineBlocked(math::vec2 a, math::vec2 b, const std::vector<Segment2>& occluders)`
- `inline std::vector<math::vec2> diskSamples(math::vec2 center, float radius, int count)`
- `inline float softVisibility(math::vec2 p, math::vec2 lightCenter, float lightRadius,`

### `SpatialGrid`
<sub>`engine/include/maz/game/SpatialGrid.hpp`</sub>

Uniform spatial hash over the X/Z plane for broadphase AABB queries. Static level geometry (walls, blocks) is bucketed once into square cells; a query then tests only the solids sharing the query box's cells instead of the whole world. Y is ignored in bucketing, which suits ground-plane games where colliders are tall relative to the play area; the narrow-phase overlap test is still full 3D.

**Types:** `SpatialGrid`

**Functions:**

- `inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,`

### `StateMachine`
<sub>`engine/include/maz/game/StateMachine.hpp`</sub>

A lightweight finite state machine keyed by an integer/enum state id — the decision layer that sits above steering/pathfinding (guard patrol -> chase -> return), and equally the backbone of game flow (menu/playing/paused) or animation states. Each state has optional onEnter/onUpdate/ onExit callbacks. Transitions are guarded predicates evaluated every update(); "any" transitions fire from whatever state is current. Evaluation is deterministic: on each update the any- transitions are checked first (in registration order), then the current state's transitions, and the first guard that returns true wins. Header-only, no GPU/allocation beyond the callback lists.

**Types:** `StateMachine`

### `Steering`
<sub>`engine/include/maz/game/Steering.hpp`</sub>

Reynolds-style steering behaviors for autonomous agents. Each behavior returns a steering *force* (an acceleration request, already clamped to the agent's maxForce); the caller sums the forces it wants, then calls integrate() to apply them. Forces compose linearly, so seek + separation + path-following just add together. Pure vector math — no GPU/allocation, so it unit-tests headless.  Convention: full 3D vectors, but games that move on the ground can simply keep y fixed (the demo zeroes the y component of every force). Speeds/forces are in world-units/second and /second^2.

**Types:** `Agent`

**Functions:**

- `inline math::vec3 limit(const math::vec3& v, float maxLen)`
- `inline math::vec3 seek(const Agent& a, const math::vec3& target)`
- `inline math::vec3 flee(const Agent& a, const math::vec3& target)`
- `inline math::vec3 arrive(const Agent& a, const math::vec3& target, float slowRadius)`
- `inline math::vec3 separation(const Agent& a, const std::vector<math::vec3>& neighborPositions,`
- `inline void integrate(Agent& a, const math::vec3& force, float dt)`
- `inline math::vec3 followPath(const Agent& a, const std::vector<math::vec3>& waypoints,`

### `TileSet`
<sub>`engine/include/maz/game/TileSet.hpp`</sub>

TileSet — Godot's TileSet resource. A Tilemap holds a grid of tile IDs; a TileSet gives each ID MEANING: which atlas cell to DRAW it with, and what COLLISION it contributes. Godot's per-tile collision can be SUB-CELL (a half-height platform, a shelf), which the Tilemap's single "solid" bit can't express — so a level built from one grid can mix full walls with thin ledges. TileDef carries a None / Full / Box collision plus an atlas source cell; the free queries turn a (Tilemap, TileSet) pair into world-space collision boxes, a point-solidity test, and a drop-to-ground helper. Pure data + geometry — deterministic, unit-testable, no GPU.

**Types:** `TileDef`, `TileSet`, `TileBox`

**Functions:**

- `inline TileBox tileBox(const Tilemap& map, const TileDef& def, int cx, int cy)`
- `inline std::vector<TileBox> collectSolids(const Tilemap& map, const TileSet& set)`
- `inline bool solidAt(const Tilemap& map, const TileSet& set, math::vec2 p)`
- `inline float dropY(const Tilemap& map, const TileSet& set, float x, float fromY, float maxY)`

### `Tilemap`
<sub>`engine/include/maz/game/Tilemap.hpp`</sub>

**Types:** `Tilemap`

### `Visibility2D`
<sub>`engine/include/maz/game/Visibility2D.hpp`</sub>

2D visibility / light occlusion: from a point light, compute the polygon of everything it can see given a set of blocking segments (occluders) inside a bounding rectangle — the geometry behind 2D lights + shadows (Godot's Light2D + LightOccluder2D). The result is a star-shaped polygon around the light: fill it (a triangle fan from the light) to render the lit region; the notches carved out behind occluders ARE the shadows. Uses the classic angle-sweep algorithm — cast a ray toward every occluder endpoint (and just past each side of it) and keep the nearest hit. Header-only, dependency- free (2D math only), so it unit-tests without a GPU.

**Types:** `Segment2`, `Visibility2D`


<a name="anim"></a>
## Anim — skeletons, clips, blending, tweening, curves

### `AdditiveBlend`
<sub>`engine/include/maz/anim/AdditiveBlend.hpp`</sub>

Additive / layered pose blending — Godot's AnimationNodeAdd2 (and the "additive" import flag). The existing blendPoses/blendPosesWeighted CROSS-FADE between whole poses (idle <-> walk): every joint is interpolated, so a walk pose fully replaces an idle pose at weight 1. Additive blending instead layers a *difference* on top of a base: an additive clip is stored relative to a REFERENCE pose, its per-joint DELTA (how far each joint moved from the reference) is computed, and that delta is applied on top of whatever base pose is playing — scaled by a weight. A joint that doesn't move in the additive clip has a zero delta and leaves the base untouched, so you can layer a "wave", "breathe", "aim", or "recoil" motion onto any locomotion without disturbing the unrelated joints. Pure math on JointPose (TRS with a quaternion), header-only, unit-testable without a skeleton or GPU.

**Functions:**

- `inline JointPose makeAdditiveDelta(const JointPose& additive, const JointPose& reference)`
- `inline JointPose applyAdditiveDelta(const JointPose& base, const JointPose& delta, float weight)`
- `inline JointPose additiveBlendJoint(const JointPose& base, const JointPose& additive,`
- `inline void makeAdditivePose(const std::vector<JointPose>& additive,`
- `inline void applyAdditivePose(const std::vector<JointPose>& base, const std::vector<JointPose>& delta,`
- `inline void additiveBlend(const std::vector<JointPose>& base, const std::vector<JointPose>& additive,`

### `AnimClip`
<sub>`engine/include/maz/anim/AnimClip.hpp`</sub>

Keyframed animation clips — the playback layer on top of Skeleton. A clip stores, per joint, three keyframe tracks (translation, rotation, scale); sampling at a time interpolates each track (vec3 lerp, quaternion slerp) into a per-joint local pose, which feeds Skeleton::computeSkinning. blendPoses cross-fades two sampled poses, the basis of animation state blending (idle<->walk). Pure math — no GPU — so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `JointPose`, `Key`, `JointTrack`, `AnimClip`

**Functions:**

- `inline math::vec3 sampleVec3(const std::vector<Key<math::vec3>>& keys, float time,`
- `inline math::quat sampleQuat(const std::vector<Key<math::quat>>& keys, float time,`
- `inline void blendPoses(const std::vector<JointPose>& a, const std::vector<JointPose>& b, float weight,`
- `inline void blendPosesWeighted(const std::vector<const std::vector<JointPose>*>& poses,`
- `inline void posesToLocals(const std::vector<JointPose>& poses, std::vector<math::mat4>& out)`

### `AnimStateMachine`
<sub>`engine/include/maz/anim/AnimStateMachine.hpp`</sub>

---- Animation state machine ------------------------------------------------------------------- Godot's AnimationNodeStateMachine: a graph of named states with CROSS-FADING transitions. Each state here carries an integer payload (a clip index, or a blend-space id — so a state can itself be a blend space, giving "a state machine over blend spaces"). The machine tracks the current state, runs timed cross-fades on transitions, and reports the active state(s) with weights that sum to 1 — the exact same shape as a blend space's weights, so the output feeds straight into anim::blendPosesWeighted and the two compose (state weight x blend-space weight). Transitions fire on an explicit travel(name) or when a per-transition condition() returns true. Pure logic (no GPU/clips), so it unit-tests headless.

**Types:** `AnimStateMachine`

### `Animator`
<sub>`engine/include/maz/anim/Animator.hpp`</sub>

The animation controller — the stateful layer a game actually drives. It holds a library of named clips and plays one at a time; play() starts a timed CROSS-FADE from whatever is currently playing to a new clip, and both clips keep advancing during the fade so the blend is smooth (a run doesn't freeze while it eases into a jump). update() advances time + the fade; pose() returns the blended per-joint result to hand to Skeleton::computeSkinning. Pure logic on top of AnimClip — no GPU, so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `Animator`

### `BlendSpace`
<sub>`engine/include/maz/anim/BlendSpace.hpp`</sub>

Animation blend spaces — Godot's AnimationTree BlendSpace1D / BlendSpace2D. A blend space places animations (referenced here by an integer `id`, e.g. a clip index) at positions in a 1-D or 2-D parameter plane; querying a point returns the small set of animations to mix and the weight of each (summing to 1). Feed those weights + the sampled poses into anim::blendPosesWeighted to get one blended pose. Pure geometry — no GPU, no clips — so it unit-tests headlessly and stays deterministic.

**Types:** `BlendSpace1D`, `BlendSpace2D`

### `BlendTree`
<sub>`engine/include/maz/anim/BlendTree.hpp`</sub>

Animation blend TREE — Godot's AnimationNodeBlendTree. Where a BlendSpace (BlendSpace.hpp) mixes a flat set of animations by one parameter and a state machine (AnimStateMachine.hpp) cross-fades whole states, a blend tree is the graph that NESTS them: a node reads named blend parameters and combines the poses of its child nodes, so you can build "a walk/run blend space, cross-faded into a jump by an air parameter, with an additive upper-body wave layered on top" as one evaluable tree.  A `Pose` is a vector of per-joint local transforms (anim::JointPose). Leaf `Input` nodes pull a pose out of an external table you pass to evaluate() (in a real rig those are sampled clips); the interior nodes blend them. Everything is pure pose math — no GPU, no clip sampling here — so the tree unit-tests headlessly and stays deterministic under the fixed timestep.

**Types:** `BlendTree`

### `Curve`
<sub>`engine/include/maz/anim/Curve.hpp`</sub>

Curve — Godot's Curve resource: a keyframed 1-D function y = f(x), sampled over a domain (usually [0,1]), that drives value-over-time / value-over-parameter effects — particle size or alpha over lifetime, an audio fade, a custom easing shape, a difficulty ramp. This is NOT math::Curve2D (a Bézier *path* through 2D space); this maps one scalar to another. Points carry per-point left/right TANGENTS (slopes) so the Cubic mode is a smooth Hermite spline; Linear and Constant modes ignore tangents. Results are clamped to [minValue, maxValue]. Pure math, header-only, deterministic — it unit-tests exactly and drives a golden.

**Types:** `CurvePoint`, `Curve`

### `Gradient`
<sub>`engine/include/maz/anim/Gradient.hpp`</sub>

Gradient — Godot's Gradient resource: a colour ramp defined by sorted (offset, colour) stops and sampled over a parameter (usually [0,1]). It is the colour analogue of `anim::Curve` (which maps a scalar), and it drives colour-over-lifetime for particles, health/heat tints, sky ramps, minimap legends, and bakes into a GradientTexture. Three interpolation modes match Godot's Gradient.InterpolationMode: Constant (hard bands — the lower stop's colour holds until the next), Linear (straight per-channel blend), and Cubic (a Catmull-Rom spline through the neighbouring stops for smooth, slightly overshooting transitions). Below the first stop it returns the first colour, above the last the last colour (Godot's clamped domain). Pure data + math, header-only, deterministic — it unit-tests exactly and drives a golden.

**Types:** `GradientStop`, `Gradient`

### `IK`
<sub>`engine/include/maz/anim/IK.hpp`</sub>

2-bone inverse kinematics — Godot's SkeletonModification2DTwoBoneIK. Given a fixed root joint, two bone lengths (upper `len1`, lower `len2`), and a target, solve for the middle joint (elbow/knee) and the end effector so the chain reaches the target. Uses the law of cosines: the angle at the root between the root->target line and the upper bone is acos((len1²+d²-len2²)/(2·len1·d)). `bendSign` (+1 / -1) picks which side the elbow bends to. When the target is out of reach the chain points straight at it, fully extended. Pure 2D math — no skeleton, no GPU — so it unit-tests headlessly.

**Types:** `IKResult`

**Functions:**

- `inline IKResult solveTwoBoneIK(math::vec2 root, float len1, float len2, math::vec2 target,`
- `inline void solveFabrik(std::vector<math::vec2>& joints, math::vec2 target, int iterations = 10,`

### `RootMotion`
<sub>`engine/include/maz/anim/RootMotion.hpp`</sub>

Root motion — Godot's AnimationMixer root-motion track. A locomotion clip (walk, run, roll) that actually TRAVELS bakes the character's displacement into a "root" bone; without root motion you play the clip in place and move the character with a separate hand-tuned velocity, and the feet slide whenever the two disagree. Root motion instead READS the travel back out of the clip and hands it to the character each frame, so the body moves exactly as far as the animation says — no foot sliding.  This is the reusable core: a track of the root's cumulative planar POSITION and HEADING over the clip. `delta` returns how far the root moved between two clip times (with one-loop wrap-around), and `advance` applies that step to a world pose — rotating the clip-local displacement by the character's current facing (so "walk forward" goes wherever the character faces) and accumulating the turn. Pure planar math (the XZ-plane + Y-yaw reduction Godot uses for characters), so it unit-tests headless and stays deterministic under the fixed timestep. Heading is stored UNWRAPPED (a cumulative path integral), so a clip may turn any amount and deltas never need angle-wrap fixups.

**Types:** `RootMotionSample`, `RootMotionKey`, `RootMotionTrack`

### `Skeleton`
<sub>`engine/include/maz/anim/Skeleton.hpp`</sub>

Skeletal-animation core: a joint hierarchy plus the matrix math that turns an animated pose into the per-joint "skinning matrices" a mesh is deformed by. This is the engine-agnostic heart of character animation; a renderer can apply the result on the GPU (a joint-matrix UBO + skinned vertex shader) or, as the demo does, skin vertices on the CPU and stream them through the dynamic- mesh path — so it needs no new vertex format and can't regress the existing mesh pipeline.  Convention: joints are stored parents-before-children (topological order), each with a parent index (-1 for a root) and a rest-pose LOCAL transform. From those, global bind transforms and their inverses are precomputed. Header-only, pure math — unit-tests without a GPU.

**Types:** `Joint`, `Skeleton`

### `SpriteAnim`
<sub>`engine/include/maz/anim/SpriteAnim.hpp`</sub>

Sprite-sheet (flipbook) animation: play a sequence of UV sub-rectangles on a single texture over time — walk cycles, explosions, idle bobs, UI spinners. Pairs with SpriteDesc's uvMin/uvMax so a frame is drawn by copying the current SpriteFrame into those fields. Pure logic (frame timing), so it advances deterministically under the fixed timestep and unit-tests without a GPU.

**Types:** `SpriteFrame`, `SpriteAnim`

**Functions:**

- `inline std::vector<SpriteFrame> gridFrames(int cols, int rows, int first, int count)`

### `Timeline`
<sub>`engine/include/maz/anim/Timeline.hpp`</sub>

---- Keyframe timeline / sequencer ------------------------------------------------------------- Godot's AnimationPlayer in miniature: an animation is a set of named TRACKS, each a list of KEYFRAMES (time -> value) that are interpolated between, and a PLAYHEAD that advances over the clip's length with a loop policy. Where a `Tween` (see Tween.hpp) animates ONE value from A to B, a `Timeline` animates MANY named properties through arbitrary keyed poses at once — the backbone of cutscenes, UI transitions, and property animation. Pure math (no GPU/allocation beyond the key vectors), so it unit-tests headless and samples identically every frame under the fixed timestep.

**Types:** `Keyframe`, `Track`, `Timeline`

### `TriggerTrack`
<sub>`engine/include/maz/anim/TriggerTrack.hpp`</sub>

Call-method / trigger tracks — the other half of Godot's AnimationPlayer (M100's Timeline gave VALUE tracks that interpolate a property; this gives METHOD tracks that FIRE at a keyframe time). A trigger track is a list of timed markers; as a playhead sweeps across the clip each marker fires EXACTLY ONCE when the head passes it — the hook a clip uses to play a footstep sound on the plant frame, spawn a muzzle flash on the shoot frame, or open a gate at the end of a cutscene. Pure timing math (no GPU), deterministic under the fixed timestep, so it unit-tests headless.

**Types:** `Trigger`, `TriggerTrack`, `MethodTimeline`

### `Tween`
<sub>`engine/include/maz/anim/Tween.hpp`</sub>

Easing + tweening: the engine's general-purpose "animate a value from A to B over time" toolkit. Cross-cutting on purpose — the same curves drive UI transitions, moving platforms, doors, camera moves, color fades, and gameplay juice. Pure math (no GPU/allocation), so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `Ease`, `Loop`, `Tween`

**Functions:**

- `inline float bounceOut(float t)`
- `inline float ease(Ease type, float t)`
- `inline T mix(const T& a, const T& b, float t)`
- `inline float mix(float a, float b, float t)`

### `TweenPlayer`
<sub>`engine/include/maz/anim/TweenPlayer.hpp`</sub>

Tween sequencer / property animator — Godot's SceneTreeTween (create_tween + tween_property/ tween_interval/tween_callback + parallel + set_loops). anim::Tween is a single time-cursor that interpolates ONE from→to over one duration; this composes many of those into a CHOREOGRAPHY: a list of steps that run one after another, any of which may run in PARALLEL with its neighbours, with delays and callbacks interleaved and the whole thing optionally looping. Each property step is bound to a value (a `void(float)` setter) that it writes every update, so one player animates a dot's x, then its y, while a second grows its radius — all advanced by a single update(dt). Pure logic (no GPU/allocation beyond the step list), so it unit-tests headlessly and runs deterministically under the fixed timestep.  Model (mirrors Godot): the player holds an ordered list of GROUPS; groups run sequentially, and the tweeners inside a group run in parallel. `append*` starts a new group; `parallel*` adds to the current (last) group. A group's duration is its longest tweener; a tweener shorter than its group holds at its end value for the remainder.

**Types:** `Tweener`, `TweenPlayer`


<a name="audio"></a>
## Audio — mixer, DSP effects, spatialization, synthesis

### `Audio`
<sub>`engine/include/maz/audio/Audio.hpp`</sub>

**Types:** `SoundDesc`, `Audio`

### `BusGraph`
<sub>`engine/include/maz/audio/BusGraph.hpp`</sub>

Audio bus graph — Godot's AudioServer bus layout. Every voice in a game plays into a named BUS ("Music", "SFX", "Voice", ...), and each bus carries a volume (in dB), mute / solo / bypass switches, an ordered chain of effects, and a SEND that routes its processed output into another bus. The buses form a forest rooted at "Master" (bus 0), whose output is what reaches the speakers — so you can drop a reverb on a "Reverb" bus and send several buses into it, duck all SFX with one fader, or solo the music while mixing. Maz already had a single linear effect chain (`Bus`); this is the multi-bus router on top of it. Pure per-sample math over the existing Effect chain — no device, no threads — so it unit-tests exactly (a −6 dB bus halves its signal; muting silences everything routed through it; solo keeps only the soloed bus's path to Master) and drives a golden mixer view.  Processing model: push each source sample into its bus's input (`pushInput`), then call `process()` once per output sample. Buses are processed deepest-first so a bus sees all of its child sends before its own effects run; each bus applies its effects (unless bypassed), its dB gain, and its audibility (mute / solo), then adds the result into its send target's input. `process()` returns the Master output and clears the per-sample input accumulators.  Scope note (honest): this is a MONO router — the routing, gain, mute/solo, bypass, and send semantics match Godot, but stereo bus processing (and stereo-aware effects) arrives with the stereo-effects milestone. A per-bus output level meter is exposed for metering/visualization.

**Types:** `BusGraph`

### `Dsp`
<sub>`engine/include/maz/audio/Dsp.hpp`</sub>

---- DSP effects + mix buses ------------------------------------------------------------------- Godot's AudioServer routes every voice through a BUS, and each bus carries an ordered chain of EFFECTS (filter, delay, reverb, ...). Maz's mixer was a flat sum with no per-bus processing. This adds the reusable, testable core: RBJ-cookbook biquad filters, a feedback delay (echo), and a `Bus` that chains effects in series with an output gain. Pure per-sample math — no device, no threads — so it unit-tests exactly and drives a deterministic offline waveform golden, and a real-time mixer can consume it unchanged (process one sample, or a whole buffer, through the chain).

**Types:** `Amplify`, `Biquad`, `Delay`, `Comb`, `Allpass`, `Reverb`, `Distortion`, `MultiDistortion`, `Compressor`, `Limiter`, `Lfo`, `Chorus`, `Flanger`, `Phaser`, `Equalizer`, `Effect`, `BiquadEffect`, `DelayEffect`, `ReverbEffect`, `DistortionEffect`, `DistortionModeEffect`, `CompressorEffect`, `LimiterEffect`, `ChorusEffect`, `FlangerEffect`, `PhaserEffect`, `AmplifyEffect`, `EqualizerEffect`, `Bus`

**Functions:**

- `inline float dbToLinear(float db)`
- `inline float linearToDb(float linear)`
- `inline float fracTap(const std::vector<float>& line, std::size_t head, float delaySamples)`

### `Envelope`
<sub>`engine/include/maz/audio/Envelope.hpp`</sub>

ADSR envelope — the amplitude contour every synth voice is shaped by. When a key goes down (note-on) the level ramps 0 → 1 over ATTACK, falls to the SUSTAIN level over DECAY, then holds there for as long as the key is held; when the key is released (note-off) it ramps from wherever it is down to 0 over RELEASE. Multiplying a raw oscillator by this level turns a flat buzz into a note with a shape — a plucky blip, a slow-swelling pad, a percussive stab. Times are in seconds, the sustain level in [0,1]. It's a tiny gated state machine advanced by process(dt); pure scalar math, deterministic, so it unit-tests headless and a synth callback just multiplies each sample by process(1/sampleRate).

**Types:** `ADSR`

### `MusicSequencer`
<sub>`engine/include/maz/audio/MusicSequencer.hpp`</sub>

Interactive / adaptive music — Godot's AudioStreamInteractive + AudioStreamPlaylist. Game music is not one long file: it is a set of SEGMENTS (intro, explore, combat, boss) that the game switches between as the action changes, and the switch has to happen MUSICALLY — on the next beat or the next bar, with an optional crossfade — or it sounds like a needle scratch. This sequencer is the scheduler that makes that clean: each segment carries a tempo (BPM) and a bar length, one plays, and `transitionTo` queues the next one with a mode (Immediate / AtNextBeat / AtNextBar / Crossfade). It runs on a sample clock and, at any instant, reports which segment(s) are audible and at what gain — so a mixer just multiplies the two candidate streams by those gains. Pure timing + gain math (no decoding), deterministic, so the beat/bar boundaries and equal-power crossfade unit-test to the exact sample and drive a golden timeline.  The scheduler owns only timing and gains; the caller supplies the actual audio for each segment index. A crossfade is equal-power (outGain = cos, inGain = sin over the fade), so the summed loudness stays roughly constant through the transition.

**Types:** `MusicSegment`, `MusicMix`, `MusicSequencer`

### `Oscillator`
<sub>`engine/include/maz/audio/Oscillator.hpp`</sub>

Oscillator / procedural tone generator — Godot's AudioStreamGenerator source material. Where the rest of the audio module PROCESSES incoming sound, this GENERATES it: the raw waveforms a synth voice is built from — sine, sawtooth, square/pulse, triangle, and white noise — at a chosen frequency, plus a detune ratio, phase modulation input (for FM), and a two-operator FM voice. The catch with naive digital saw/square is ALIASING: their sharp edges contain harmonics above the Nyquist limit that fold back as inharmonic "grit". This uses PolyBLEP (polynomial band-limited step) to round those edges so the saw and square stay clean across the musical range — the same anti-aliasing real soft-synths use. Pure per-sample math, deterministic (the noise source is a seeded xorshift), so it unit-tests exactly (a sine hits 0,1,0,-1 at quarter phases; the band-limited saw's edge jump is softened below the naive 2.0 step) and drives a golden waveform gallery.  Scope note (honest): saw and square are PolyBLEP band-limited (the aliasing-prone shapes); sine is exact and triangle is the direct piecewise form (its harmonics roll off as 1/n^2, so its aliasing is minor). A full wavetable-with-mip synthesis path and higher-order BLAMP triangle correction are natural follow-ups.

**Types:** `Oscillator`, `FMVoice`

**Functions:**

- `inline float polyBlep(float t, float dt)`

### `PitchShifter`
<sub>`engine/include/maz/audio/PitchShifter.hpp`</sub>

Pitch shifter — Godot's AudioEffectPitchShift. Raises or lowers the pitch of a signal WITHOUT changing its speed (a monster voice an octave down, a chipmunk an octave up, a pickup jingle nudged up a few semitones). This is the classic time-domain GRANULAR / overlap-add shifter: recent input is kept in a ring buffer and read back through TWO overlapping "grains" whose read pointer moves at the pitch ratio relative to the write pointer; the two grains are crossfaded with a Hann window so that as one grain runs off the end of the buffer it fades out while the other (half a window out of phase) fades in — keeping the output continuous and the buffer from over/under-running. Cheaper and more deterministic than a phase vocoder. Mono float->float, so it drops straight into a Bus/Effect chain (PitchShiftEffect). Deterministic -> it unit-tests (unity ratio passes through delayed; a shifted sine's dominant period scales by the ratio) and drives a golden.  Scope note (honest): a granular shifter trades some quality for simplicity — on very wide shifts or transient-heavy material it has mild warble/smearing (as Godot's does). A phase-vocoder or formant-preserving path is the higher-fidelity follow-up.

**Types:** `PitchShifter`, `PitchShiftEffect`

### `Randomizer`
<sub>`engine/include/maz/audio/Randomizer.hpp`</sub>

Stream randomizer — Godot's AudioStreamRandomizer. Repetitive one-shots (footsteps, gunshots, impacts, UI blips) sound robotic when the exact same clip plays every time. A randomizer wraps a pool of interchangeable streams and, on each trigger, picks one and jitters its pitch and volume so the ear never hears a mechanical repeat. Three pick modes match Godot: Random (weighted uniform), RandomNoRepeat (weighted, but never the clip that just played — Godot's default), and Sequential (round-robin). Pitch is scaled by a log-symmetric factor in [1/randomPitch, randomPitch] and volume offset by ± a dB range. This is the SELECTION + variance logic only (which clip, what pitch/volume) — it returns a RandomPick the caller feeds to the mixer; it does not itself decode or play audio. Deterministic (seeded core::Random), std-only, so it unit-tests exactly and drives a golden histogram/scatter.

**Types:** `RandomPick`, `StreamRandomizer`

### `SampleMixer`
<sub>`engine/include/maz/audio/SampleMixer.hpp`</sub>

Sample-playback mixer — Godot's AudioStreamPlayer over an AudioStreamWAV. Until now Maz could *decode* a .wav into float samples (audio::Wav) and *synthesize* procedural tones (audio::Audio), but there was no way to take a decoded clip and actually PLAY it back through a mixer — start it as a voice, set its gain and stereo pan, loop it, or pitch-shift it, and have several such voices summed into one output buffer. That runtime is this header. It is a self-contained, offline (buffer-in / buffer-out) stereo mixer: it never touches the SDL audio device, so it unit-tests headlessly and byte-deterministically, yet it feeds exactly the interleaved-float format a real device callback wants. The app (or a future device backend) owns the callback and simply asks the mixer to fill each block.  A voice references a WavData clip by pointer (the caller owns the clip and must outlive the voice). Reads are linearly interpolated so pitch/speed and sample-rate conversion are smooth; a mono clip is panned into both output channels, a stereo clip maps its two channels straight through (with pan attenuating the opposite side). Non-looping voices deactivate automatically when they run past the end.

**Types:** `SampleVoice`, `SampleMixer`

**Functions:**

- `inline float sampleAt(const WavData& clip, double pos, std::uint16_t ch)`

### `Spatial2D`
<sub>`engine/include/maz/audio/Spatial2D.hpp`</sub>

2D positional audio — Godot's AudioStreamPlayer2D. Given a listener (position + a "right" axis) and a sound source position, compute the source's per-channel (left/right) gain: a distance ATTENUATION (the source fades with range) times a constant-power PAN (a source off to one side is louder in that ear). Pure math — no device, no mixer — so it unit-tests headlessly and can drive any backend that accepts a left/right gain per voice (the Maz mixer does, via SoundDesc::leftGain/rightGain).

**Types:** `Attenuation`, `Listener2D`, `StereoGain`

**Functions:**

- `inline float attenuation(float distance, float refDistance, float maxDistance, Attenuation mode)`
- `inline StereoGain spatialize(const Listener2D& listener, math::vec2 source, float baseVolume,`

### `Spatial3D`
<sub>`engine/include/maz/audio/Spatial3D.hpp`</sub>

3D spatial audio — Godot's AudioStreamPlayer3D. Where Spatial2D gives distance attenuation + a left/ right pan on a plane, a 3D source heard by a 3D LISTENER needs three things: (1) distance ATTENUATION with a choice of falloff curve, (2) a stereo PAN derived from where the source sits relative to the listener's ORIENTATION (its forward/up basis), and (3) DOPPLER — the pitch shift when source and listener move relative to each other. All pure math — no device, no mixer — so it unit-tests headlessly and drives any backend that takes a per-voice left/right gain + pitch.

**Types:** `Attenuation3D`, `Listener3D`, `Source3D`, `SpatialMix`, `SpatialConfig`

**Functions:**

- `inline float attenuation3D(float distance, float refDistance, float maxDistance, float rolloff,`
- `inline float panPosition(const Listener3D& l, math::vec3 sourcePos)`
- `inline void equalPowerPan(float pan, float gain, float& left, float& right)`
- `inline float dopplerPitch(const Listener3D& l, const Source3D& s, float speedOfSound = 343.0f)`
- `inline SpatialMix computeSpatialMix(const Listener3D& l, const Source3D& s, const SpatialConfig& cfg)`

### `Spectrum`
<sub>`engine/include/maz/audio/Spectrum.hpp`</sub>

SpectrumAnalyzer — Godot's AudioEffectSpectrumAnalyzer: turn a block of audio samples into a frequency spectrum so a game can react to sound (rhythm games, VU meters / equalizer visualizers, beat-reactive lights and particles, lip-sync). It runs an in-place radix-2 FFT over a windowed, zero-padded frame and exposes the per-bin magnitudes plus `magnitudeForRange(lowHz, highHz)` — the same query Godot's analyzer gives (`get_magnitude_for_frequency_range`) — for band energy (bass / mid / treble meters). Pure DSP maths, header-only, deterministic — it unit-tests exactly (a pure tone peaks on its bin) and drives a golden (a spectrum bar graph).

**Types:** `SpectrumAnalyzer`

**Functions:**

- `inline void fft(std::vector<Cplx>& a, bool inverse)`
- `inline std::size_t nextPow2(std::size_t n)`

### `Stereo`
<sub>`engine/include/maz/audio/Stereo.hpp`</sub>

Stereo processors — Godot's AudioEffectStereoEnhance and AudioEffectPanner. Maz's DSP so far is mono (one float in, one float out); this adds the small stereo-aware layer that games use to place and widen a sound across the two speakers. A StereoFrame is one interleaved L/R sample pair. Two processors operate on it: StereoEnhance controls the perceived WIDTH of the stereo image (from mono at the center out to a wide, enveloping field) via mid/side scaling plus an optional Haas time-offset; and Panner shifts the stereo BALANCE left or right with a constant-power law. Both are pure per-frame math — no device, no threads — so they unit-test exactly (width 0 collapses to mono; a centered balance is unchanged; a hard pan silences one side) and drive a golden goniometer (vectorscope) view.  Mid/side: mid = (L+R)/2 is the mono-compatible center, side = (L-R)/2 is the stereo difference. Scaling `side` by a width factor narrows (<1, toward mono) or widens (>1) the image; a truly mono input (L==R) has zero side, so width cannot invent width that isn't there — it stays mono, which is correct.  Scope note (honest): these are standalone stereo processors + a StereoFrame type. Rewiring the whole BusGraph/effect chain to carry stereo end-to-end (so every effect is stereo-aware) is the larger follow-up; the mono effect path is unchanged.

**Types:** `StereoFrame`, `StereoEnhance`, `Panner`

**Functions:**

- `inline float stereoMid(const StereoFrame& f)`
- `inline float stereoSide(const StereoFrame& f)`
- `inline StereoFrame fromMidSide(float m, float s)`

### `Wav`
<sub>`engine/include/maz/audio/Wav.hpp`</sub>

WAV load/save — Godot's AudioStreamWAV / the "load a .wav" half of the asset pipeline. Every prior Maz sound was PROCEDURALLY synthesized (audio::SoundDesc); there was no way to read an actual audio file, or to export one. This is a self-contained RIFF/WAVE PCM codec: `decodeWav` parses the exact bytes of a .wav file into float samples, and `encodeWav` writes them back out. It handles the two ubiquitous PCM formats — 8-bit unsigned and 16-bit signed, mono or interleaved multi-channel — which covers the vast majority of game sound assets. Byte-in / byte-out (no file device), so it unit-tests headlessly and the app owns any real disk read/write.

**Types:** `WavData`

**Functions:**

- `inline std::uint16_t rd16(const std::uint8_t* p)`
- `inline std::uint32_t rd32(const std::uint8_t* p)`
- `inline void wr16(std::vector<std::uint8_t>& b, std::uint16_t v)`
- `inline void wr32(std::vector<std::uint8_t>& b, std::uint32_t v)`
- `inline void wrTag(std::vector<std::uint8_t>& b, const char* t)`
- `inline bool tagEq(const std::uint8_t* p, const char* t)`
- `inline bool decodeWav(const std::uint8_t* data, std::size_t size, WavData& out)`
- `inline bool decodeWav(const std::vector<std::uint8_t>& bytes, WavData& out)`
- `inline std::vector<std::uint8_t> encodeWav(const WavData& wav)`


<a name="ui"></a>
## UI — controls, layout, theming, text

### `Container`
<sub>`engine/include/maz/ui/Container.hpp`</sub>

Auto-layout containers — Godot's Container controls (BoxContainer / GridContainer / MarginContainer / CenterContainer). M86's LayoutNode already gives an anchor tree with a *simple* box mode where every `expand` child grabs an equal slice of the leftover space and the cross axis always fills. Godot's real container model is richer, and that richness is what you actually need to build a resizable UI:  * per-axis SIZE FLAGS — a child independently chooses, for its horizontal and its vertical axis, whether to Fill the cell, Expand (grab leftover main-axis space), or Shrink to its minimum and sit at the Begin / Center / End of the cell; * STRETCH RATIOS — two expanding children with ratios 1 and 3 split the leftover 1:3, not 50/50; * a real GRID — N columns, column widths driven by the widest cell in each column, expanding columns sharing the leftover, so a form of label/field pairs lines up; * BOTTOM-UP minimum size — a container reports the min size it needs from its children, so nested containers (a VBox of HBoxes) size correctly.  All pure rectangle math (no renderer, no Font) operating on ui::Rect, so it unit-tests headlessly and the results are deterministic. The app owns the Control structs; each layout call writes their `rect`.

**Types:** `SizeFlag`, `Control`, `Span`

**Functions:**

- `inline Span placeCross(float origin, float extent, float minSize, SizeFlag flag)`
- `inline std::vector<Span> distributeMain(const std::vector<float>& mins,`
- `inline void hbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f)`
- `inline void vbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f)`
- `inline void grid(const Rect& area, const std::vector<Control*>& kids, int columns, float hsep = 0.0f,`
- `inline void margin(const Rect& area, Control& child, float left, float top, float right, float bottom)`
- `inline void center(const Rect& area, Control& child)`
- `inline void hboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH)`
- `inline void vboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH)`
- `inline void gridMinSize(const std::vector<Control*>& kids, int columns, float hsep, float vsep,`

### `DebugOverlay`
<sub>`engine/include/maz/ui/DebugOverlay.hpp`</sub>

A small profiling overlay: smoothed FPS + frame time and the renderer's per-frame draw counts, drawn with a Font. Off by default; toggle it (e.g. on F3). Engine-agnostic — the app owns the Font and decides where to place it.

**Types:** `DebugOverlay`

### `Font`
<sub>`engine/include/maz/ui/Font.hpp`</sub>

Bitmap font baked from a TTF via stb_truetype into a single atlas texture. Text is drawn as tinted glyph sprites through the Renderer's 2D API, so it works on any renderer backend and respects the active camera (use a pixel-space Camera2D for a screen-fixed HUD).

**Types:** `Font`

### `ItemList`
<sub>`engine/include/maz/ui/ItemList.hpp`</sub>

ItemList — Godot's ItemList control: a scrollable column of selectable text rows. It backs Godot's FileDialog file list, the animation/audio-bus pickers, inventory and dialogue lists, level-select menus — anywhere a game shows a bounded box of choosable entries. Each row carries text, a caller id, and `selectable`/`disabled` flags. Selection is either Single (picking one clears the rest, like a radio group) or Multi (rows toggle independently). The list has a fixed row height + separation and a vertical `scroll` offset, so it exposes the geometry a renderer needs: itemRect(i) for a row's pixel box, itemAtPoint() to hit-test a click, ensureVisible() / visibleRange() for scrolling. It is pure logic — no GPU, no windowing — so it unit-tests deterministically and a view just draws the rows it reports.

**Types:** `ListItem`, `ItemList`

### `Layout`
<sub>`engine/include/maz/ui/Layout.hpp`</sub>

Retained UI layout — anchors + containers, modeled on Godot's Control system. Maz already had immediate-mode widgets, but every position was a hand-typed pixel coordinate that broke at a different resolution. This adds the missing piece: a layout tree that computes screen rects responsively.  Two ways a node is placed: * Anchors + offsets (Godot's model): anchorMin/anchorMax are fractions [0..1] of the PARENT rect for each edge; L/T/R/B offsets are pixel margins from those anchored points. So (0,0,1,1) with zero offsets fills the parent; (0,0,1,0)+offsets makes a top bar of fixed height that stretches to any width; (0.5,0.5,0.5,0.5) pins a fixed-size box to the center. * Container modes (HBox / VBox / Center) that arrange children automatically: fixed-size children keep their min size, `expand` children share the leftover space, `spacing` sits between them and `pad` insets the container. This is how you build responsive toolbars, lists, and dialogs.  layout() walks the tree from a root rect (usually the framebuffer) and fills every node's `rect`. Non-owning children pointers — the app owns the nodes. Header-only, math-only (no renderer dep).

**Types:** `LayoutNode`

### `PopupMenu`
<sub>`engine/include/maz/ui/PopupMenu.hpp`</sub>

PopupMenu — Godot's PopupMenu: the vertical list of items behind right-click context menus, OptionButton dropdowns, and menu bars. Each item is a label with a caller id and optional check state (a checkbox or a radio button), a disabled flag, an accelerator/shortcut hint, or a submenu arrow; a `separator` item draws a thin divider and is never selectable. The menu stacks items from its `position` at a fixed row height, so it exposes the geometry a view + input need: `rect()` / `itemRect(i)` for drawing, `itemAtPoint()` to hit-test the cursor (rejecting separators and outside points), `hoverNext`/`hoverPrev` for keyboard navigation (skipping separators + disabled rows), and `activate()` to fire the hovered item — toggling a checkbox, switching a radio group, and returning the item id. Pure logic, header-only, deterministic — it unit-tests exactly and drives a golden (an open menu).

**Types:** `MenuItem`, `PopupMenu`

### `Range`
<sub>`engine/include/maz/ui/Range.hpp`</sub>

Range — Godot's Range, the shared value model behind ProgressBar, HSlider/VSlider, ScrollBar, and SpinBox. It holds a scalar `value` clamped to [min, max], optionally snapped to a `step`, and exposes it as a normalized `ratio` in [0,1] — the single number a bar or slider draws from. `page` supports scrollbar-style ranges where a visible window of size `page` means the value can only reach `max-page` (so ratio still spans 0..1). `allowGreater`/`allowLesser` lift the clamp when a field may legitimately exceed its nominal bounds. Pure logic, header-only, deterministic — it unit-tests exactly.

**Types:** `Range`, `ProgressBar`

### `Rect`
<sub>`engine/include/maz/ui/Rect.hpp`</sub>

Screen-space rectangle (pixel coordinates, origin top-left). Shared by the immediate-mode UI (hit-testing) and the retained layout system (computed node rects).

**Types:** `Rect`

### `RichText`
<sub>`engine/include/maz/ui/RichText.hpp`</sub>

BBCode rich-text parser — Godot's RichTextLabel markup. Maz can draw a plain string (ui::Font) and wrap it (ui::layoutText, M134), but there was no way to mix styles WITHIN a string: bold a word, colour a phrase, enlarge a heading. Godot does this with BBCode — `[b]bold[/b]`, `[i]/[u]`, `[color=#ff0000]red[/color]`, `[size=32]big[/size]` — parsed into styled runs a label then lays out. This is that parser: `parseBBCode` turns a tagged string into a flat list of `RichSpan`s (each a substring + its resolved bold/italic/ underline/colour/size), and `stripBBCode` returns the tags-removed plain text. It is deliberately renderer-independent (no Font/Color dependency) so it unit-tests headlessly; the app maps each span's attributes onto its own font draw. Tag handling is lenient like Godot: nested tags stack, an unclosed tag runs to the end, a stray close tag is ignored, `[lb]`/`[rb]` emit literal brackets, and an unrecognized tag is passed through as literal text rather than dropped.

**Types:** `RichSpan`

**Functions:**

- `inline bool parseRichColor(const std::string& v, float& r, float& g, float& b, float& a)`
- `inline std::vector<RichSpan> parseBBCode(const std::string& src)`
- `inline std::string stripBBCode(const std::string& src)`

### `StyleBox`
<sub>`engine/include/maz/ui/StyleBox.hpp`</sub>

---- Nine-patch / StyleBox -------------------------------------------------------------------- Godot draws every themed Panel/Button through a StyleBox — most powerfully a NINE-PATCH: a source image sliced into a 3x3 grid by border insets. When the box is drawn at an arbitrary size the four CORNERS keep their exact size, the four EDGES stretch along one axis, and the CENTER stretches both ways — so a bordered/rounded panel scales to any rectangle without distorting its corner art. This module is the pure mapping (source region -> destination region) behind that; it's dependency-free geometry (no GPU), so it unit-tests headless and a renderer just blits the 9 quads it returns.

**Types:** `Border`, `Patch9`, `Patch`

**Functions:**

- `inline std::array<Patch, 9> ninePatch(const Rect& dst, const Border& border, const Rect& src)`

### `TextInput`
<sub>`engine/include/maz/ui/TextInput.hpp`</sub>

Single-line editable text — the model behind Godot's LineEdit. Pure logic: no rendering, no input polling, so it unit-tests headlessly. A UI widget draws text() and a caret at position caret(); typed characters come in via insert() and the editing keys drive the caret/erase ops. ASCII/byte caret (one byte == one column), which is what the bundled font renders.

**Types:** `TextField`, `FocusChain`, `TextEditInput`

### `TextLayout`
<sub>`engine/include/maz/ui/TextLayout.hpp`</sub>

Text layout — word-wrapping + alignment for multi-line paragraphs (Godot's Label autowrap + align). The Font renderer can draw a single line and measure its width, but it has no notion of FITTING text into a box: breaking a paragraph across lines at word boundaries so it doesn't overflow, and aligning each line left / center / right within the box. That's what every dialog box, tooltip, description pane, and subtitle needs. This is a pure algorithm — it takes a MEASURE callback (so it has no renderer/Font dependency and unit-tests headlessly) and returns positioned lines the caller then draws with one Font::drawText per line.

**Types:** `TextLine`, `TextLayout`

**Functions:**

- `inline void wrapParagraph(std::string_view para, float maxWidth,`
- `inline TextLayout layoutText(std::string_view text, float maxWidth,`

### `Theme`
<sub>`engine/include/maz/ui/Theme.hpp`</sub>

StyleBoxFlat + Theme — the other half of Godot's theming (M103 gave the nine-patch StyleBoxTexture). Godot draws almost every default control through a StyleBoxFlat: a solid, ROUNDED-corner rectangle with an optional border and a soft drop shadow, generated procedurally — no texture asset. A Theme then names those styles per control class + state ("Button/normal", "Button/hover", …) so a whole UI restyles from one place. This module is that: dependency-light rounded-rect geometry (unit-testable), a StyleBoxFlat value type + a draw helper that layers shadow → border → fill through the 2D renderer, and a small Theme registry with a default fallback.

**Types:** `Corners`, `StyleBoxFlat`, `Theme`

**Functions:**

- `inline std::vector<render::Point2> roundedRectPolygon(const Rect& box, Corners c, int seg = 6)`
- `inline void drawStyleBoxFlat(render::Renderer& r, const Rect& box, const StyleBoxFlat& s, int seg = 6)`

### `Tree`
<sub>`engine/include/maz/ui/Tree.hpp`</sub>

Tree / TreeItem — Godot's hierarchical list control, the backbone of its scene dock, inspector, and FileSystem dock. A TreeItem carries text, an optional id + colour, a `collapsed` flag, and child items; the Tree FLATTENS the currently-expanded items into an ordered list of visible ROWS, each tagged with its depth (for indentation) and whether it has children (so the view draws a fold arrow). Folding a branch hides its whole subtree in one flag. It's a pure data structure + depth-first traversal — deterministic and GPU-free — so it unit-tests headless and a renderer just draws the rows it returns.

**Types:** `TreeItem`, `TreeRow`, `Tree`

### `UI`
<sub>`engine/include/maz/ui/UI.hpp`</sub>

Map a horizontal pointer position within `track` to a value in [minV, maxV], clamped to the ends.

**Types:** `Context`

**Functions:**

- `inline float sliderValueFromX(const Rect& track, float pointerX, float minV, float maxV)`


<a name="fx"></a>
## FX — particles and force fields

### `ForceField2D`
<sub>`engine/include/maz/fx/ForceField2D.hpp`</sub>

Composable 2D force field for particles & gameplay — Godot's GPUParticlesAttractor2D family plus a wind zone and drag, generalized into one reusable resource. The existing fx::ParticleSystem carries a single hard-wired attractor; this is a *set* of attractors/repulsors (each with a position, a signed strength, an influence radius, a falloff curve, and an optional tangential SWIRL for vortices), on top of a uniform directional WIND and a global linear DRAG. It exposes the pure force query `accelAt(pos, vel)` and a deterministic semi-implicit-Euler `step()` integrator over a particle array, so it drives gravity wells, black holes, wind tunnels, and orbiting swarms — and, being pure math with no GPU/RNG, it unit-tests exactly and renders a golden-stable swirl.  Honest scope: this is a CPU point/vector force model. It does NOT implement Godot's texture-baked vector-field attractors, 3D attractors, or the full turbulence-noise process; those remain follow-ups.

**Types:** `Attractor2D`, `FieldParticle`, `ForceField2D`

### `ParticleEmitter`
<sub>`engine/include/maz/fx/ParticleEmitter.hpp`</sub>

Particle emitter RESOURCE — Godot's CPUParticles2D. The existing fx::ParticleSystem is a runtime pool that emits point bursts with a linear start->end colour/size; a real emitter is a *resource* you author once and reuse, with (1) an EMISSION SHAPE (point / disk / ring / rectangle), (2) per-lifetime CURVES for scale and alpha (not just two endpoints), and (3) a multi-stop colour GRADIENT over lifetime. This header is that resource plus a DETERMINISTIC simulator: given a seed and a query time it returns every live particle's drawable state, so it unit-tests headlessly and renders a golden-stable snapshot. Pure math — no GPU, no global state — so it composes with any renderer.

**Types:** `Curve`, `Gradient`, `EmitShape`, `Emitter`, `ParticleState`

**Functions:**

- `inline render::Color lerpColor(const render::Color& a, const render::Color& b, float t)`
- `inline math::vec2 sampleOffset(const EmitShape& s, float u, float v)`
- `inline float hash01(uint32_t seed, int index, int channel)`
- `inline std::vector<ParticleState> simulate(const Emitter& e, uint32_t seed, float t)`

### `Particles`
<sub>`engine/include/maz/fx/Particles.hpp`</sub>

Parameters for one burst of particles emitted from a point.

**Types:** `BurstDesc`, `ParticleSystem`


<a name="io"></a>
## IO — JSON, config, serialization, resource packs

### `Base64`
<sub>`engine/include/maz/io/Base64.hpp`</sub>

Base64 — Godot's Marshalls raw_to_base64 / base64_to_raw. The standard way to carry BINARY data through TEXT channels: embed a texture, a save blob, or any byte buffer inside a JSON string, a .tres/.tscn resource, a URL, or a config value. Encoding maps every 3 bytes to 4 ASCII characters (A–Z a–z 0–9 + /) with '=' padding; decoding reverses it, tolerating embedded whitespace/newlines (so wrapped blobs decode) and rejecting stray non-alphabet characters. Standard RFC 4648 alphabet. Header-only, deterministic — it unit-tests exactly against the canonical vectors and drives a golden text readout.

**Functions:**

- `inline std::string base64Encode(const std::uint8_t* data, std::size_t n)`
- `inline std::string base64Encode(const std::vector<std::uint8_t>& data)`
- `inline std::string base64Encode(const std::string& text)`
- `inline int base64Value(char c)`
- `inline bool base64Decode(const std::string& text, std::vector<std::uint8_t>& out)`
- `inline std::vector<std::uint8_t> base64Decode(const std::string& text)`

### `Config`
<sub>`engine/include/maz/io/Config.hpp`</sub>

The bridge between the config registry (core::CVarRegistry, dependency-free) and JSON. This lives in the io layer so core stays zero-dependency while apps still get "config.json drives the engine": loadConfig applies a parsed JSON object's fields onto matching cvars (coercing JSON types into each cvar's declared type, so a bool cvar ignores a stray number), and configToJson serializes the whole registry back out for a settings file. Unknown keys are ignored, so a config file may target a subset (or a superset) of the registered cvars without error.

**Functions:**

- `inline int loadConfig(core::CVarRegistry& reg, const JsonValue& obj)`
- `inline JsonValue configToJson(const core::CVarRegistry& reg)`
- `inline int loadConfigFile(core::CVarRegistry& reg, const std::string& path)`
- `inline bool saveConfigFile(const core::CVarRegistry& reg, const std::string& path)`

### `ConfigFile`
<sub>`engine/include/maz/io/ConfigFile.hpp`</sub>

ConfigFile — Godot's ConfigFile: an INI-style `[section]` + `key=value` store, the format behind project settings, input maps, and hand-editable save/options files. Values are held as raw strings with typed accessors (getBool/getInt/getFloat coerce; setBool/setInt/setFloat format), which keeps it dependency-free while covering the overwhelmingly common settings-file use. Sections and keys preserve INSERTION ORDER so `encode()` produces stable, diff-friendly text that round-trips through `parse()`. Keys written before any `[section]` header live in the unnamed global section (Godot allows this). Parsing is lenient: blank lines and `;` / `#` comments are skipped, whitespace around keys/values is trimmed, and a value wrapped in matching quotes has them stripped. Header-only, deterministic — it unit-tests exactly and drives a golden (a rendered settings table + its encoded text).

**Types:** `ConfigFile`

### `Json`
<sub>`engine/include/maz/io/Json.hpp`</sub>

JSON value + parser + serializer: the engine's human-readable data format, alongside the binary Serialize backbone. Where ByteWriter/Reader is for fast, compact save games, this is for the files a person (or a tool) edits: configs, tuning tables, and data-driven scenes/levels. A JsonValue is a tagged union over the six JSON types (null / bool / number / string / array / object); objects keep insertion order so a round-trip is stable and diff-friendly. parse() is a hand-written recursive descent scanner that is bounds-checked and never throws — malformed input yields a null value plus a human-readable error (line/column), so loading untrusted data fails cleanly. dump() re-serializes, optionally pretty-printed. Header-only, zero dependencies beyond the standard library.

**Types:** `JsonValue`, `JsonParseResult`, `JsonParser`

**Functions:**

- `inline JsonValue* JsonValue::Object::find(const std::string& key)`
- `inline const JsonValue* JsonValue::Object::find(const std::string& key) const`
- `inline void JsonValue::writeEscaped(std::string& out, const std::string& s)`
- `inline void JsonValue::writeTo(std::string& out, int indent, int depth) const`
- `inline JsonParseResult parseJson(const std::string& text)`
- `inline bool readTextFile(const std::string& path, std::string& out)`
- `inline bool writeTextFile(const std::string& path, const std::string& text)`
- `inline JsonParseResult parseJsonFile(const std::string& path)`
- `inline bool writeJsonFile(const std::string& path, const JsonValue& value, int indent = 2)`

### `Localization`
<sub>`engine/include/maz/io/Localization.hpp`</sub>

CSV parsing + localization — Godot's Translation / CSV import. A shippable game needs its on-screen text in more than one language, and the standard authoring format (Godot's included) is a CSV whose first column is a message KEY and whose remaining columns are one LOCALE each. Maz could read JSON and its own prefab/binary formats but had no CSV reader and no translation lookup at all. This adds a robust RFC-4180-style CSV parser (quoted fields, embedded delimiters/newlines, "" escapes, CRLF or LF) plus a `TranslationTable` that loads such a CSV and answers tr(key) in the active locale with sensible fallback. Pure std, header-only, no engine deps.

**Types:** `TranslationTable`

**Functions:**

- `inline std::vector<std::vector<std::string>> parseCsv(std::string_view text, char delim = ',')`

### `PrefabText`
<sub>`engine/include/maz/io/PrefabText.hpp`</sub>

Text resource save/load for prefabs — Godot's .tscn / .tres text format. M125 gave prefabs an in-memory template + instancing; this makes them a DISK RESOURCE you can read, diff, and version-control as plain text (the whole reason Godot's scene files are text). A prefab tree serializes to a sequence of `[node name="…" parent="…"]` sections, each followed by typed `key = TYPE values` property lines, and parses straight back into an identical tree. Deterministic + round-trip-stable; pure string work, no I/O device, so it unit-tests headlessly (the app owns any actual file read/write).

**Functions:**

- `inline std::string fmtF(float v)`
- `inline std::string propToText(const scene::PropValue& p)`
- `inline std::vector<std::string> tokens(const std::string& s)`
- `inline bool textToProp(const std::string& s, scene::PropValue& out)`
- `inline void writeNode(std::string& out, const scene::PrefabNode& node, const std::string& parentPath)`
- `inline std::string attr(const std::string& line, const std::string& key)`
- `inline std::string savePrefabText(const scene::Prefab& prefab)`
- `inline bool loadPrefabText(const std::string& text, scene::Prefab& out)`

### `ResourcePack`
<sub>`engine/include/maz/io/ResourcePack.hpp`</sub>

Resource pack archive — Godot's PackedData / the .pck file its shipping games load every asset from. Maz could already serialize a single blob (io::Serialize) and read/write one file at a time, but there was no way to bundle MANY named resources — textures, level JSON, sound clips, prefab text — into ONE archive and pull them back out by path. That is what a game ships: one .pck instead of a loose tree of files. This is a self-contained byte-in / byte-out container built on the existing ByteWriter/ByteReader: `packResources` writes a magic+version header, a directory of (path, offset, size) records, then the concatenated blob data; `ResourcePack::load` parses that back and hands out each blob by path with full bounds checking, so a truncated or foreign archive fails cleanly instead of reading out of range. The app owns any real disk read/write (via io::writeFile / io::readFile) — this just does the packing.

**Types:** `PackEntry`, `ResourcePack`

**Functions:**

- `inline std::vector<std::uint8_t> packResources(const std::vector<PackEntry>& entries)`

### `SceneSerializer`
<sub>`engine/include/maz/io/SceneSerializer.hpp`</sub>

Reflection-lite ECS scene serialization: save and load a live ecs::World as JSON. The ECS stores arbitrary component types in type-erased pools, so — without a full reflection system — the app tells the serializer, once, how each component maps to/from JSON:  io::SceneSerializer s; s.component<Transform>("Transform", [](const Transform& t){ io::JsonValue j; j.set("x", t.x); j.set("y", t.y); return j; }, [](const io::JsonValue& j){ return Transform{ j["x"].asFloat(), j["y"].asFloat() }; });  saveWorld then emits { "entities": [ { "id": N, "components": { "Transform": {..}, .. } }, .. ] } with entities in ascending-id order (deterministic), and loadWorld rebuilds the world from that. This is the standard content-pipeline backbone for save games, prefabs, and an editor. It lives in the io layer so ecs stays dependency-free. Header-only.

**Types:** `SceneSerializer`

### `Serialize`
<sub>`engine/include/maz/io/Serialize.hpp`</sub>

Binary serialization: the engine's backbone for save games, level files, and any structured data that must round-trip to disk (or across a wire later). ByteWriter appends; ByteReader consumes with bounds checking so truncated/corrupt input fails cleanly (ok() == false) instead of reading out of range. Layout is the host byte order — fine for the LE desktop targets the engine builds for; a byte-swap layer can slot in later if a BE target ever matters. Header-only.

**Types:** `ByteWriter`, `ByteReader`

**Functions:**

- `inline bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes)`
- `inline bool readFile(const std::string& path, std::vector<uint8_t>& out)`

### `Xml`
<sub>`engine/include/maz/io/Xml.hpp`</sub>

Pull-style XML reader — Godot's XMLParser: a forward, streaming tokenizer that hands back one node at a time (`read()` advances; the accessors describe the node just read) instead of building a DOM tree in memory. That is the shape you want for reading big or foreign documents — a Tiled `.tmx` tilemap, an SVG path set, a COLLADA model, an RSS feed, an app config in XML — where you walk the stream and pull out the handful of elements you care about. Maz had text formats (JSON, CSV, .tres/PrefabText) and binary (Serialize, ResourcePack) but no XML at all, so any XML-shaped asset was unreadable.  The parser recognises elements (`<a x="1">`), self-closing elements (`<br/>`), end tags (`</a>`), text runs, comments (`<!-- … -->`), CDATA (`<![CDATA[ … ]]>`), and processing / declaration nodes (`<?xml … ?>`). Attribute values and text runs are entity-decoded (`&lt; &gt; &amp; &quot; &apos;` and numeric `&#NN;` / `&#xHH;`, UTF-8 encoded). `depth()` reports the count of currently-open ancestor elements so a caller can indent or scope without tracking a stack by hand. Header-only, no allocation beyond the node's own strings, deterministic — it unit-tests exactly and drives a golden.  Scope note (honest): this is a well-formed-input pull parser, not a validator. It does not check tag nesting/matching, resolve namespaces, expand DTD/custom entities, or enforce a schema; malformed input (an unterminated tag or quote) sets the error flag and stops rather than recovering. Those remain follow-ups; a full validating/DOM parser is out of scope for a header-only module.

**Types:** `XmlParser`


<a name="input"></a>
## Input — action maps, analog helpers

### `ActionMap`
<sub>`engine/include/maz/input/ActionMap.hpp`</sub>

Action mapping: the layer that turns raw device state into named gameplay intents. Gameplay code asks "is Jump pressed?" or "what's MoveX?" instead of "is Space down / is pad A down / is the left stick past the deadzone?" — so one action can bind several physical sources (keyboard OR gamepad), bindings can be rebound at runtime, and the game logic never mentions a scancode.  Two action kinds: * Button actions are down if ANY bound source is down; each frame yields held / pressed (edge down this frame) / released (edge up this frame). * Axis actions combine negative/positive button pairs (each contributing -1 / +1) with any analog pad axes (scaled), clamped to [-1, 1] — so WASD and a thumbstick drive the same MoveX.  The map is deliberately SDL-free: update() takes sampler callbacks (down(device, code) and analog(axis)), so it is unit-testable with synthetic input and works over any backend. An app wires the samplers to platform::Input. Header-only, std only.

**Types:** `ActionMap`

### `Analog`
<sub>`engine/include/maz/input/Analog.hpp`</sub>

Analog-stick conditioning — Godot's `Input.get_vector` / `get_axis` deadzone maths.  A raw thumbstick reports a 2D vector whose length can drift up to ~√2 at the diagonals and jitters around zero at rest. Feeding that straight into movement gives two classic bugs: the character creeps while the stick is "centred" (drift inside the deadzone), and moves ~40% faster on the diagonals than the cardinals (the square input region is bigger than the unit circle). Godot fixes both in `get_vector`: a **radial** deadzone (the whole vector's magnitude, not each axis) zeroes rest jitter, the remaining magnitude is **rescaled** so the deadzone edge maps to 0 and 1 maps to 1 (no sudden jump as you leave the deadzone), and the magnitude is **clamped to the unit circle** so diagonals aren't faster. These are pure, stateless functions over raw values — no backend, no allocation — so they unit-test exactly and pair with any input source (the `platform::Input` gamepad axes, `ActionMap` axis actions, or a synthetic test vector).

**Functions:**

- `inline float sanitizeDeadzone(float deadzone)`
- `inline float applyDeadzone(float value, float deadzone)`
- `inline math::vec2 analogVector(math::vec2 raw, float deadzone = 0.2f)`


<a name="editor"></a>
## Editor — scene model, gizmos, inspector

### `Scene`
<sub>`engine/include/maz/editor/Scene.hpp`</sub>

A minimal editable scene model for the in-engine editor — the data an inspector edits and a scene tree lists, kept renderer-agnostic so it is pure logic and unit-testable without a GPU. Each Node owns a transform (position / Euler degrees / scale), a local-space AABB (for click picking), a mesh id (an index the app maps to a real MeshHandle), and material parameters. The editor app turns these into draw calls; the engine keeps only the model.

**Types:** `Node`, `Scene`, `History`

**Functions:**

- `inline int pickNode(const Scene& scene, const math::vec3& origin, const math::vec3& dir)`
- `inline float snap1(float v, float step)`
- `inline math::vec3 snapToGrid(const math::vec3& v, float step)`
- `inline bool rayPlaneY(const math::vec3& origin, const math::vec3& dir, float planeY,`
- `inline void screenRay(const math::mat4& invViewProj, float px, float py, float w, float h,`
- `inline bool worldToScreen(const math::mat4& viewProj, const math::vec3& world, float w, float h,`
- `inline io::JsonValue toJson(const Scene& s)`
- `inline bool fromJson(const io::JsonValue& root, Scene& out)`


<a name="root"></a>
## (root)

### `Engine`
<sub>`engine/include/maz/Engine.hpp`</sub>


