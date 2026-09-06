cmake_minimum_required(VERSION 3.24)

# Concord Flash engine.
#
# Self-contained: public headers in include/Concord, module-private headers
# in include/engine, sources in src/engine, vendored third-party headers in
# src/3rd and prebuilt libraries in lib.
#
# Two DLLs, mirroring the first generation's per-module split (see
# AGENTS.md §12):
#   ConcordFlashGameEngineRuntime.dll  lifecycle, ECS, scenes, windowing
#   ConcordFlashGameEngineRender.dll   the Vulkan backend
#
# Runtime never names the concrete Vulkan backend: Render self-registers
# through RenderBackendFactory the moment it is loaded, and a forced link
# anchor (see the target_link_options below) makes sure Windows always
# loads it — so the DLL dependency runs one way only (Render -> Runtime,
# for Window access) even though Runtime is the one that constructs a
# renderer, and application code never has to call anything to wire it up.
#
#   target_link_libraries(<consumer> PRIVATE concord::runtime concord::render)

project(Concord
    VERSION 0.1.0
    DESCRIPTION "Native Forward+ Vulkan 3D engine for Windows"
    LANGUAGES C CXX
)

# Every DLL and its consumer exchange STL-bearing API types.  Keep the CRT
# choice explicit for MSVC so release/debug heaps and iterator settings cannot
# diverge across the Runtime/Render boundary.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "MSVC runtime used by Concord and its consumers")
endif()

option(CONCORD_BUILD_TESTS "Build Concord's non-GPU regression tests" OFF)
option(CONCORD_BUILD_GPU_TESTS "Build the Vulkan validation smoke test" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Cached so the functions this file defines still resolve these paths when a
# parent project calls them from its own scope.
set(CONCORD_3RD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/3rd" CACHE INTERNAL
    "Concord vendored third-party headers")
set(CONCORD_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib" CACHE INTERNAL
    "Concord prebuilt libraries")

include(cmake/ConcordDependencies.cmake)
include(cmake/ConcordSources.cmake)
include(cmake/ConcordShaders.cmake)

concord_configure_shaders()

# Opt-in native Vulkan surface for advanced application code (including
# ConcordScript `@vulkan` blocks). The normal public API still hides Vulkan;
# consumers only receive these headers and the loader import library when they
# explicitly link concord::vulkan_sdk.
add_library(concord_vulkan_sdk INTERFACE)
add_library(concord::vulkan_sdk ALIAS concord_vulkan_sdk)
target_include_directories(concord_vulkan_sdk INTERFACE
    $<BUILD_INTERFACE:${CONCORD_3RD_DIR}/Vulkan>
)
target_link_libraries(concord_vulkan_sdk INTERFACE Vulkan::Vulkan)

# ---------------------------------------------------------------------------
# ConcordFlashGameEngineRuntime.dll
# ---------------------------------------------------------------------------
add_library(concord_runtime SHARED ${CONCORD_RUNTIME_SOURCES})
add_library(concord::runtime ALIAS concord_runtime)

set_target_properties(concord_runtime PROPERTIES
    OUTPUT_NAME "ConcordFlashGameEngineRuntime"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_compile_features(concord_runtime PUBLIC cxx_std_23)
target_compile_definitions(concord_runtime PUBLIC CONCORD_SHARED)
target_compile_definitions(concord_runtime PRIVATE CENGINE_EXPORTS)

target_include_directories(concord_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# SDL3 is a runtime implementation detail: PRIVATE keeps it off the
# consumer's include path, enforcing the no-leak rule in AGENTS.md §5.
target_include_directories(concord_runtime PRIVATE
    ${CONCORD_3RD_DIR}/SDL3
    ${CONCORD_3RD_DIR}/stb
)
target_link_libraries(concord_runtime PRIVATE SDL3::SDL3)

# ---------------------------------------------------------------------------
# ConcordFlashGameEngineRender.dll
# ---------------------------------------------------------------------------
add_library(concord_render SHARED ${CONCORD_RENDER_SOURCES})
add_library(concord::render ALIAS concord_render)

set_target_properties(concord_render PROPERTIES
    OUTPUT_NAME "ConcordFlashGameEngineRender"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_compile_features(concord_render PUBLIC cxx_std_23)
target_compile_definitions(concord_render PUBLIC CONCORD_SHARED)
target_compile_definitions(concord_render PRIVATE CRENDER_EXPORTS)

target_include_directories(concord_render PRIVATE
    ${CONCORD_3RD_DIR}/SDL3
    ${CONCORD_3RD_DIR}/Vulkan
    ${CONCORD_3RD_DIR}/stb
)

# Render depends on Runtime for Window; never the reverse.
target_link_libraries(concord_render PRIVATE concord::runtime SDL3::SDL3 Vulkan::Vulkan)
concord_attach_shaders(concord_render)

# On Windows, an unreferenced import library contributes nothing to a link:
# linking concord::render alone would not make the loader actually pull the
# DLL in at process start, so its self-registering static initializer
# (VulkanBackendRegistration.cpp) would never run. Forcing every consumer to
# resolve ConcordRenderBackendLinkAnchor closes that gap without asking
# application code to call anything — Game::AttachWindow finds a renderer
# automatically the moment concord::concord is linked.
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    target_link_options(concord_render INTERFACE "/INCLUDE:ConcordRenderBackendLinkAnchor")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(concord_render INTERFACE "-Wl,-u,ConcordRenderBackendLinkAnchor")
endif()

# ---------------------------------------------------------------------------
# concord::concord — convenience alias linking both DLLs
# ---------------------------------------------------------------------------
add_library(concord INTERFACE)
add_library(concord::concord ALIAS concord)
target_link_libraries(concord INTERFACE concord::runtime concord::render)

include(cmake/ConcordTests.cmake)
