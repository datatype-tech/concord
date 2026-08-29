# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Expands caller-owned shader build options into compiler-specific arguments.
# Each list item remains one process argument; add_custom_command(VERBATIM)
# performs the platform quoting needed for spaces and shell metacharacters.
function(concord_shader_extra_arguments kind defines include_dirs options out_arguments)
    set(arguments)
    foreach(define IN LISTS defines)
        if(define STREQUAL "")
            message(FATAL_ERROR "Concord shader DEFINES entries cannot be empty")
        endif()
        if(kind STREQUAL glslang)
            list(APPEND arguments "--define-macro" "${define}")
        else()
            list(APPEND arguments "-D${define}")
        endif()
    endforeach()
    foreach(include_dir IN LISTS include_dirs)
        if(include_dir STREQUAL "")
            message(FATAL_ERROR "Concord shader INCLUDE_DIRS entries cannot be empty")
        endif()
        list(APPEND arguments "-I${include_dir}")
    endforeach()
    foreach(option IN LISTS options)
        if(option STREQUAL "")
            message(FATAL_ERROR "Concord shader OPTIONS entries cannot be empty")
        endif()
        list(APPEND arguments "${option}")
    endforeach()
    set(${out_arguments} "${arguments}" PARENT_SCOPE)
endfunction()
