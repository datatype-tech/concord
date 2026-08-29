# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT (CONCORD_BUILD_TESTS OR CONCORD_BUILD_GPU_TESTS))
    return()
endif()

enable_testing()
set(CONCORD_TEST_INCLUDE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

include("${CMAKE_CURRENT_LIST_DIR}/ConcordCpuTests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ConcordVulkanTests.cmake")

if(CONCORD_BUILD_GPU_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/ConcordGpuTests.cmake")
endif()
