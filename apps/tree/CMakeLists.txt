add_executable(tree main.cpp)

target_link_libraries(tree PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
