add_executable(boundary main.cpp)

target_link_libraries(boundary PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
