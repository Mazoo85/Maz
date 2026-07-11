add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE maz::maz maz_warnings)

# Compile the demo shaders next to the executable (bin/shaders/*.spv).
maz_compile_shaders(sandbox
    SOURCES
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.frag)

# Copy bundled assets (font) next to the executable so they load via a relative path.
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sandbox>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf
        $<TARGET_FILE_DIR:sandbox>/assets/fonts/DejaVuSans.ttf
    COMMENT "Copying font asset next to sandbox")
