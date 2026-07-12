add_executable(lights2d main.cpp)

target_link_libraries(lights2d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
