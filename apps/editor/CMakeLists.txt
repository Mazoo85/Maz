add_executable(editor main.cpp)

target_link_libraries(editor PRIVATE maz::maz maz_imgui maz_warnings)

# The editor renders meshes, so it needs the same compiled shaders as the sandbox. They land in
# the shared bin/shaders directory, so reuse the sandbox's shader build rather than compiling the
# same outputs twice (which CMake would reject).
add_dependencies(editor sandbox_shaders)
