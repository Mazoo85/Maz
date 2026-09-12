add_executable(inifile main.cpp)

target_link_libraries(inifile PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
