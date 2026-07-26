# Playable terminal front-end for the Zomboid port (POSIX-only: uses termios).
add_executable(zomboid_tui main.cpp)
target_link_libraries(zomboid_tui PRIVATE maz::zomboid maz_warnings)
set_target_properties(zomboid_tui PROPERTIES OUTPUT_NAME zomboid-tui)
