# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Prebuilt Windows x64 MinGW SDK. Source builds continue to use add_subdirectory.
if(TARGET concord::concord)
    return()
endif()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MINGW)
    message(FATAL_ERROR "This preview SDK requires Windows x64 MinGW; use a source build for other toolchains")
endif()
get_filename_component(CONCORD_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
foreach(part runtime render)
    if(part STREQUAL runtime)
        set(dll ConcordFlashGameEngineRuntime)
    else()
        set(dll ConcordFlashGameEngineRender)
    endif()
    add_library(concord::${part} SHARED IMPORTED)
    set_target_properties(concord::${part} PROPERTIES
        IMPORTED_LOCATION "${CONCORD_SDK_ROOT}/bin/${dll}.dll"
        IMPORTED_IMPLIB "${CONCORD_SDK_ROOT}/lib/${dll}.dll.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CONCORD_SDK_ROOT}/include"
        INTERFACE_COMPILE_DEFINITIONS CONCORD_SHARED
        INTERFACE_COMPILE_FEATURES cxx_std_23)
endforeach()
set_property(TARGET concord::render PROPERTY INTERFACE_LINK_LIBRARIES concord::runtime)
set_property(TARGET concord::render PROPERTY INTERFACE_LINK_OPTIONS "-Wl,-u,ConcordRenderBackendLinkAnchor")
add_library(concord::concord INTERFACE IMPORTED)
set_property(TARGET concord::concord PROPERTY INTERFACE_LINK_LIBRARIES "concord::runtime;concord::render")

function(concord_stage_runtime target)
    get_target_property(runtime concord::runtime IMPORTED_LOCATION)
    get_filename_component(runtime_dir "${runtime}" DIRECTORY)
    file(GLOB runtime_dlls "${runtime_dir}/*.dll")
    foreach(dll IN LISTS runtime_dlls)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:${target}>" VERBATIM)
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${runtime_dir}/Assets/Shaders"
            "$<TARGET_FILE_DIR:${target}>/Assets/Shaders" VERBATIM)
endfunction()
