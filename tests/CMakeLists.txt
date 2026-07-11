# Unit tests for the pure-logic modules (math, collision, spatial grid, ECS, shake, particles).
# No GPU/window needed — fast, deterministic, and the first line of defence against logic bugs.
add_executable(maz_unit_tests unit/main.cpp)
target_link_libraries(maz_unit_tests PRIVATE maz::maz maz_warnings)
add_test(NAME unit_tests COMMAND maz_unit_tests)
set_tests_properties(unit_tests PROPERTIES TIMEOUT 30)

# Golden-image regression: render each app on lavapipe and diff against committed references. Self-
# skips (exit 0) when software Vulkan / Xvfb / ImageMagick are unavailable, so it's harmless in a
# GPU-less CI. Requires an up-to-date build/bin.
add_test(NAME golden_images COMMAND ${CMAKE_SOURCE_DIR}/tools/golden.sh check)
set_tests_properties(golden_images PROPERTIES TIMEOUT 180)

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

add_test(
    NAME instances_headless_smoke
    COMMAND instances --headless --frames 30)

set_tests_properties(instances_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME glass_headless_smoke
    COMMAND glass --headless --frames 30)

set_tests_properties(glass_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME maze_headless_smoke
    COMMAND maze --headless --frames 30)

set_tests_properties(maze_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME crowd_headless_smoke
    COMMAND crowd --headless --frames 30)

set_tests_properties(crowd_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tween_headless_smoke
    COMMAND tween --headless --frames 30)

set_tests_properties(tween_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME menu_headless_smoke
    COMMAND menu --headless --frames 30)

set_tests_properties(menu_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME persist_headless_smoke
    COMMAND persist --headless --frames 30)

set_tests_properties(persist_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME guard_headless_smoke
    COMMAND guard --headless --frames 30)

set_tests_properties(guard_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME sprites_headless_smoke
    COMMAND sprites --headless --frames 30)

set_tests_properties(sprites_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME events_headless_smoke
    COMMAND events --headless --frames 30)

set_tests_properties(events_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME jobs_headless_smoke
    COMMAND jobs --headless --frames 30)

set_tests_properties(jobs_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME assetcache_headless_smoke
    COMMAND assetcache --headless --frames 30)

set_tests_properties(assetcache_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME skeleton_headless_smoke
    COMMAND skeleton --headless --frames 30)

set_tests_properties(skeleton_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME animclip_headless_smoke
    COMMAND animclip --headless --frames 30)

set_tests_properties(animclip_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME physics_headless_smoke
    COMMAND physics --headless --frames 30)

set_tests_properties(physics_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME animator_headless_smoke
    COMMAND animator --headless --frames 30)

set_tests_properties(animator_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME behavior_headless_smoke
    COMMAND behavior --headless --frames 30)

set_tests_properties(behavior_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME boxes_headless_smoke
    COMMAND boxes --headless --frames 30)

set_tests_properties(boxes_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")










