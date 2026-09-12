add_executable(physics main.cpp)

target_link_libraries(physics PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
