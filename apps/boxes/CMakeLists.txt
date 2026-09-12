add_executable(boxes main.cpp)

target_link_libraries(boxes PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
