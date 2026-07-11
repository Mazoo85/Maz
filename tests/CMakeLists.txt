# Smoke test: run the sandbox headless for a few frames and require a clean exit(0).
# This validates module wiring, resource lifetime, and shutdown without needing a GPU/display.
add_test(
    NAME sandbox_headless_smoke
    COMMAND sandbox --headless --frames 30)

set_tests_properties(sandbox_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME orbs_headless_smoke
    COMMAND orbs --headless --frames 30)

set_tests_properties(orbs_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME swarm_headless_smoke
    COMMAND swarm --headless --frames 30)

set_tests_properties(swarm_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME cube_headless_smoke
    COMMAND cube --headless --frames 30)

set_tests_properties(cube_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME scene3d_headless_smoke
    COMMAND scene3d --headless --frames 30)

set_tests_properties(scene3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME world_headless_smoke
    COMMAND world --headless --frames 30)

set_tests_properties(world_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME model_headless_smoke
    COMMAND model --headless --frames 30)

set_tests_properties(model_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME village_headless_smoke
    COMMAND village --headless --frames 30)

set_tests_properties(village_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME water_headless_smoke
    COMMAND water --headless --frames 30)

set_tests_properties(water_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
