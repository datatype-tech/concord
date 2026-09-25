cmake_minimum_required(VERSION 3.24)

# Host project for Concord Flash.
#
# The engine is entirely self-contained under concord/ (headers, sources,
# vendored third-party code and prebuilt libraries alike). This file only
# registers it and builds the application that consumes it.

project(ConcordFlashHost
    VERSION 0.1.0
    DESCRIPTION "Host application for the Concord Flash engine"
    LANGUAGES C CXX
)

option(CONCORD_BUILD_TESTS "Build Concord's non-GPU regression tests" OFF)
option(CONCORD_BUILD_GPU_TESTS "Build Concord's Vulkan validation tests" OFF)
if(CONCORD_BUILD_TESTS OR CONCORD_BUILD_GPU_TESTS)
    enable_testing()
endif()

add_subdirectory(concord)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE concord::concord)

concord_stage_runtime(main)
