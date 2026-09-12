add_executable(contacts main.cpp)

target_link_libraries(contacts PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
