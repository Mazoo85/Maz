add_executable(itemlist main.cpp)

target_link_libraries(itemlist PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
