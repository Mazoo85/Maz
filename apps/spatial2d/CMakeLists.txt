add_executable(spatial2d main.cpp)

target_link_libraries(spatial2d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
