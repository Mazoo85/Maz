add_executable(manybody main.cpp)

target_link_libraries(manybody PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
