add_executable(blackboard main.cpp)

target_link_libraries(blackboard PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
