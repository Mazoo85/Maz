add_executable(jobs main.cpp)

target_link_libraries(jobs PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
