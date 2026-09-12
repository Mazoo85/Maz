add_executable(stack main.cpp)

target_link_libraries(stack PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
