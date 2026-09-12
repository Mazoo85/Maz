add_executable(crowd main.cpp)

target_link_libraries(crowd PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
