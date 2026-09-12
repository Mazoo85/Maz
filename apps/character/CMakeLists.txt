add_executable(character main.cpp)

target_link_libraries(character PRIVATE maz::maz maz_warnings)

# SENTINEL — a 3D character authored entirely in code (character.hpp) from the engine's procedural
# mesh primitives, drawn with the Vulkan PBR path. No external model file. Shaders and the font are
# staged into bin/ by the engine target (see engine/CMakeLists.txt).
