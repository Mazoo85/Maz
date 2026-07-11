# maz_compile_shaders(<target> OUTPUT_DIR <dir> SOURCES <a.vert> <b.frag> ...)
#
# Compiles each GLSL source to <OUTPUT_DIR>/<name>.spv using glslangValidator and attaches the
# results to <target> as a build dependency. The SPIR-V lands next to the executable so it can
# be loaded at runtime with a relative "shaders/<name>.spv" path.
function(maz_compile_shaders TARGET)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR" "SOURCES" ${ARGN})
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
    endif()

    set(spv_outputs "")
    foreach(src ${ARG_SOURCES})
        get_filename_component(fname "${src}" NAME)
        set(out "${ARG_OUTPUT_DIR}/${fname}.spv")
        add_custom_command(
            OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_OUTPUT_DIR}"
            COMMAND "${MAZ_GLSLANG_VALIDATOR}" -V "${src}" -o "${out}"
            DEPENDS "${src}"
            COMMENT "Compiling shader ${fname} -> ${fname}.spv"
            VERBATIM)
        list(APPEND spv_outputs "${out}")
    endforeach()

    add_custom_target(${TARGET}_shaders DEPENDS ${spv_outputs})
    add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()
