add_executable(sprite_probe main.cpp)

target_link_libraries(sprite_probe PRIVATE maz::maz maz_warnings)

# Needs the compiled mesh + sprite shaders next to it (shared bin/shaders directory, built by
# the sandbox's shader target).
add_dependencies(sprite_probe sandbox_shaders)
