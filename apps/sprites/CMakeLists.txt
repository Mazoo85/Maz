add_executable(sprites main.cpp)

target_link_libraries(sprites PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
