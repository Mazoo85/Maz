# Example CLAP plugin, built as a shared module (.clap) the ClapHost loads with dlopen.
add_library(maz_example_clap MODULE tremolo_clap.c)
target_include_directories(maz_example_clap PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/third_party/clap/include)
# CLAP plugins are conventionally named .clap; the host loads whatever path it is given.
set_target_properties(maz_example_clap PROPERTIES PREFIX "" SUFFIX ".clap" C_STANDARD 11)
if(NOT MSVC)
    target_link_libraries(maz_example_clap PRIVATE m)
endif()
