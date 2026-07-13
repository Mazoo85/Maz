# Dependency resolution for Maz Engine.
#   - Vulkan : found on the system (Vulkan SDK / libvulkan-dev)
#   - SDL3   : fetched + built from source
#   - GLM    : fetched (header-only)
include(FetchContent)

# ---- Vulkan -----------------------------------------------------------------
if(NOT MAZ_CORE_ONLY)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan: ${Vulkan_LIBRARY} (include: ${Vulkan_INCLUDE_DIRS})")

    # glslangValidator is used by CompileShaders.cmake to turn GLSL into SPIR-V.
    # FindVulkan exposes it as Vulkan::glslangValidator when available; fall back to PATH.
    if(TARGET Vulkan::glslangValidator)
        set(MAZ_GLSLANG_VALIDATOR "$<TARGET_FILE:Vulkan::glslangValidator>" CACHE INTERNAL "")
    else()
        find_program(MAZ_GLSLANG_VALIDATOR NAMES glslangValidator)
    endif()
    if(NOT MAZ_GLSLANG_VALIDATOR)
        message(FATAL_ERROR "glslangValidator not found (install glslang-tools or the Vulkan SDK)")
    endif()
endif()

# ---- SDL3 -------------------------------------------------------------------
if(NOT MAZ_CORE_ONLY)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.12
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
endif()

# ---- GLM --------------------------------------------------------------------
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glm)
if(NOT MAZ_CORE_ONLY)
    FetchContent_MakeAvailable(SDL3)
endif()
