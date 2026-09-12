add_executable(statemachine main.cpp)

target_link_libraries(statemachine PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
