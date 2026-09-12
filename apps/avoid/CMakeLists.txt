add_executable(avoid main.cpp)

target_link_libraries(avoid PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
