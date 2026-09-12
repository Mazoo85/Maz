add_executable(blendspace main.cpp)

target_link_libraries(blendspace PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
