add_executable(config main.cpp)

target_link_libraries(config PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
