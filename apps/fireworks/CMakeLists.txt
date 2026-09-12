add_executable(fireworks main.cpp)

target_link_libraries(fireworks PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
