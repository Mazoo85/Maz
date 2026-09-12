add_executable(level main.cpp)

target_link_libraries(level PRIVATE maz::maz maz_warnings)

# Shaders, the font, and level JSON are staged into bin/ by the engine target.
