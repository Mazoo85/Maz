cmake_minimum_required(VERSION 3.24)

project(MazEngine
    VERSION 0.1.0
    DESCRIPTION "Maz Engine — a native C++/Vulkan/SDL3 2D/3D game engine"
    LANGUAGES C CXX) # C is needed for the vendored cgltf implementation (third_party/cgltf)

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
option(MAZ_CORE_ONLY "Build only the non-GPU core + unit tests (no Vulkan/SDL/sandbox)" OFF)

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
if(NOT MAZ_CORE_ONLY)
    add_subdirectory(apps/sandbox)
endif()

if(MAZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Maz Engine ${PROJECT_VERSION} configured (build type: ${CMAKE_BUILD_TYPE})")
