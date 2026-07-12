add_executable(timeline main.cpp)

target_link_libraries(timeline PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
