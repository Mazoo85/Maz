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

# Unit tests: pure C++ (no GPU/display). Camera view/projection math.
add_executable(maz_unit_tests unit_camera.cpp)
target_link_libraries(maz_unit_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_camera COMMAND maz_unit_tests)
set_tests_properties(unit_camera PROPERTIES TIMEOUT 30)

# Unit tests: scene model + .mazscene save/load round-trip.
add_executable(maz_scene_tests unit_scene.cpp)
target_link_libraries(maz_scene_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_scene COMMAND maz_scene_tests)
set_tests_properties(unit_scene PROPERTIES TIMEOUT 30)

# Unit tests: procedural primitive mesh generation (vertex/index counts, unit normals, bounds).
add_executable(maz_primitives_tests unit_primitives.cpp)
target_link_libraries(maz_primitives_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_primitives COMMAND maz_primitives_tests)
set_tests_properties(unit_primitives PROPERTIES TIMEOUT 30)

# Unit tests: composite asset baking (buildModel) + .mazasset save/load round-trip.
add_executable(maz_asset_tests unit_asset.cpp)
target_link_libraries(maz_asset_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_asset COMMAND maz_asset_tests)
set_tests_properties(unit_asset PROPERTIES TIMEOUT 30)

# Load a scene headless and require a clean exit — exercises scene parsing + the render wiring
# end to end (draws no-op with no GPU).
add_test(
    NAME scene_load_headless
    COMMAND sandbox --headless --frames 1 --scene ${CMAKE_SOURCE_DIR}/assets/scenes/demo.mazscene)
set_tests_properties(scene_load_headless PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "loaded scene")

# Editor: start headless, load a scene, run a few frames, exit cleanly. Verifies the editor app +
# ImGui integration link and boot (the UI itself needs a GPU and is not exercised here).
add_test(
    NAME editor_headless_smoke
    COMMAND editor --headless --frames 3 --scene ${CMAKE_SOURCE_DIR}/assets/scenes/demo.mazscene)
set_tests_properties(editor_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "Maz Editor shutting down")

# Render probe: actually rasterize the cube off-screen (via a software Vulkan driver, lavapipe, in
# CI) and verify the pixels — proof the mesh renderer draws, not just compiles. Requires a Vulkan
# ICD; fails loudly if none is present so a missing driver is visible rather than silently skipped.
add_test(
    NAME render_probe
    COMMAND render_probe ${CMAKE_SOURCE_DIR}/assets/models/cube.gltf)
set_tests_properties(render_probe PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "render probe: PASS")

# Render probe over a baked composite asset: bake a .mazasset to a mesh and rasterize it off-screen,
# proving the character/item creator's build path produces drawable geometry end to end (not just
# correct vertex counts). Same lavapipe requirement as render_probe.
add_test(
    NAME render_probe_asset
    COMMAND render_probe ${CMAKE_SOURCE_DIR}/assets/items/probe_asset.mazasset)
set_tests_properties(render_probe_asset PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "render probe: PASS")
