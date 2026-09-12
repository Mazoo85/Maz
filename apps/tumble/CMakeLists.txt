add_executable(tumble main.cpp)

target_link_libraries(tumble PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
