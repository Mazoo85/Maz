add_executable(sleep main.cpp)

target_link_libraries(sleep PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
