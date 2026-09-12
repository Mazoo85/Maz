add_executable(scenes main.cpp)

target_link_libraries(scenes PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
