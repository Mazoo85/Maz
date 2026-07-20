# Example VST3 *instrument* plugin, built as a shared module (.vst3) the Vst3Host loads with dlopen.
# The instrument counterpart to example_vst3 (tremolo): it declares an event/note input bus and
# synthesises audio from note events. Written against the MIT-licensed VST3 pluginterfaces only
# (no GPL public.sdk).
add_library(maz_example_vst3_instrument MODULE synth_vst3.cpp)
target_include_directories(maz_example_vst3_instrument PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/third_party/vst3)
# VST3 modules are conventionally named .vst3; the host loads whatever path it is given. Hide symbols
# by default so only the SMTG_EXPORT_SYMBOL entry points are visible, like a real plugin.
set_target_properties(maz_example_vst3_instrument PROPERTIES
    PREFIX "" SUFFIX ".vst3" CXX_STANDARD 17
    CXX_VISIBILITY_PRESET hidden)
