add_executable(actions main.cpp)

target_link_libraries(actions PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
