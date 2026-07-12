add_executable(sequencer main.cpp)

target_link_libraries(sequencer PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
