add_executable(shooter main.cpp Sim.cpp)

target_include_directories(shooter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(shooter PRIVATE maz::maz maz_warnings)

# DEAD SECTOR draws with the 2D sprite renderer, so it needs the same compiled sprite/mesh shaders
# as the sandbox. They land in the shared bin/shaders directory, so reuse the sandbox's shader
# build rather than compiling the same outputs twice (which CMake would reject).
add_dependencies(shooter sandbox_shaders)
