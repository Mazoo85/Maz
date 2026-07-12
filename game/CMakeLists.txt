# ZOMBOID: ANCHORAGE — native gameplay simulation ported onto Maz.
#
# Deliberately dependency-free (no SDL3/Vulkan/GLM): the deterministic sim runs
# and is unit-tested headless anywhere. The render/audio bridge to the engine
# lives in the app layer, not here.
add_library(zomboid STATIC
    src/World.cpp
    src/Items.cpp
    src/Sim.cpp
    src/SoftRenderer.cpp)

target_include_directories(zomboid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(zomboid PRIVATE maz_warnings)

add_library(maz::zomboid ALIAS zomboid)
