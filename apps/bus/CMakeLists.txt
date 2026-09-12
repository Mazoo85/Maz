add_executable(bus main.cpp)

target_link_libraries(bus PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
