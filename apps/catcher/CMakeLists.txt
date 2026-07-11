add_executable(catcher main.cpp)

target_link_libraries(catcher PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
