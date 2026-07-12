cmake_minimum_required(VERSION 3.24)

project(MazEngine
    VERSION 0.1.0
    DESCRIPTION "Maz Engine — a native C++/Vulkan/SDL3 2D/3D game engine"
    LANGUAGES CXX)

# ---- Global settings --------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Put all binaries in one place so the sandbox can find compiled shaders next to it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Export compile_commands.json for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(MAZ_WERROR "Treat warnings as errors" ON)
option(MAZ_BUILD_TESTS "Build tests" ON)

# Shared warning flags applied to first-party targets only (not fetched deps).
add_library(maz_warnings INTERFACE)
if(MSVC)
    target_compile_options(maz_warnings INTERFACE /W4 $<$<BOOL:${MAZ_WERROR}>:/WX>)
else()
    target_compile_options(maz_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter
        $<$<BOOL:${MAZ_WERROR}>:-Werror>)
endif()

include(Dependencies)
include(CompileShaders)

add_subdirectory(engine)
add_subdirectory(apps/sandbox)
add_subdirectory(apps/orbs)
add_subdirectory(apps/swarm)
add_subdirectory(apps/cube)
add_subdirectory(apps/scene3d)
add_subdirectory(apps/world)
add_subdirectory(apps/model)
add_subdirectory(apps/village)
add_subdirectory(apps/water)
add_subdirectory(apps/instances)
add_subdirectory(apps/glass)
add_subdirectory(apps/maze)
add_subdirectory(apps/crowd)
add_subdirectory(apps/tween)
add_subdirectory(apps/menu)
add_subdirectory(apps/persist)
add_subdirectory(apps/guard)
add_subdirectory(apps/sprites)
add_subdirectory(apps/events)
add_subdirectory(apps/jobs)
add_subdirectory(apps/assetcache)
add_subdirectory(apps/skeleton)
add_subdirectory(apps/animclip)
add_subdirectory(apps/physics)
add_subdirectory(apps/animator)
add_subdirectory(apps/behavior)
add_subdirectory(apps/boxes)
add_subdirectory(apps/scenes)
add_subdirectory(apps/catcher)
add_subdirectory(apps/data)
add_subdirectory(apps/level)
add_subdirectory(apps/config)
add_subdirectory(apps/profiler)
add_subdirectory(apps/ecsave)
add_subdirectory(apps/actions)
add_subdirectory(apps/solar)
add_subdirectory(apps/camera)
add_subdirectory(apps/fireworks)
add_subdirectory(apps/scatter)
add_subdirectory(apps/noise)
add_subdirectory(apps/uilayout)

if(MAZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Maz Engine ${PROJECT_VERSION} configured (build type: ${CMAKE_BUILD_TYPE})")
