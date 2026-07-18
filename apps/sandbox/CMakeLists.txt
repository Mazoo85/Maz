add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE maz::maz maz_warnings)

# Compile the demo shaders next to the executable (bin/shaders/*.spv).
maz_compile_shaders(sandbox
    SOURCES
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/mesh.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/mesh.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.frag)
