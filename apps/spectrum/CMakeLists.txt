add_executable(spectrum main.cpp)

target_link_libraries(spectrum PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
