add_executable(village main.cpp)

target_link_libraries(village PRIVATE maz::maz maz_warnings)

# Shaders, the font, and village.gltf are staged into bin/ by the engine target.
