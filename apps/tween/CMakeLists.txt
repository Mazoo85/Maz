add_executable(tween main.cpp)

target_link_libraries(tween PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
