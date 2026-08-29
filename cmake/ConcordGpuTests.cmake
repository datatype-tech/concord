# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(concord_ray_tracing_scene_gpu_tests tests/VulkanRayTracingSceneGpuTests.cpp
    tests/VulkanRayTracingSceneGpuSupport.cpp
    src/engine/render/vulkan/VulkanRayTracingSupport.cpp
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
target_compile_features(concord_ray_tracing_scene_gpu_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_scene_gpu_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_scene_gpu_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_scene_gpu_tests COMMAND concord_ray_tracing_scene_gpu_tests)
set_tests_properties(concord_ray_tracing_scene_gpu_tests PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 30)

add_executable(concord_ray_tracing_model_gpu_tests
    tests/VulkanRayTracingModelGpuTests.cpp
    tests/VulkanRayTracingSceneGpuSupport.cpp
    src/engine/render/vulkan/VulkanRayTracingSupport.cpp
    src/engine/render/vulkan/VulkanRayTracingScene.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneGeometry.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneBottomLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneTopLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRecord.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModels.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelCreate.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelBuild.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelBarrier.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneModelDestroy.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneDescriptor.cpp
    src/engine/render/vulkan/VulkanModelAsset.cpp
    src/engine/render/vulkan/VulkanModelAssetUpload.cpp
    src/engine/render/vulkan/VulkanModelAssetCache.cpp
    src/engine/asset/ModelAsset.cpp
    src/engine/asset/Skeleton.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_ray_tracing_model_gpu_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_model_gpu_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_model_gpu_tests PRIVATE Vulkan::Vulkan)
add_test(NAME concord_ray_tracing_model_gpu_tests COMMAND concord_ray_tracing_model_gpu_tests)
set_tests_properties(concord_ray_tracing_model_gpu_tests PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 30)

add_executable(concord_ray_tracing_pipeline_gpu_tests
    tests/VulkanRayTracingPipelineGpuTests.cpp
    tests/VulkanRayTracingSceneGpuSupport.cpp
    src/engine/render/vulkan/VulkanRayTracingSupport.cpp
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
    src/engine/render/vulkan/VulkanRayTracingPipeline.cpp
    src/engine/render/vulkan/VulkanRayTracingPipelineCreate.cpp
    src/engine/render/vulkan/VulkanRayTracingPipelineSbt.cpp
    src/engine/render/vulkan/VulkanRayTracingPipelineLifecycle.cpp
    src/engine/render/vulkan/VulkanRayTracingOutput.cpp
    src/engine/render/vulkan/VulkanRayTracingOutputImage.cpp
    src/engine/render/vulkan/VulkanRayTracingOutputComposite.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanShaderModule.cpp
    src/engine/render/vulkan/VulkanResult.cpp)
target_compile_features(concord_ray_tracing_pipeline_gpu_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_pipeline_gpu_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_ray_tracing_pipeline_gpu_tests PRIVATE Vulkan::Vulkan)
concord_stage_runtime(concord_ray_tracing_pipeline_gpu_tests)
add_test(NAME concord_ray_tracing_pipeline_gpu_tests
    COMMAND concord_ray_tracing_pipeline_gpu_tests)
set_tests_properties(concord_ray_tracing_pipeline_gpu_tests PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 30)

add_executable(concord_vulkan_smoke_tests tests/VulkanSmokeTests.cpp)
target_compile_features(concord_vulkan_smoke_tests PRIVATE cxx_std_23)
target_include_directories(concord_vulkan_smoke_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/Vulkan)
target_link_libraries(concord_vulkan_smoke_tests PRIVATE concord::concord Vulkan::Vulkan)
concord_stage_runtime(concord_vulkan_smoke_tests)
add_test(NAME concord_vulkan_smoke_tests COMMAND concord_vulkan_smoke_tests)
set_tests_properties(concord_vulkan_smoke_tests PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 30
    FAIL_REGULAR_EXPRESSION "\\[Concord\\]\\[Vulkan\\]\\[(warning|error)\\]")
