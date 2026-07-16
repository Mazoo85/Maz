add_executable(daw main.cpp)

target_link_libraries(daw PRIVATE maz::maz maz_imgui maz_warnings)

# The DAW draws only an ImGui overlay (no meshes of its own), but the Vulkan renderer still expects
# the shared compiled shaders next to the binary. Reuse the sandbox's shader build rather than
# compiling the same outputs twice.
add_dependencies(daw sandbox_shaders)
