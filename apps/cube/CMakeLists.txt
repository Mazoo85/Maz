add_executable(cube main.cpp)

target_link_libraries(cube PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
