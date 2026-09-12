add_executable(gradient main.cpp)

target_link_libraries(gradient PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
