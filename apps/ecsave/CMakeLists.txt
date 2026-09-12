add_executable(ecsave main.cpp)

target_link_libraries(ecsave PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
