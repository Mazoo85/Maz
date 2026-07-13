add_executable(render_probe main.cpp)

target_link_libraries(render_probe PRIVATE maz::maz maz_warnings)

# Needs the same compiled mesh shaders as the sandbox (shared bin/shaders directory).
add_dependencies(render_probe sandbox_shaders)
