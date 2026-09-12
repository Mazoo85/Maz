add_executable(water main.cpp)

target_link_libraries(water PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
