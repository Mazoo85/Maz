add_executable(flowfield main.cpp)

target_link_libraries(flowfield PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
