add_executable(assetcache main.cpp)

target_link_libraries(assetcache PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target.
