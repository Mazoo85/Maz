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
