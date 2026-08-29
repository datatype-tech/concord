# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Compiles caller-owned shader sources emitted by ConcordScript (or another
# project tool) and stages the resulting SPIR-V beside the executable.

include("${CMAKE_CURRENT_LIST_DIR}/ConcordShaderCompiler.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ConcordShaderOptions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ConcordShaderStaging.cmake")

function(concord_add_shader_sources target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" ""
        "SOURCES;STAGES;ENTRIES;LANGUAGES;DEFINES;INCLUDE_DIRS;OPTIONS")
    if(NOT ARG_SOURCES)
        return()
    endif()
    if(NOT CONCORD_BUILD_SHADERS)
        message(STATUS "ConcordScript shaders: disabled (CONCORD_BUILD_SHADERS=OFF)")
        return()
    endif()

    list(LENGTH ARG_SOURCES source_count)
    list(LENGTH ARG_STAGES stage_count)
    list(LENGTH ARG_ENTRIES entry_count)
    list(LENGTH ARG_LANGUAGES language_count)
    if(stage_count GREATER 0 AND NOT stage_count EQUAL source_count)
        message(FATAL_ERROR "ConcordScript shader SOURCES/STAGES length mismatch")
    endif()
    if(entry_count GREATER 0 AND NOT entry_count EQUAL source_count)
        message(FATAL_ERROR "ConcordScript shader SOURCES/ENTRIES length mismatch")
    endif()
    if(language_count GREATER 0 AND NOT language_count EQUAL source_count)
        message(FATAL_ERROR "ConcordScript shader SOURCES/LANGUAGES length mismatch")
    endif()

    concord_resolve_shader_compiler(compiler compiler_kind ${ARG_LANGUAGES})
    if(NOT compiler)
        message(WARNING
            "ConcordScript shaders requested, but no supported shader compiler was found")
        return()
    endif()

    get_property(call_index GLOBAL PROPERTY CONCORD_SHADER_SOURCE_CALL_INDEX)
    if(NOT call_index)
        set(call_index 0)
    endif()
    math(EXPR call_index "${call_index} + 1")
    set_property(GLOBAL PROPERTY CONCORD_SHADER_SOURCE_CALL_INDEX "${call_index}")

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/concordscript_shaders")
    set(outputs)
    math(EXPR last_index "${source_count} - 1")
    foreach(index RANGE ${last_index})
        list(GET ARG_SOURCES ${index} source)
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "ConcordScript shader source missing: ${source}")
        endif()
        get_filename_component(filename "${source}" NAME)
        if(stage_count GREATER 0)
            list(GET ARG_STAGES ${index} stage)
        else()
            string(REGEX REPLACE "^.*\\.([^.]+)$" "\\1" stage "${filename}")
            string(TOLOWER "${stage}" stage)
            if(stage STREQUAL "hlsl" OR stage STREQUAL "glsl")
                string(REGEX REPLACE "^.*\\.([^.]+)\\.[^.]+$" "\\1" stage "${filename}")
            endif()
        endif()
        concord_normalize_shader_stage("${stage}" stage)
        if(entry_count GREATER 0)
            list(GET ARG_ENTRIES ${index} entry)
        else()
            set(entry main)
        endif()
        if(language_count GREATER 0)
            list(GET ARG_LANGUAGES ${index} language)
        else()
            set(language glsl)
        endif()
        string(TOLOWER "${stage}" stage)
        string(TOLOWER "${language}" language)
        if(entry STREQUAL "")
            set(entry main)
        endif()
        set(hash_input "${target}|${call_index}|${index}|${source}|${stage}|${entry}|${language}")
        string(APPEND hash_input "|${ARG_DEFINES}|${ARG_INCLUDE_DIRS}|${ARG_OPTIONS}")
        string(SHA1 source_hash "${hash_input}")
        string(SUBSTRING "${source_hash}" 0 10 source_id)
        set(output "${output_dir}/${filename}.${source_id}.spv")

        concord_shader_arguments(
            "${compiler_kind}" "${stage}" "${entry}" "${language}" arguments output_flag)
        concord_shader_extra_arguments(
            "${compiler_kind}" "${ARG_DEFINES}" "${ARG_INCLUDE_DIRS}" "${ARG_OPTIONS}"
            extra_arguments)
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND "${compiler}" ${arguments} ${extra_arguments}
                    "${output_flag}" "${output}" "${source}"
            DEPENDS "${source}"
            COMMENT "Compile ConcordScript shader ${filename}"
            VERBATIM)
        list(APPEND outputs "${output}")
    endforeach()

    string(MAKE_C_IDENTIFIER "${target}" target_id)
    set(asset_target "${target_id}_concordscript_shaders_${call_index}")
    add_custom_target("${asset_target}" DEPENDS ${outputs})
    add_dependencies(${target} "${asset_target}")
    concord_stage_shader_outputs(${target} OUTPUTS ${outputs})
endfunction()
