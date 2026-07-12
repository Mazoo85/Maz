# Smoke test: run the sandbox headless for a few frames and require a clean exit(0).
# This validates module wiring, resource lifetime, and shutdown without needing a GPU/display.
add_test(
    NAME sandbox_headless_smoke
    COMMAND sandbox --headless --frames 30)

set_tests_properties(sandbox_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# ZOMBOID simulation unit tests — dependency-free, no GPU/display required.
add_executable(zomboid_tests zomboid_tests.cpp)
target_link_libraries(zomboid_tests PRIVATE maz::zomboid maz_warnings)

add_test(NAME zomboid_sim_tests COMMAND zomboid_tests)
set_tests_properties(zomboid_sim_tests PROPERTIES TIMEOUT 60)
