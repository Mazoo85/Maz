add_executable(tileset main.cpp)

target_link_libraries(tileset PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
