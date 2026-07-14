add_executable(polyline main.cpp)

target_link_libraries(polyline PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
