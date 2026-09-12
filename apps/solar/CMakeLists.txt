add_executable(solar main.cpp)

target_link_libraries(solar PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
