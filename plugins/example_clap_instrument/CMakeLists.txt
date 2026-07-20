# Example CLAP *instrument* plugin, built as a shared module (.clap) the ClapHost loads with dlopen.
add_library(maz_example_clap_instrument MODULE synth_clap.c)
target_include_directories(maz_example_clap_instrument PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/third_party/clap/include)
set_target_properties(maz_example_clap_instrument PROPERTIES PREFIX "" SUFFIX ".clap" C_STANDARD 11)
if(NOT MSVC)
    target_link_libraries(maz_example_clap_instrument PRIVATE m)
endif()
