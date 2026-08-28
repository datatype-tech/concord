# Locates the prebuilt libraries vendored under concord/lib and exposes them
# as imported targets. Import library names differ between MSVC and MinGW, so
# each dependency probes the candidates it may ship under.

function(concord_find_implib outVar)
    foreach(candidate IN LISTS ARGN)
        if(EXISTS "${CONCORD_LIB_DIR}/${candidate}")
            set(${outVar} "${CONCORD_LIB_DIR}/${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${outVar} "" PARENT_SCOPE)
endfunction()

concord_find_implib(CONCORD_SDL3_IMPLIB SDL3.lib libSDL3.dll.a SDL3.dll.a)
if(NOT CONCORD_SDL3_IMPLIB)
    message(FATAL_ERROR "concord: SDL3 import library not found in ${CONCORD_LIB_DIR}")
endif()

add_library(SDL3::SDL3 SHARED IMPORTED)
set_target_properties(SDL3::SDL3 PROPERTIES
    IMPORTED_IMPLIB   "${CONCORD_SDL3_IMPLIB}"
    IMPORTED_LOCATION "${CONCORD_LIB_DIR}/SDL3.dll")

concord_find_implib(CONCORD_VULKAN_IMPLIB vulkan-1.lib libvulkan-1.dll.a)
if(NOT CONCORD_VULKAN_IMPLIB)
    message(FATAL_ERROR "concord: Vulkan import library not found in ${CONCORD_LIB_DIR}")
endif()

add_library(Vulkan::Vulkan UNKNOWN IMPORTED)
set_target_properties(Vulkan::Vulkan PROPERTIES
    IMPORTED_LOCATION "${CONCORD_VULKAN_IMPLIB}")

# Copies every DLL a consumer's executable needs beside it, so it can be
# launched straight from the build tree: the engine's own two DLLs plus the
# third-party runtimes they depend on. Engine DLLs are copied via their
# target file so this always matches OUTPUT_NAME, even if that changes.
function(concord_stage_runtime target)
    foreach(engine_target concord_runtime concord_render)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${engine_target}>" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Stage $<TARGET_FILE_NAME:${engine_target}> beside ${target}")
    endforeach()

    foreach(dll SDL3.dll phonon.dll)
        if(EXISTS "${CONCORD_LIB_DIR}/${dll}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CONCORD_LIB_DIR}/${dll}" "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Stage ${dll} beside ${target}")
        endif()
    endforeach()

    if(COMMAND concord_stage_shaders)
        concord_stage_shaders(${target})
    endif()
endfunction()
