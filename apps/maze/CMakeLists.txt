add_executable(maze main.cpp)

target_link_libraries(maze PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
