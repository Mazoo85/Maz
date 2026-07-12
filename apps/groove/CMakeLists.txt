add_executable(groove main.cpp)

target_link_libraries(groove PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
