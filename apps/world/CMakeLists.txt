add_executable(world main.cpp)

target_link_libraries(world PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
