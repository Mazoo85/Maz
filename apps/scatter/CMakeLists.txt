add_executable(scatter main.cpp)

target_link_libraries(scatter PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
