add_executable(behavior main.cpp)

target_link_libraries(behavior PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
