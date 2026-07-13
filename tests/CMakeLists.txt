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
