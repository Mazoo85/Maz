add_executable(noise main.cpp)

target_link_libraries(noise PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
