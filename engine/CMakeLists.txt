# Vendored cgltf (glTF 2.0 parser) as its own target so its implementation compiles
# with relaxed warnings, independent of the engine's -Werror first-party flags.
add_library(maz_cgltf STATIC third_party/cgltf/cgltf_impl.c)
target_include_directories(maz_cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf)
if(NOT MSVC)
    target_compile_options(maz_cgltf PRIVATE -w)
endif()

# Vendored Dear ImGui (docking) + its SDL3 and Vulkan backends, for the editor UI. Compiled
# with relaxed warnings for the same reason as cgltf.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(maz_imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(maz_imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(maz_imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)
if(NOT MSVC)
    target_compile_options(maz_imgui PRIVATE -w)
endif()

add_library(maz STATIC
    src/core/Log.cpp
    src/core/Time.cpp
    src/core/Config.cpp
    src/assets/Model.cpp
    src/scene/Camera.cpp
    src/scene/Scene.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/render/VulkanContext.cpp
    src/render/VulkanSwapchain.cpp
    src/render/VulkanRenderer.cpp
    src/render/MeshRenderer.cpp
    src/render/OffscreenRenderer.cpp)

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
        maz_cgltf
        maz_imgui
        maz_warnings)

# Enable validation-layer plumbing in debug builds.
target_compile_definitions(maz PUBLIC $<$<CONFIG:Debug>:MAZ_DEBUG=1>)

add_library(maz::maz ALIAS maz)
