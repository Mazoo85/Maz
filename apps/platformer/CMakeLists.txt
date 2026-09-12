add_executable(platformer main.cpp)

target_link_libraries(platformer PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target (see engine/CMakeLists.txt).
