add_executable(motor main.cpp)

target_link_libraries(motor PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
