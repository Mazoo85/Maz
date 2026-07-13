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
