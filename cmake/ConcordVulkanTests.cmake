# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

set(CONCORD_RAY_SCENE_SOURCES
    src/engine/render/vulkan/VulkanRayTracingScene.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneGeometry.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneBottomLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneTopLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRecord.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelBuild.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelBarrier.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelDestroy.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneDescriptor.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)

add_executable(concord_ray_tracing_scene_tests
    tests/VulkanRayTracingSceneTests.cpp ${CONCORD_RAY_SCENE_SOURCES})
target_compile_features(concord_ray_tracing_scene_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_scene_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_scene_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_scene_tests COMMAND concord_ray_tracing_scene_tests)

add_executable(concord_ray_tracing_scene_ring_tests
    tests/VulkanRayTracingSceneRingTests.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRing.cpp
    ${CONCORD_RAY_SCENE_SOURCES})
target_compile_features(concord_ray_tracing_scene_ring_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_scene_ring_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_scene_ring_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_scene_ring_tests COMMAND concord_ray_tracing_scene_ring_tests)

add_executable(concord_ray_tracing_model_tests
    tests/VulkanRayTracingModelTests.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_ray_tracing_model_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_model_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_model_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_model_tests COMMAND concord_ray_tracing_model_tests)

add_executable(concord_vulkan_buffer_tests tests/VulkanBufferTests.cpp)
target_compile_features(concord_vulkan_buffer_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_buffer_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_vulkan_buffer_tests COMMAND concord_vulkan_buffer_tests)

add_executable(concord_vulkan_model_asset_tests
    tests/VulkanModelAssetTests.cpp
    src/engine/render/vulkan/VulkanModelAsset.cpp
    src/engine/asset/ModelAsset.cpp
    src/engine/asset/Skeleton.cpp)
target_compile_features(concord_vulkan_model_asset_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_model_asset_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_vulkan_model_asset_tests COMMAND concord_vulkan_model_asset_tests)

add_executable(concord_vulkan_texture_tests tests/VulkanTextureTests.cpp
    src/engine/asset/ImageAsset.cpp src/engine/asset/ImageAssetUri.cpp
    src/engine/asset/StbImage.cpp
    src/engine/render/vulkan/VulkanTexture.cpp
    src/engine/render/vulkan/VulkanTextureCache.cpp
    src/engine/render/vulkan/VulkanTextureDescriptors.cpp
    src/engine/render/vulkan/VulkanTextureImage.cpp
    src/engine/render/vulkan/VulkanTextureUpload.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_vulkan_texture_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_texture_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan ${CONCORD_3RD_DIR}/stb)
target_link_libraries(concord_vulkan_texture_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_vulkan_texture_tests COMMAND concord_vulkan_texture_tests)

add_executable(concord_vulkan_pass_context_tests
    tests/VulkanPassContextAdapterTests.cpp
    src/engine/render/VulkanRenderBackendExtensions.cpp
    src/engine/render/VulkanPassRegistry.cpp)
target_compile_features(concord_vulkan_pass_context_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_pass_context_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
add_test(NAME concord_vulkan_pass_context_tests COMMAND concord_vulkan_pass_context_tests)

add_test(NAME concord_shader_option_arguments
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/ShaderOptionsScript.cmake)
