add_executable(normalmap main.cpp)

target_link_libraries(normalmap PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
