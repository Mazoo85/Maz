add_library(maz STATIC
    src/core/Log.cpp
    src/core/Time.cpp
    src/core/Config.cpp
    src/core/KeyValueStore.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/platform/Paths.cpp
    src/render/VulkanContext.cpp
    src/render/VulkanSwapchain.cpp
    src/render/TextureStore.cpp
    src/render/VulkanBuffer.cpp
    src/render/VulkanTexture.cpp
    src/render/SpriteRenderer.cpp
    src/render/MeshRenderer.cpp
    src/render/Particles3D.cpp
    src/render/PostProcess.cpp
    src/render/Shapes.cpp
    src/render/Model.cpp
    src/render/VulkanRenderer.cpp
    src/game/Tilemap.cpp
    src/ui/Font.cpp
    src/ui/DebugOverlay.cpp
    src/audio/Audio.cpp
    src/fx/Particles.cpp)

target_include_directories(maz
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(maz
    PUBLIC
        SDL3::SDL3
        glm::glm
        Vulkan::Vulkan
    PRIVATE
        stb
        cgltf
        maz_warnings)

# Enable validation-layer plumbing in debug builds.
target_compile_definitions(maz PUBLIC $<$<CONFIG:Debug>:MAZ_DEBUG=1>)

# Stage runtime assets next to the executables (bin/) so every app that links maz can load them
# via a relative path: shaders compiled to SPIR-V, and the bundled HUD font.
maz_compile_shaders(maz
    SOURCES
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/fullscreen.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/sprite.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/mesh.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/mesh.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/shadow.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/shadow.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/sky.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/sky.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/composite.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/composite.frag
        ${CMAKE_SOURCE_DIR}/engine/shaders/particle.vert
        ${CMAKE_SOURCE_DIR}/engine/shaders/particle.frag)

add_custom_command(TARGET maz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/fonts/DejaVuSans.ttf
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/models/house.gltf
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/house.gltf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/models/village.gltf
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/village.gltf
    COMMENT "Staging font + model assets into bin/assets")

add_library(maz::maz ALIAS maz)
