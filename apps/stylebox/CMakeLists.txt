add_executable(stylebox main.cpp)

target_link_libraries(stylebox PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
