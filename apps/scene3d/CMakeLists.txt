add_executable(scene3d main.cpp)

target_link_libraries(scene3d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
