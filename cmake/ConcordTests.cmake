# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if(NOT (CONCORD_BUILD_TESTS OR CONCORD_BUILD_GPU_TESTS))
    return()
endif()

enable_testing()
set(CONCORD_TEST_INCLUDE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

add_executable(concord_ecs_tests
    tests/EcsTests.cpp tests/EcsWorldTests.cpp tests/EcsQueryTests.cpp
    tests/EcsDeferredTests.cpp tests/EcsSceneTests.cpp)
target_compile_features(concord_ecs_tests PRIVATE cxx_std_23)
target_include_directories(concord_ecs_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_ecs_tests COMMAND concord_ecs_tests)

add_executable(concord_asset_tests
    tests/AssetTests.cpp
    ${CONCORD_ASSET_SOURCES})
target_compile_features(concord_asset_tests PRIVATE cxx_std_23)
target_include_directories(concord_asset_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_asset_tests COMMAND concord_asset_tests)

add_executable(concord_render_snapshot_tests
    tests/RenderSceneSnapshotTests.cpp src/engine/render/RenderSceneSnapshot.cpp)
target_compile_features(concord_render_snapshot_tests PRIVATE cxx_std_23)
target_include_directories(concord_render_snapshot_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_render_snapshot_tests COMMAND concord_render_snapshot_tests)

add_executable(concord_render_frame_data_tests
    tests/RenderFrameDataTests.cpp src/engine/render/RenderFrameData.cpp
    src/engine/render/RenderSceneSnapshot.cpp)
target_compile_features(concord_render_frame_data_tests PRIVATE cxx_std_23)
target_include_directories(concord_render_frame_data_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_render_frame_data_tests COMMAND concord_render_frame_data_tests)

add_executable(concord_tile_light_tests tests/VulkanTileLightTests.cpp)
target_compile_features(concord_tile_light_tests PRIVATE cxx_std_23)
target_include_directories(concord_tile_light_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_tile_light_tests COMMAND concord_tile_light_tests)

add_executable(concord_shadow_tests tests/VulkanShadowTests.cpp
    src/engine/render/vulkan/VulkanShadowMath.cpp)
target_compile_features(concord_shadow_tests PRIVATE cxx_std_23)
target_include_directories(concord_shadow_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_shadow_tests COMMAND concord_shadow_tests)

add_executable(concord_ray_tracing_tests tests/VulkanRayTracingTests.cpp
    src/engine/render/vulkan/VulkanRayTracingSupport.cpp)
target_compile_features(concord_ray_tracing_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_tests COMMAND concord_ray_tracing_tests)
set_tests_properties(concord_ray_tracing_tests PROPERTIES SKIP_RETURN_CODE 77)

add_executable(concord_ray_tracing_layout_tests tests/VulkanRayTracingLayoutTests.cpp
    src/engine/render/vulkan/VulkanRayTracingPipeline.cpp)
target_compile_features(concord_ray_tracing_layout_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_layout_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_ray_tracing_layout_tests COMMAND concord_ray_tracing_layout_tests)

add_executable(concord_ray_tracing_scene_tests tests/VulkanRayTracingSceneTests.cpp
    src/engine/render/vulkan/VulkanRayTracingScene.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneGeometry.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneBottomLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneTopLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRecord.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneDescriptor.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_ray_tracing_scene_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_scene_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_scene_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_scene_tests COMMAND concord_ray_tracing_scene_tests)

add_executable(concord_ray_tracing_scene_ring_tests tests/VulkanRayTracingSceneRingTests.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRing.cpp
    src/engine/render/vulkan/VulkanRayTracingScene.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneGeometry.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneBottomLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneTopLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRecord.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneDescriptor.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_ray_tracing_scene_ring_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_scene_ring_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_scene_ring_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_scene_ring_tests COMMAND concord_ray_tracing_scene_ring_tests)

add_executable(concord_vulkan_buffer_tests tests/VulkanBufferTests.cpp)
target_compile_features(concord_vulkan_buffer_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_buffer_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_vulkan_buffer_tests COMMAND concord_vulkan_buffer_tests)

add_executable(concord_system_tests tests/SystemScheduleTests.cpp)
target_compile_features(concord_system_tests PRIVATE cxx_std_23)
target_link_libraries(concord_system_tests PRIVATE concord::runtime)
concord_stage_runtime(concord_system_tests)
add_test(NAME concord_system_tests COMMAND concord_system_tests)

add_executable(concord_window_desc_tests tests/WindowDescTests.cpp)
target_compile_features(concord_window_desc_tests PRIVATE cxx_std_23)
target_link_libraries(concord_window_desc_tests PRIVATE concord::runtime)
concord_stage_runtime(concord_window_desc_tests)
add_test(NAME concord_window_desc_tests COMMAND concord_window_desc_tests)

add_executable(concord_vulkan_pass_registry_tests tests/VulkanPassRegistryTests.cpp)
target_compile_features(concord_vulkan_pass_registry_tests PRIVATE cxx_std_23)
target_link_libraries(concord_vulkan_pass_registry_tests PRIVATE concord::runtime)
concord_stage_runtime(concord_vulkan_pass_registry_tests)
add_test(NAME concord_vulkan_pass_registry_tests COMMAND concord_vulkan_pass_registry_tests)

add_test(NAME concord_shader_option_arguments
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/ShaderOptionsScript.cmake)

add_executable(concord_vulkan_pass_context_tests
    tests/VulkanPassContextAdapterTests.cpp
    src/engine/render/VulkanRenderBackendExtensions.cpp
    src/engine/render/VulkanPassRegistry.cpp)
target_compile_features(concord_vulkan_pass_context_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_pass_context_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_vulkan_pass_context_tests COMMAND concord_vulkan_pass_context_tests)

if(CONCORD_BUILD_GPU_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/ConcordGpuTests.cmake")
endif()
