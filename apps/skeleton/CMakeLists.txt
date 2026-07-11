add_executable(skeleton main.cpp)

target_link_libraries(skeleton PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
