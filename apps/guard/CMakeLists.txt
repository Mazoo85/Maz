add_executable(guard main.cpp)

target_link_libraries(guard PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
