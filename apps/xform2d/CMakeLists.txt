add_executable(xform2d main.cpp)

target_link_libraries(xform2d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
