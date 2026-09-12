add_executable(navmesh main.cpp)

target_link_libraries(navmesh PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
