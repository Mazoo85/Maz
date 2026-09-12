add_executable(form main.cpp)

target_link_libraries(form PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
