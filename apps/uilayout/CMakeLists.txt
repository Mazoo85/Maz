add_executable(uilayout main.cpp)

target_link_libraries(uilayout PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
