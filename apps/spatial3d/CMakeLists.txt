add_executable(spatial3d main.cpp)

target_link_libraries(spatial3d PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
