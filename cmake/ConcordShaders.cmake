# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Optional GLSL -> SPIR-V compilation.  The engine does not require a shader
# compiler until a pipeline consumes these artifacts, so a missing tool only
# disables this target and never prevents the normal build.

option(CONCORD_BUILD_SHADERS
    "Compile bundled Vulkan GLSL shaders when a compiler is available" OFF)
set(CONCORD_SHADER_COMPILER "" CACHE FILEPATH
    "Path to glslc, glslangValidator, or dxc (auto-detected when empty)")
set(CONCORD_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders" CACHE PATH
    "Deterministic output directory for compiled SPIR-V shaders")

set(CONCORD_SHADER_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../assets/shaders" CACHE INTERNAL
    "Bundled Concord GLSL source directory")
set(CONCORD_SHADER_SOURCES
    "${CONCORD_SHADER_SOURCE_DIR}/mesh.vert"
    "${CONCORD_SHADER_SOURCE_DIR}/model.vert"
    "${CONCORD_SHADER_SOURCE_DIR}/skinned.vert"
    "${CONCORD_SHADER_SOURCE_DIR}/solid.frag"
    "${CONCORD_SHADER_SOURCE_DIR}/skinned.frag"
    "${CONCORD_SHADER_SOURCE_DIR}/solid_shadow.frag"
    "${CONCORD_SHADER_SOURCE_DIR}/solid_rayquery.frag"
    "${CONCORD_SHADER_SOURCE_DIR}/tile_cull.comp"
    "${CONCORD_SHADER_SOURCE_DIR}/directional_shadow.vert"
    "${CONCORD_SHADER_SOURCE_DIR}/raygen.rgen"
    "${CONCORD_SHADER_SOURCE_DIR}/raymiss.rmiss"
    "${CONCORD_SHADER_SOURCE_DIR}/rayhit.rchit" CACHE INTERNAL
    "Bundled Concord GLSL sources" FORCE)

function(concord_configure_shaders)
    if(NOT CONCORD_BUILD_SHADERS)
        add_custom_target(concord_shaders)
        message(STATUS "Concord shaders: disabled (CONCORD_BUILD_SHADERS=OFF)")
        set(CONCORD_SHADERS_AVAILABLE FALSE CACHE INTERNAL "" FORCE)
        set(CONCORD_SHADER_OUTPUTS "" CACHE INTERNAL "" FORCE)
        return()
    endif()

    concord_resolve_shader_compiler(compiler compiler_kind glsl)
    if(NOT compiler)
        add_custom_target(concord_shaders)
        message(WARNING
            "Concord shaders requested, but no supported shader compiler was found; "
            "skipping compilation")
        set(CONCORD_SHADERS_AVAILABLE FALSE CACHE INTERNAL "" FORCE)
        set(CONCORD_SHADER_OUTPUTS "" CACHE INTERNAL "" FORCE)
        return()
    endif()
    if(compiler_kind STREQUAL dxc)
        add_custom_target(concord_shaders)
        message(WARNING
            "Concord bundled shaders are GLSL; dxc only supports HLSL, skipping")
        set(CONCORD_SHADERS_AVAILABLE FALSE CACHE INTERNAL "" FORCE)
        set(CONCORD_SHADER_OUTPUTS "" CACHE INTERNAL "" FORCE)
        return()
    endif()

    set(outputs)
    foreach(source IN LISTS CONCORD_SHADER_SOURCES)
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "Concord shader source missing: ${source}")
        endif()
        get_filename_component(stage_ext "${source}" EXT)
        string(REGEX REPLACE "^\\." "" stage "${stage_ext}")
        concord_normalize_shader_stage("${stage}" stage)
        get_filename_component(stem "${source}" NAME)
        set(output "${CONCORD_SHADER_OUTPUT_DIR}/${stem}.spv")
        concord_shader_arguments(
            "${compiler_kind}" "${stage}" main glsl arguments output_flag)
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CONCORD_SHADER_OUTPUT_DIR}"
            COMMAND "${compiler}" ${arguments} "${output_flag}" "${output}" "${source}"
            DEPENDS "${source}"
            COMMENT "Compile Vulkan shader ${stem}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()
    add_custom_target(concord_shaders DEPENDS ${outputs})
    set(CONCORD_SHADERS_AVAILABLE TRUE CACHE INTERNAL "" FORCE)
    set(CONCORD_SHADER_OUTPUTS "${outputs}" CACHE INTERNAL "" FORCE)
    message(STATUS "Concord shaders: ${compiler} -> ${CONCORD_SHADER_OUTPUT_DIR}")
endfunction()

function(concord_stage_shaders target)
    if(NOT CONCORD_SHADERS_AVAILABLE)
        return()
    endif()
    add_dependencies(${target} concord_shaders)
    foreach(output IN LISTS CONCORD_SHADER_OUTPUTS)
        get_filename_component(name "${output}" NAME)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${target}>/Assets/Shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${output}" "$<TARGET_FILE_DIR:${target}>/Assets/Shaders/${name}"
            COMMENT "Stage ${name} beside ${target}"
            VERBATIM)
    endforeach()
endfunction()

function(concord_attach_shaders target)
    if(TARGET concord_shaders)
        add_dependencies(${target} concord_shaders)
    endif()
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/ConcordShaderSources.cmake")
