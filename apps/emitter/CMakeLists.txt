add_executable(emitter main.cpp)

target_link_libraries(emitter PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
