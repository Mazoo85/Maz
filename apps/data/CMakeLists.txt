add_executable(data main.cpp)

target_link_libraries(data PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
