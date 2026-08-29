# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/ConcordShaderOptions.cmake")

function(assert_equal actual expected label)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${label}: got '${actual}', expected '${expected}'")
    endif()
endfunction()

set(defines "MAX_LIGHTS=64;USE_SHADOWS=1")
set(include_dirs "C:/Shader Includes;D:/Concord/includes")
set(options "-g;--target-env=vulkan1.3")

concord_shader_extra_arguments(glslang "${defines}" "${include_dirs}" "${options}" glslang_args)
set(glslang_expected
    "--define-macro" "MAX_LIGHTS=64"
    "--define-macro" "USE_SHADOWS=1"
    "-IC:/Shader Includes"
    "-ID:/Concord/includes"
    "-g"
    "--target-env=vulkan1.3")
assert_equal("${glslang_args}" "${glslang_expected}" "glslang arguments")

concord_shader_extra_arguments(glslc "${defines}" "${include_dirs}" "${options}" glslc_args)
set(glslc_expected
    "-DMAX_LIGHTS=64"
    "-DUSE_SHADOWS=1"
    "-IC:/Shader Includes"
    "-ID:/Concord/includes"
    "-g"
    "--target-env=vulkan1.3")
assert_equal("${glslc_args}" "${glslc_expected}" "glslc arguments")

concord_shader_extra_arguments(dxc "FEATURE=1" "C:/HLSL Includes" "-Zi" dxc_args)
set(dxc_expected "-DFEATURE=1" "-IC:/HLSL Includes" "-Zi")
assert_equal("${dxc_args}" "${dxc_expected}" "dxc arguments")
