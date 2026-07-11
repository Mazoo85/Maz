add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target (see engine/CMakeLists.txt),
# so all apps share them.
