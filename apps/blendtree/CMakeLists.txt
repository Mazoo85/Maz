add_executable(blendtree main.cpp)

target_link_libraries(blendtree PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
