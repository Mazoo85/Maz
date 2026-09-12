add_executable(ring main.cpp)

target_link_libraries(ring PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
