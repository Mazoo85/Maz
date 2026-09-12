add_executable(tentacle main.cpp)

target_link_libraries(tentacle PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
