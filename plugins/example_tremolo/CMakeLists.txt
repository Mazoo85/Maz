# Example native audio plugin built as a shared module the host loads with dlopen.
add_library(maz_example_tremolo MODULE tremolo.c)
target_include_directories(maz_example_tremolo PRIVATE ${CMAKE_SOURCE_DIR}/engine/include)
set_target_properties(maz_example_tremolo PROPERTIES PREFIX "" C_STANDARD 11)
if(NOT MSVC)
    target_link_libraries(maz_example_tremolo PRIVATE m)
endif()
