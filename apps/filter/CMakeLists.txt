add_executable(filter main.cpp)

target_link_libraries(filter PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
