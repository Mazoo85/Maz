add_executable(reverb main.cpp)

target_link_libraries(reverb PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
