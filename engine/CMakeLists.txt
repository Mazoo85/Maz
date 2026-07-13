# Always-built non-GPU core: logging, config, and camera math. Depends only on
# GLM (header-only), so it builds without Vulkan/SDL and backs the unit tests.
add_library(maz_core STATIC
    src/core/Log.cpp
    src/core/Config.cpp
    src/scene/Camera.cpp)
target_include_directories(maz_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(maz_core PUBLIC glm::glm PRIVATE maz_warnings)
target_compile_definitions(maz_core PUBLIC $<$<CONFIG:Debug>:MAZ_DEBUG=1>)

if(NOT MAZ_CORE_ONLY)
    # Vendored cgltf (glTF 2.0 parser) as its own target so its implementation compiles
    # with relaxed warnings, independent of the engine's -Werror first-party flags.
    add_library(maz_cgltf STATIC third_party/cgltf/cgltf_impl.c)
    target_include_directories(maz_cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf)
    if(NOT MSVC)
        target_compile_options(maz_cgltf PRIVATE -w)
    endif()

    add_library(maz STATIC
        src/core/Time.cpp
        src/assets/Model.cpp
        src/platform/Window.cpp
        src/platform/Input.cpp
        src/render/VulkanContext.cpp
        src/render/VulkanSwapchain.cpp
        src/render/VulkanRenderer.cpp
        src/render/MeshRenderer.cpp)

    target_include_directories(maz
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_link_libraries(maz
        PUBLIC
            maz_core
            SDL3::SDL3
            glm::glm
            Vulkan::Vulkan
        PRIVATE
            maz_cgltf
            maz_warnings)

    # Enable validation-layer plumbing in debug builds.
    target_compile_definitions(maz PUBLIC $<$<CONFIG:Debug>:MAZ_DEBUG=1>)

    add_library(maz::maz ALIAS maz)
endif()
