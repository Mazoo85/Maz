add_executable(goap main.cpp)

target_link_libraries(goap PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
