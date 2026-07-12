add_executable(vectors main.cpp)

target_link_libraries(vectors PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
