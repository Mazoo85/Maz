add_executable(swarm main.cpp)

target_link_libraries(swarm PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
