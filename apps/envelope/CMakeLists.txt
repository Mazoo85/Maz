add_executable(envelope main.cpp)

target_link_libraries(envelope PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
