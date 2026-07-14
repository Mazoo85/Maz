if(NOT MAZ_CORE_ONLY)
    # Smoke test: run the sandbox headless for a few frames and require a clean exit(0).
    # This validates module wiring, resource lifetime, and shutdown without needing a GPU/display.
    add_test(
        NAME sandbox_headless_smoke
        COMMAND sandbox --headless --frames 30)

    set_tests_properties(sandbox_headless_smoke PROPERTIES
        TIMEOUT 60
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    # Blender pipeline: load the test cube (glTF) and require both a clean exit and the
    # expected geometry. Exercises the whole asset path with no GPU/display.
    add_test(
        NAME model_load_gltf
        COMMAND sandbox --headless --frames 1
                --load-model ${CMAKE_SOURCE_DIR}/assets/models/cube.gltf)

    set_tests_properties(model_load_gltf PROPERTIES
        TIMEOUT 60
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        PASS_REGULAR_EXPRESSION "24 verts, 12 tris")
endif()

# Unit tests: pure C++ (no GPU/display). Camera view/projection math.
add_executable(maz_unit_tests unit_camera.cpp)
target_link_libraries(maz_unit_tests PRIVATE maz_core maz_warnings)

add_test(NAME unit_camera COMMAND maz_unit_tests)
set_tests_properties(unit_camera PROPERTIES TIMEOUT 30)

# Unit tests: geometry primitives (AABB/Ray/Plane) and intersection helpers.
add_executable(maz_unit_geometry unit_geometry.cpp)
target_link_libraries(maz_unit_geometry PRIVATE maz_core maz_warnings)

add_test(NAME unit_geometry COMMAND maz_unit_geometry)
set_tests_properties(unit_geometry PROPERTIES TIMEOUT 30)

# Unit tests: view frustum plane extraction and culling tests.
add_executable(maz_unit_frustum unit_frustum.cpp)
target_link_libraries(maz_unit_frustum PRIVATE maz_core maz_warnings)

add_test(NAME unit_frustum COMMAND maz_unit_frustum)
set_tests_properties(unit_frustum PROPERTIES TIMEOUT 30)

# Unit tests: deterministic PRNG (PCG32) canonical vectors and helper bounds.
add_executable(maz_unit_random unit_random.cpp)
target_link_libraries(maz_unit_random PRIVATE maz_core maz_warnings)

add_test(NAME unit_random COMMAND maz_unit_random)
set_tests_properties(unit_random PROPERTIES TIMEOUT 30)

# Unit tests: affine Transform (basis+origin) compose/inverse/matrix round-trips.
add_executable(maz_unit_transform unit_transform.cpp)
target_link_libraries(maz_unit_transform PRIVATE maz_core maz_warnings)

add_test(NAME unit_transform COMMAND maz_unit_transform)
set_tests_properties(unit_transform PROPERTIES TIMEOUT 30)

# Unit tests: 2D affine Transform2D (2x2 basis+origin) identity/translation/rotation(CCW)/scale factories, compose order + non-commutativity, inverse round-trips.
add_executable(maz_unit_transform2d unit_transform2d.cpp)
target_link_libraries(maz_unit_transform2d PRIVATE maz_core maz_warnings)

add_test(NAME unit_transform2d COMMAND maz_unit_transform2d)
set_tests_properties(unit_transform2d PROPERTIES TIMEOUT 30)

# Unit tests: scalar interpolation, remapping, and easing helpers.
add_executable(maz_unit_interp unit_interp.cpp)
target_link_libraries(maz_unit_interp PRIVATE maz_core maz_warnings)

add_test(NAME unit_interp COMMAND maz_unit_interp)
set_tests_properties(unit_interp PROPERTIES TIMEOUT 30)

# Unit tests: hashed string identifiers (FNV-1a 64) and the _sid literal.
add_executable(maz_unit_stringid unit_stringid.cpp)
target_link_libraries(maz_unit_stringid PRIVATE maz_core maz_warnings)

add_test(NAME unit_stringid COMMAND maz_unit_stringid)
set_tests_properties(unit_stringid PROPERTIES TIMEOUT 30)

# Unit tests: generational Handle + object Pool (stale-reference detection).
add_executable(maz_unit_pool unit_pool.cpp)
target_link_libraries(maz_unit_pool PRIVATE maz_core maz_warnings)

add_test(NAME unit_pool COMMAND maz_unit_pool)
set_tests_properties(unit_pool PROPERTIES TIMEOUT 30)

# Unit tests: fixed-capacity FIFO ring buffer (wrap-around correctness).
add_executable(maz_unit_ringbuffer unit_ringbuffer.cpp)
target_link_libraries(maz_unit_ringbuffer PRIVATE maz_core maz_warnings)

add_test(NAME unit_ringbuffer COMMAND maz_unit_ringbuffer)
set_tests_properties(unit_ringbuffer PROPERTIES TIMEOUT 30)

# Unit tests: sparse-set component map (swap-and-pop, dense-iteration parity).
add_executable(maz_unit_sparseset unit_sparseset.cpp)
target_link_libraries(maz_unit_sparseset PRIVATE maz_core maz_warnings)

add_test(NAME unit_sparseset COMMAND maz_unit_sparseset)
set_tests_properties(unit_sparseset PROPERTIES TIMEOUT 30)

# Unit tests: named-signal event bus (subscribe/emit/disconnect, re-entrancy, generational safety).
add_executable(maz_unit_eventbus unit_eventbus.cpp)
target_link_libraries(maz_unit_eventbus PRIVATE maz_core maz_warnings)

add_test(NAME unit_eventbus COMMAND maz_unit_eventbus)
set_tests_properties(unit_eventbus PROPERTIES TIMEOUT 30)

# Unit tests: CPU profiler + RAII scoped timer (scope aggregation, min-seeding, timing smoke).
add_executable(maz_unit_profiler unit_profiler.cpp)
target_link_libraries(maz_unit_profiler PRIVATE maz_core maz_warnings)

add_test(NAME unit_profiler COMMAND maz_unit_profiler)
set_tests_properties(unit_profiler PROPERTIES TIMEOUT 30)

# Unit tests: fixed-capacity bump-pointer arena (align/exhaustion/marker rewind).
add_executable(maz_unit_linearalloc unit_linearalloc.cpp)
target_link_libraries(maz_unit_linearalloc PRIVATE maz_core maz_warnings)

add_test(NAME unit_linearalloc COMMAND maz_unit_linearalloc)
set_tests_properties(unit_linearalloc PROPERTIES TIMEOUT 30)

# Unit tests: small-vector with inline storage that spills to heap (manual element lifetime).
add_executable(maz_unit_smallvector unit_smallvector.cpp)
target_link_libraries(maz_unit_smallvector PRIVATE maz_core maz_warnings)

add_test(NAME unit_smallvector COMMAND maz_unit_smallvector)
set_tests_properties(unit_smallvector PROPERTIES TIMEOUT 30)

# Unit tests: compile-time type identifiers + minimal reflection (type_name/hash/id, TypeInfo).
add_executable(maz_unit_typeid unit_typeid.cpp)
target_link_libraries(maz_unit_typeid PRIVATE maz_core maz_warnings)

add_test(NAME unit_typeid COMMAND maz_unit_typeid)
set_tests_properties(unit_typeid PROPERTIES TIMEOUT 30)

# Unit tests: binary serialization ByteWriter/ByteReader (LE layout, bounds/overflow-safe reads).
add_executable(maz_unit_serialization unit_serialization.cpp)
target_link_libraries(maz_unit_serialization PRIVATE maz_core maz_warnings)

add_test(NAME unit_serialization COMMAND maz_unit_serialization)
set_tests_properties(unit_serialization PROPERTIES TIMEOUT 30)

# Unit tests: type-safe Flags<E> bitflags over scoped enums (composition, queries, mutators, zero-overhead sizeof, small-underlying -Wconversion path).
add_executable(maz_unit_flags unit_flags.cpp)
target_link_libraries(maz_unit_flags PRIVATE maz_core maz_warnings)

add_test(NAME unit_flags COMMAND maz_unit_flags)
set_tests_properties(unit_flags PROPERTIES TIMEOUT 30)

# Unit tests: fixed-size Bitset<N> (word-backed ECS signature mask; tail masking when N%64!=0; fast set-bit iteration via <bit>).
add_executable(maz_unit_bitset unit_bitset.cpp)
target_link_libraries(maz_unit_bitset PRIVATE maz_core maz_warnings)

add_test(NAME unit_bitset COMMAND maz_unit_bitset)
set_tests_properties(unit_bitset PROPERTIES TIMEOUT 30)

# Unit tests: single-target zero-heap Delegate<R(Args...)> (fast-delegate; free/member/const-member binds via C++20 auto NTTP, no heap).
add_executable(maz_unit_delegate unit_delegate.cpp)
target_link_libraries(maz_unit_delegate PRIVATE maz_core maz_warnings)

add_test(NAME unit_delegate COMMAND maz_unit_delegate)
set_tests_properties(unit_delegate PROPERTIES TIMEOUT 30)

# Unit tests: minimal ECS World (entity registry + generational recycle, type-erased component stores, signature/store sync, each<T> iteration).
add_executable(maz_unit_ecs_world unit_ecs_world.cpp)
target_link_libraries(maz_unit_ecs_world PRIVATE maz_core maz_warnings)

add_test(NAME unit_ecs_world COMMAND maz_unit_ecs_world)
set_tests_properties(unit_ecs_world PROPERTIES TIMEOUT 30)

# Unit tests: scene graph / transform hierarchy (dirty-flag world-transform propagation, compose order, reparent).
add_executable(maz_unit_scenegraph unit_scenegraph.cpp)
target_link_libraries(maz_unit_scenegraph PRIVATE maz_core maz_warnings)

add_test(NAME unit_scenegraph COMMAND maz_unit_scenegraph)
set_tests_properties(unit_scenegraph PROPERTIES TIMEOUT 30)

# Unit tests: ECS system scheduler (ordered systems over World; registration-order run, enable/remove, movement-system end-to-end).
add_executable(maz_unit_ecs_scheduler unit_ecs_scheduler.cpp)
target_link_libraries(maz_unit_ecs_scheduler PRIVATE maz_core maz_warnings)

add_test(NAME unit_ecs_scheduler COMMAND maz_unit_ecs_scheduler)
set_tests_properties(unit_ecs_scheduler PROPERTIES TIMEOUT 30)

# Unit tests: sparse uniform spatial hash grid (broad+narrow phase queries, multi-cell dedup, negative-coord floor cell math, insert/remove/update/clear).
add_executable(maz_unit_spatial unit_spatial.cpp)
target_link_libraries(maz_unit_spatial PRIVATE maz_core maz_warnings)

add_test(NAME unit_spatial COMMAND maz_unit_spatial)
set_tests_properties(unit_spatial PROPERTIES TIMEOUT 30)

# Unit tests: device-neutral action-mapping layer (bind abstract actions to buttons/axes; OR-combined buttons, digital+analog axes summed then clamped, edge detection, unknown-action safety).
add_executable(maz_unit_action_map unit_action_map.cpp)
target_link_libraries(maz_unit_action_map PRIVATE maz_core maz_warnings)

add_test(NAME unit_action_map COMMAND maz_unit_action_map)
set_tests_properties(unit_action_map PROPERTIES TIMEOUT 30)

# Unit tests: ECS core components (LocalTransform/WorldTransform/Name/Tag/Parent + findByName/setParent/parentOf/childrenOf helpers over World).
add_executable(maz_unit_ecs_components unit_ecs_components.cpp)
target_link_libraries(maz_unit_ecs_components PRIVATE maz_core maz_warnings)

add_test(NAME unit_ecs_components COMMAND maz_unit_ecs_components)
set_tests_properties(unit_ecs_components PROPERTIES TIMEOUT 30)

# Unit tests: registration-based ECS scene (de)serializer (Name/Tag/LocalTransform round-trip, Parent entity-reference remap via EntityRemap, unknown-tag forward-compat skip, fail-safe load on corrupt/truncated/bad-magic/bad-version, empty-world round-trip, byte-for-byte determinism).
add_executable(maz_unit_scene_serializer unit_scene_serializer.cpp)
target_link_libraries(maz_unit_scene_serializer PRIVATE maz_core maz_warnings)

add_test(NAME unit_scene_serializer COMMAND maz_unit_scene_serializer)
set_tests_properties(unit_scene_serializer PROPERTIES TIMEOUT 30)

# Unit tests: ECS transform-propagation system (LocalTransform+Parent->WorldTransform up the parent chain; per-pass memoized recursion so parents resolve before children in any dense-iteration order, parentWorld*local compose order, transform-less/dead parent treated as root, idempotent, cycle-safe, end-to-end via SystemScheduler).
add_executable(maz_unit_transform_system unit_transform_system.cpp)
target_link_libraries(maz_unit_transform_system PRIVATE maz_core maz_warnings)

add_test(NAME unit_transform_system COMMAND maz_unit_transform_system)
set_tests_properties(unit_transform_system PROPERTIES TIMEOUT 30)

# Unit tests: typed runtime console-variable / settings registry (register bool/int/float/string with defaults+descriptions; type-checked get/set with wrong-type/missing fallbacks; setFromString parsing incl. parse-fail-leaves-unchanged; reset-to-default; change callbacks firing only on actual value change; typeOf; clear).
add_executable(maz_unit_cvar unit_cvar.cpp)
target_link_libraries(maz_unit_cvar PRIVATE maz_core maz_warnings)

add_test(NAME unit_cvar COMMAND maz_unit_cvar)
set_tests_properties(unit_cvar PROPERTIES TIMEOUT 30)

# Unit tests: single-value float eased tween (from->to over duration via iter6 Easing/ease/lerp; Once end-clamp + onComplete fire-once, Loop/PingPong fmod phase wrap incl. multi-cycle dt, zero-duration div-by-zero guard, reset/restart).
add_executable(maz_unit_tween unit_tween.cpp)
target_link_libraries(maz_unit_tween PRIVATE maz_core maz_warnings)

add_test(NAME unit_tween COMMAND maz_unit_tween)
set_tests_properties(unit_tween PROPERTIES TIMEOUT 30)

# Unit tests: 2D grid A* pathfinder (GridMap + findPath; integer 10/14 costs, Manhattan/octile heuristic, corner-cutting-forbidden diagonals, deterministic tie-break; straight/detour/unreachable/diagonal/corner-cut/optimality via cost+validity assertions).
add_executable(maz_unit_astar unit_astar.cpp)
target_link_libraries(maz_unit_astar PRIVATE maz_core maz_warnings)

add_test(NAME unit_astar COMMAND maz_unit_astar)
set_tests_properties(unit_astar PROPERTIES TIMEOUT 30)

# Unit tests: general weighted-graph A*/Dijkstra shortest-path finder (AStarGraph; nodes with 3D positions + non-negative weighted directed/bidirectional edges, Euclidean heuristic scaled by heuristicScale where 0 == pure Dijkstra, deterministic (f,h,id) tie-break, cost-optimal reconstruction; linear/diamond/start==goal/unreachable/directed/admissible-A*==Dijkstra/determinism via cost+endpoint assertions).
add_executable(maz_unit_astar_graph unit_astar_graph.cpp)
target_link_libraries(maz_unit_astar_graph PRIVATE maz_core maz_warnings)

add_test(NAME unit_astar_graph COMMAND maz_unit_astar_graph)
set_tests_properties(unit_astar_graph PROPERTIES TIMEOUT 30)

# Unit tests: event-driven finite state machine (StringId states/events, onEnter/onUpdate/onExit callbacks; start/fire/update/reset; transition ordering exitOld->enterNew, self-transition fires both, unmatched event no-ops, update ticks only current, reset+restart re-enters initial, callback-less states safe).
add_executable(maz_unit_state_machine unit_state_machine.cpp)
target_link_libraries(maz_unit_state_machine PRIVATE maz_core maz_warnings)

add_test(NAME unit_state_machine COMMAND maz_unit_state_machine)
set_tests_properties(unit_state_machine PROPERTIES TIMEOUT 30)

# Unit tests: classic game-AI behavior tree (unique_ptr node tree ticked top-down; Sequence AND short-circuit, Selector OR first-success, Inverter/Succeeder/Repeater decorators, Action leaves returning Success/Failure/Running; composites remember a Running child and resume there next tick, reset() restarts from the top, BehaviorTree wrapper delegates to a nested root).
add_executable(maz_unit_behavior_tree unit_behavior_tree.cpp)
target_link_libraries(maz_unit_behavior_tree PRIVATE maz_core maz_warnings)

add_test(NAME unit_behavior_tree COMMAND maz_unit_behavior_tree)
set_tests_properties(unit_behavior_tree PROPERTIES TIMEOUT 30)

# Unit tests: classic Reynolds steering behaviors (seek/flee/arrive/pursue/evade returning a maxForce-truncated steering force; arrive ramps speed down inside slowRadius; pursue/evade seek/flee the linearly-predicted future position; safeNormalize/truncate guard zero-length vectors so no NaN escapes; exact-representable axis-aligned/3-4-5 inputs).
add_executable(maz_unit_steering unit_steering.cpp)
target_link_libraries(maz_unit_steering PRIVATE maz_core maz_warnings)

add_test(NAME unit_steering COMMAND maz_unit_steering)
set_tests_properties(unit_steering PROPERTIES TIMEOUT 30)

# Unit tests: uniform Catmull-Rom spline through control points (CatmullRomSpline; evaluate(u) over global u in [0,segmentCount] interpolating position and passing through every control point exactly with endpoint clamping, tangent(u) unnormalized travel direction, sample(n) boundary-sharing render polyline; interior collinear equally-spaced segments reproduce the straight line, out-of-range u clamps to the endpoints, exact-representable integer/half inputs).
add_executable(maz_unit_spline unit_spline.cpp)
target_link_libraries(maz_unit_spline PRIVATE maz_core maz_warnings)

add_test(NAME unit_spline COMMAND maz_unit_spline)
set_tests_properties(unit_spline PROPERTIES TIMEOUT 30)

# Unit tests: sphere primitive + sphere/ray-sphere intersection helpers complementing Geometry (Sphere::contains; sphereSphere/sphereAabb/spherePlane inclusive overlap via squared-distance/closest-point; closestPointOnAabb per-component clamp; raySphere earliest t>=0 with inside-origin exit-root fallback, behind-ray rejection, and unnormalized-dir t in ray-dir units).
add_executable(maz_unit_collision unit_collision.cpp)
target_link_libraries(maz_unit_collision PRIVATE maz_core maz_warnings)

add_test(NAME unit_collision COMMAND maz_unit_collision)
set_tests_properties(unit_collision PROPERTIES TIMEOUT 30)

# Unit tests: Ken Perlin improved gradient noise (PerlinNoise; noise2D/noise3D exactly 0 at integer lattice coords, seed-determined permutation via Rng so same seed reproduces bit-identical fields and different seeds differ, output bounded ~[-1,1] and actually varies, noise2D == z=0 slice of noise3D, fbm2D octaves==1 reproduces noise2D + multi-octave finite/bounded/deterministic).
add_executable(maz_unit_noise unit_noise.cpp)
target_link_libraries(maz_unit_noise PRIVATE maz_core maz_warnings)

add_test(NAME unit_noise COMMAND maz_unit_noise)
set_tests_properties(unit_noise PROPERTIES TIMEOUT 30)

# Unit tests: typed StringId-keyed shared-memory blackboard (Blackboard; std::any-backed set/get roundtrip across int/float/bool/StringId/vec3, type-checked has<T>, always-safe getOr<T> fallback on absent OR wrong type, last-write-wins overwrite incl. changing a key's stored type, erase/clear/empty/size bookkeeping, fresh empty blackboard).
add_executable(maz_unit_blackboard unit_blackboard.cpp)
target_link_libraries(maz_unit_blackboard PRIVATE maz_core maz_warnings)

add_test(NAME unit_blackboard COMMAND maz_unit_blackboard)
set_tests_properties(unit_blackboard PROPERTIES TIMEOUT 30)

# Unit tests: update(dt)-advanced timer/callback scheduler (TimerManager; one-shot after(delay)/repeating every(interval) firing std::function callbacks, catch-up firing N times over a large dt with remainder carry, cancel deactivate-not-erase; re-entrancy — a callback may cancel any timer incl. itself and schedule new timers that are DEFERRED to the next tick, registration-order firing within a tick, clear).
add_executable(maz_unit_timer unit_timer.cpp)
target_link_libraries(maz_unit_timer PRIVATE maz_core maz_warnings)

add_test(NAME unit_timer COMMAND maz_unit_timer)
set_tests_properties(unit_timer PROPERTIES TIMEOUT 30)

# Unit tests: 2D multi-layer tile grid (TileMap; row-major-within-layer flat index over stacked layers, set/at roundtrip, out-of-bounds x/y reads as empty while writes assert in-bounds, per-layer independence, whole-layer fill, inclusive+clamped+reversed-coord-normalizing fillRegion, clear across all layers, four-corner index math with no cross-layer aliasing).
add_executable(maz_unit_tilemap unit_tilemap.cpp)
target_link_libraries(maz_unit_tilemap PRIVATE maz_core maz_warnings)

add_test(NAME unit_tilemap COMMAND maz_unit_tilemap)
set_tests_properties(unit_tilemap PROPERTIES TIMEOUT 30)

# Unit tests: hierarchical adaptive point octree (Octree<T>; leaves subdivide into 8 octants once they exceed maxPerNode, bounded by maxDepth so coincident points beyond capacity pile up in the deepest leaf; octantOf/childBounds agree so redistribution loses no point; queryAabb/querySphere descend only into octants overlapping the query — Aabb-intersection / iter36 sphereAabb pruning — then narrow-test each candidate point; insert/size/empty/clear, append-not-clear query contract).
add_executable(maz_unit_octree unit_octree.cpp)
target_link_libraries(maz_unit_octree PRIVATE maz_core maz_warnings)

add_test(NAME unit_octree COMMAND maz_unit_octree)
set_tests_properties(unit_octree PROPERTIES TIMEOUT 30)

# Unit tests: ASCII locale-independent string helpers (maz::core::str; startsWith/endsWith/contains, borrowing trim/trimLeft/trimRight views, split keepEmpty vs skipEmpty incl. empty-input single-empty-piece vs empty-vector semantics, join with split round-trip, toLower/toUpper/equalsIgnoreCase ASCII case-fold, non-overlapping replaceAll incl. empty-'to', whole-string parseInt/parseFloat via from_chars with no whitespace leniency and out-untouched-on-failure).
add_executable(maz_unit_stringutil unit_stringutil.cpp)
target_link_libraries(maz_unit_stringutil PRIVATE maz_core maz_warnings)

add_test(NAME unit_stringutil COMMAND maz_unit_stringutil)
set_tests_properties(unit_stringutil PROPERTIES TIMEOUT 30)

# Unit tests: random sampling helpers over the iter4 Rng (Fisher-Yates shuffle permutation+seed determinism, weightedIndex loot-table draw proportional to non-negative weights incl. RNG-independent single-non-zero-weight and zero-weight-never-picked, sampleWithoutReplacement k distinct indices / k==n full permutation / k==0 empty / determinism, Box-Muller gaussian seed-determinism+finite+stddev-0-returns-mean).
add_executable(maz_unit_sampling unit_sampling.cpp)
target_link_libraries(maz_unit_sampling PRIVATE maz_core maz_warnings)

add_test(NAME unit_sampling COMMAND maz_unit_sampling)
set_tests_properties(unit_sampling PROPERTIES TIMEOUT 30)

# Unit tests: quaternion rotation helpers over glm (maz::math::Rotation; fromAxisAngle normalizing a non-unit axis, rotate(q,v), fromEuler documented YXZ/Godot-default single-axis == fromAxisAngle, angleBetween unsigned + double-cover-safe via abs(dot)+acos-domain clamp, shortest-path slerp endpoints/midpoint-arc, rotateTowards angle-clamped step + reach/coincident short-circuit, lookRotation local -Z maps to forward with +Y-near-up camera-convention basis; right-hand-rule/handedness hand-verified expecteds).
add_executable(maz_unit_rotation unit_rotation.cpp)
target_link_libraries(maz_unit_rotation PRIVATE maz_core maz_warnings)

add_test(NAME unit_rotation COMMAND maz_unit_rotation)
set_tests_properties(unit_rotation PROPERTIES TIMEOUT 30)

# Unit tests: base64 + hex binary-to-text codecs (maz::core; RFC 4648 standard-alphabet base64 with '=' padding — exact canonical vectors, decode round-trip incl. all bytes 0..255 with no signed-char corruption, invalid-length/char/misplaced-padding rejection leaving out cleared, empty-input decodes to nothing; hex encode/decode with case handling, odd-length/non-hex-char rejection).
add_executable(maz_unit_base64 unit_base64.cpp)
target_link_libraries(maz_unit_base64 PRIVATE maz_core maz_warnings)

add_test(NAME unit_base64 COMMAND maz_unit_base64)
set_tests_properties(unit_base64 PROPERTIES TIMEOUT 30)

# Unit tests: bounded least-recently-used cache (maz::core::LruCache<K,V>; MRU-ordered intrusive list + key->node map, put insert/update+promote with evict-after-insert LRU eviction over capacity, get promotes to MRU while peek/contains do not — opposite eviction victims prove the promotion semantics, live/mutable get pointer into storage, erase/clear/empty/size bookkeeping, capacity-1 edge).
add_executable(maz_unit_lru unit_lru.cpp)
target_link_libraries(maz_unit_lru PRIVATE maz_core maz_warnings)

add_test(NAME unit_lru COMMAND maz_unit_lru)
set_tests_properties(unit_lru PROPERTIES TIMEOUT 30)

# Unit tests: command-pattern undo/redo history (maz::core::UndoStack; push(redo,undo) records a reversible command and by default applies redo immediately, execute=false records without applying, undo/redo walk a cursor over captured-prior-value commands, pushing after an undo truncates the orphaned redo branch, canUndo/canRedo/undoCount/redoCount bookkeeping, clear/empty, optional capacity bound drops the oldest undo step when exceeded).
add_executable(maz_unit_undo unit_undo.cpp)
target_link_libraries(maz_unit_undo PRIVATE maz_core maz_warnings)

add_test(NAME unit_undo COMMAND maz_unit_undo)
set_tests_properties(unit_undo PROPERTIES TIMEOUT 30)

# Unit tests: orbit + fly camera rigs (maz::scene::OrbitCamera/FlyCamera; OrbitCamera spherical position() from focus/yaw/pitch/distance with pitch clamped away from the pole and zoom-clamped distance, FlyCamera -Z forward()/world-up right()/moveLocal along its own axes, both applyTo(Camera) setting position+target; clean 0/±1 trig targets, +X right() handedness hand-verified).
add_executable(maz_unit_camera_controller unit_camera_controller.cpp)
target_link_libraries(maz_unit_camera_controller PRIVATE maz_core maz_warnings)

add_test(NAME unit_camera_controller COMMAND maz_unit_camera_controller)
set_tests_properties(unit_camera_controller PROPERTIES TIMEOUT 30)

# Unit tests: cross-platform byte-order helpers (maz::core Endian.hpp; byteSwap16/32/64 exact values + involution, explicit-endianness pack/unpack writeU32LE/BE exact byte layout, write/read round-trips across 16/32/64 both endiannesses, reads from known bytes, host<->little/big conversions with hostIsLittleEndian()-computed native-identity, cross-endianness LE-written-read-as-BE == byteSwap consistency; host-independent).
add_executable(maz_unit_endian unit_endian.cpp)
target_link_libraries(maz_unit_endian PRIVATE maz_core maz_warnings)

add_test(NAME unit_endian COMMAND maz_unit_endian)
set_tests_properties(unit_endian PROPERTIES TIMEOUT 30)

# Unit tests: string interning table (maz::core::StringInterner; intern(s) hashes via iter7 StringId::fromBytes AND stores the original text so resolve(id) recovers the source string — the original-string storage companion to hash-only StringId, à la Godot's StringName; intern+resolve roundtrip, same-string de-dup returning the same id/stored once, interned id == plain StringId::fromBytes == _sid UDL, un-interned id resolves to an empty view / contains false, both contains overloads (id and string), multiple distinct strings, empty-string edge (valid non-zero id, empty view, contained), clear).
add_executable(maz_unit_interner unit_interner.cpp)
target_link_libraries(maz_unit_interner PRIVATE maz_core maz_warnings)

add_test(NAME unit_interner COMMAND maz_unit_interner)
set_tests_properties(unit_interner PROPERTIES TIMEOUT 30)

# Unit tests: IEEE 754 binary16 half-float conversion (maz::math HalfFloat.hpp; floatToHalf f32->f16 round-to-nearest-even with overflow-to-Inf, subnormal-half, NaN/Inf passthrough, and halfToFloat exact f16->f32 decode — canonical bit-pattern encode/decode vectors, exact round-trips of half-representable magnitudes, overflow saturation, Inf/NaN passthrough, smallest-subnormal 2^-24 decode/encode, and hand-verified round-to-nearest-even boundaries; std::bit_cast, header-only).
add_executable(maz_unit_halffloat unit_halffloat.cpp)
target_link_libraries(maz_unit_halffloat PRIVATE maz_core maz_warnings)

add_test(NAME unit_halffloat COMMAND maz_unit_halffloat)
set_tests_properties(unit_halffloat PROPERTIES TIMEOUT 30)

# Unit tests: 2D convex hull via Andrew's monotone chain (maz::math::convexHull2D; sorts+dedupes a by-value copy then sweeps a lower+upper hull returning CCW vertices with collinear edge points excluded (minimal hull) and no repeated closing vertex, deterministic start at the lexicographically-smallest point; square/triangle/collinear-degenerate-to-segment, edge-midpoints+interior excluded so a square stays 4 corners, duplicate removal, <3-unique-point point/segment passthrough, exact CCW sequence, pentagon-of-extremes with interior points excluded; integer-valued vec2 for exact float compares).
add_executable(maz_unit_convexhull unit_convexhull.cpp)
target_link_libraries(maz_unit_convexhull PRIVATE maz_core maz_warnings)

add_test(NAME unit_convexhull COMMAND maz_unit_convexhull)
set_tests_properties(unit_convexhull PROPERTIES TIMEOUT 30)

# Unit tests: 2D convex-polygon overlap via the Separating Axis Theorem (maz::math::convexPolygonsOverlap; projects both CONVEX polygons onto every edge normal of both, reporting overlap unless some axis's [min,max] projection intervals are STRICTLY disjoint so touching/shared-edge counts as overlap (inclusive), axes unnormalized/scale-invariant, any winding; overlapping/separated/edge-touching/containment/triangle-vs-square, the diagonal-separation SAT anchor where AABBs overlap but the hypotenuse normal separates, symmetry+determinism, CW==CCW winding independence; integer-valued vec2 for exact projection compares).
add_executable(maz_unit_collision2d unit_collision2d.cpp)
target_link_libraries(maz_unit_collision2d PRIVATE maz_core maz_warnings)

add_test(NAME unit_collision2d COMMAND maz_unit_collision2d)
set_tests_properties(unit_collision2d PROPERTIES TIMEOUT 30)

# Unit tests: handle-based cubic Bezier curve (maz::math::CubicBezier + free cubicBezier/cubicBezierTangent; four control points where the curve passes through the endpoints p0/p3 and is pulled toward the handles p1/p2 which it does NOT interpolate; evaluate(t) Bernstein polynomial hitting p0 at t=0 and p3 at t=1 exactly and extrapolating outside [0,1], tangent(t) analytic derivative == 3*(p1-p0) at t=0 / 3*(p3-p2) at t=1, sample(steps) first==p0/last==p3 render polyline; evenly-spaced collinear controls reproduce a straight line with constant (3,0,0) tangent, symmetric-arc midpoint, free-function==method parity, exact-representable integer/half/quarter inputs; complements iter35's interpolating Catmull-Rom).
add_executable(maz_unit_bezier unit_bezier.cpp)
target_link_libraries(maz_unit_bezier PRIVATE maz_core maz_warnings)

add_test(NAME unit_bezier COMMAND maz_unit_bezier)
set_tests_properties(unit_bezier PROPERTIES TIMEOUT 30)

# Unit tests: 2D axis-aligned rectangle (maz::math::Rect2; position (min corner) + size, the Godot Rect2 analog / 2D counterpart to iter2's 3D Aabb; min/max/center/area/hasArea getters, HALF-OPEN contains (inclusive min edge, exclusive max edge — has_point convention), intersects with EXCLUSIVE borders (edge-touching rects do NOT overlap), intersection (overlap rect, zero-size when disjoint), merge/expand bounding rect over another rect / a point, grow inflating all four sides, fromMinMax convenience; integer-valued vec2 coords for exact compares; header-only).
add_executable(maz_unit_rect2 unit_rect2.cpp)
target_link_libraries(maz_unit_rect2 PRIVATE maz_core maz_warnings)

add_test(NAME unit_rect2 COMMAND maz_unit_rect2)
set_tests_properties(unit_rect2 PROPERTIES TIMEOUT 30)
