add_executable(cave main.cpp)

target_link_libraries(cave PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
