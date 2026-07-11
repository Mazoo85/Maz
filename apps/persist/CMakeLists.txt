add_executable(persist main.cpp)

target_link_libraries(persist PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
