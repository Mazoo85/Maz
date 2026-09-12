add_executable(adventure main.cpp)

target_link_libraries(adventure PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target (see engine/CMakeLists.txt).
