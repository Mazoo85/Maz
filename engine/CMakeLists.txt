add_library(maz STATIC
    src/core/Log.cpp
    src/core/Time.cpp
    src/core/Config.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/render/VulkanContext.cpp
    src/render/VulkanSwapchain.cpp
    src/render/VulkanBuffer.cpp
    src/render/VulkanTexture.cpp
    src/render/SpriteRenderer.cpp
    src/render/VulkanRenderer.cpp
    src/game/Tilemap.cpp)

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
        maz_warnings)

# Enable validation-layer plumbing in debug builds.
target_compile_definitions(maz PUBLIC $<$<CONFIG:Debug>:MAZ_DEBUG=1>)

add_library(maz::maz ALIAS maz)
