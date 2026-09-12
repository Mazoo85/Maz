add_executable(camera3d main.cpp)

target_link_libraries(camera3d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
