# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Stages generated SPIR-V outputs beside a consuming executable.
function(concord_stage_shader_outputs target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "OUTPUTS")
    foreach(output IN LISTS ARG_OUTPUTS)
        get_filename_component(name "${output}" NAME)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${target}>/Assets/Shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${output}" "$<TARGET_FILE_DIR:${target}>/Assets/Shaders/${name}"
            COMMENT "Stage ConcordScript shader ${name}"
            VERBATIM)
    endforeach()
endfunction()
