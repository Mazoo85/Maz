add_executable(instances main.cpp)

target_link_libraries(instances PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
