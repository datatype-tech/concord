# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Builds Jolt as a static library and links it privately into Runtime.
# Public headers never include Jolt; only PhysicsWorld.cpp does.

set(CONCORD_JOLT_ROOT "${CONCORD_3RD_DIR}/Jolt")
if(NOT EXISTS "${CONCORD_JOLT_ROOT}/Build/CMakeLists.txt")
    message(FATAL_ERROR
        "concord: Jolt Physics is missing at ${CONCORD_JOLT_ROOT}. "
        "Run setup_deps.ps1 to vendor it.")
endif()

set(USE_ASSERTS OFF CACHE BOOL "" FORCE)
set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)

add_subdirectory("${CONCORD_JOLT_ROOT}/Build" "${CMAKE_BINARY_DIR}/Jolt" EXCLUDE_FROM_ALL)

# Jolt is an implementation detail of Runtime. Keep its include path off
# every consumer so <Jolt/Jolt.h> cannot leak through Concord/CPhysics.h.
if(TARGET Jolt)
    set_property(TARGET Jolt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
