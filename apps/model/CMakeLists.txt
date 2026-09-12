add_executable(model main.cpp)

target_link_libraries(model PRIVATE maz::maz maz_warnings)

# Shaders, the font, and house.gltf are staged into bin/ by the engine target.
