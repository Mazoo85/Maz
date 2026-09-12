add_executable(solver main.cpp)

target_link_libraries(solver PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
