# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(concord_ecs_tests
    tests/EcsTests.cpp tests/EcsWorldTests.cpp tests/EcsQueryTests.cpp
    tests/EcsDeferredTests.cpp tests/EcsSceneTests.cpp)
target_compile_features(concord_ecs_tests PRIVATE cxx_std_23)
target_include_directories(concord_ecs_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_ecs_tests COMMAND concord_ecs_tests)

add_executable(concord_asset_tests
    tests/AssetTests.cpp
    ${CONCORD_ASSET_SOURCES}
    src/engine/animation/AnimationSampling.cpp)
target_compile_features(concord_asset_tests PRIVATE cxx_std_23)
target_include_directories(concord_asset_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/stb)
add_test(NAME concord_asset_tests COMMAND concord_asset_tests)

add_executable(concord_image_tests
    tests/ImageAssetTests.cpp
    src/engine/asset/ImageAsset.cpp src/engine/asset/ImageAssetCache.cpp
    src/engine/asset/ImageAssetUri.cpp src/engine/asset/StbImage.cpp)
target_compile_features(concord_image_tests PRIVATE cxx_std_23)
target_include_directories(concord_image_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/stb)
add_test(NAME concord_image_tests COMMAND concord_image_tests)

add_executable(concord_render_snapshot_tests
    tests/RenderSceneSnapshotTests.cpp src/engine/render/RenderSceneSnapshot.cpp
    src/engine/render/RenderModelSnapshot.cpp
    src/engine/render/RenderSkinningSnapshot.cpp
    src/engine/asset/ModelAsset.cpp src/engine/asset/Skeleton.cpp
    src/engine/asset/SkinningPalette.cpp)
target_compile_features(concord_render_snapshot_tests PRIVATE cxx_std_23)
target_include_directories(concord_render_snapshot_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_render_snapshot_tests COMMAND concord_render_snapshot_tests)

add_executable(concord_render_skinning_snapshot_tests
    tests/RenderSkinningSnapshotTests.cpp src/engine/render/RenderSceneSnapshot.cpp
    src/engine/render/RenderModelSnapshot.cpp src/engine/render/RenderSkinningSnapshot.cpp
    src/engine/asset/ModelAsset.cpp src/engine/asset/Skeleton.cpp
    src/engine/asset/SkinningPalette.cpp)
target_compile_features(concord_render_skinning_snapshot_tests PRIVATE cxx_std_23)
target_include_directories(concord_render_skinning_snapshot_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_render_skinning_snapshot_tests COMMAND concord_render_skinning_snapshot_tests)

add_executable(concord_render_frame_data_tests
    tests/RenderFrameDataTests.cpp src/engine/render/RenderFrameData.cpp
    src/engine/render/RenderSceneSnapshot.cpp src/engine/render/RenderModelSnapshot.cpp
    src/engine/render/RenderSkinningSnapshot.cpp
    src/engine/asset/ModelAsset.cpp src/engine/asset/Skeleton.cpp
    src/engine/asset/SkinningPalette.cpp)
target_compile_features(concord_render_frame_data_tests PRIVATE cxx_std_23)
target_include_directories(concord_render_frame_data_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_render_frame_data_tests COMMAND concord_render_frame_data_tests)

add_executable(concord_ray_tracing_geometry_tests
    tests/RayTracingGeometryTests.cpp
    src/engine/render/RayTracingGeometry.cpp)
target_compile_features(concord_ray_tracing_geometry_tests PRIVATE cxx_std_23)
target_include_directories(concord_ray_tracing_geometry_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_ray_tracing_geometry_tests COMMAND concord_ray_tracing_geometry_tests)

add_executable(concord_skinning_palette_tests
    tests/SkinningPaletteTests.cpp src/engine/asset/SkinningPalette.cpp)
target_compile_features(concord_skinning_palette_tests PRIVATE cxx_std_23)
target_include_directories(concord_skinning_palette_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_skinning_palette_tests COMMAND concord_skinning_palette_tests)

add_executable(concord_animation_system_tests
    tests/AnimationSystemTests.cpp
    src/engine/ecs/AnimationSystem.cpp
    src/engine/animation/AnimationBlend.cpp
    src/engine/animation/AnimationController.cpp
    src/engine/animation/AnimationGraph.cpp
    src/engine/animation/AnimationLayer.cpp
    src/engine/animation/AnimationSampling.cpp
    src/engine/animation/AnimationStateMachine.cpp
    src/engine/animation/AnimationStateMachineTime.cpp
    src/engine/animation/JointMask.cpp
    src/engine/asset/Animation.cpp src/engine/asset/Skeleton.cpp
    src/engine/asset/ModelAsset.cpp)
target_compile_features(concord_animation_system_tests PRIVATE cxx_std_23)
target_include_directories(concord_animation_system_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_animation_system_tests COMMAND concord_animation_system_tests)

add_executable(concord_animation_retarget_tests
    tests/AnimationRetargetTests.cpp
    src/engine/animation/AnimationRetarget.cpp
    src/engine/animation/AnimationRetargetMath.cpp
    src/engine/animation/Humanoid.cpp
    src/engine/asset/Skeleton.cpp)
target_compile_features(concord_animation_retarget_tests PRIVATE cxx_std_23)
target_include_directories(concord_animation_retarget_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_animation_retarget_tests COMMAND concord_animation_retarget_tests)

add_executable(concord_asset_hardening_tests
    tests/AssetImportHardeningTests.cpp
    ${CONCORD_ASSET_SOURCES}
    src/engine/animation/AnimationSampling.cpp)
target_compile_features(concord_asset_hardening_tests PRIVATE cxx_std_23)
target_include_directories(concord_asset_hardening_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/stb)
add_test(NAME concord_asset_hardening_tests COMMAND concord_asset_hardening_tests)

add_executable(concord_animation_blend_tests
    tests/AnimationBlendTests.cpp
    src/engine/animation/AnimationBlend.cpp
    src/engine/animation/AnimationLayer.cpp
    src/engine/animation/JointMask.cpp
    src/engine/asset/Skeleton.cpp)
target_compile_features(concord_animation_blend_tests PRIVATE cxx_std_23)
target_include_directories(concord_animation_blend_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_animation_blend_tests COMMAND concord_animation_blend_tests)

add_executable(concord_animation_state_machine_tests
    tests/AnimationStateMachineTests.cpp
    src/engine/ecs/AnimationSystem.cpp
    src/engine/ecs/WorldId.cpp
    src/engine/animation/AnimationBlend.cpp
    src/engine/animation/AnimationController.cpp
    src/engine/animation/AnimationGraph.cpp
    src/engine/animation/AnimationLayer.cpp
    src/engine/animation/AnimationSampling.cpp
    src/engine/animation/AnimationStateMachine.cpp
    src/engine/animation/AnimationStateMachineTime.cpp
    src/engine/animation/JointMask.cpp
    src/engine/asset/Animation.cpp src/engine/asset/Skeleton.cpp
    src/engine/asset/ModelAsset.cpp)
target_compile_features(concord_animation_state_machine_tests PRIVATE cxx_std_23)
target_include_directories(concord_animation_state_machine_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_animation_state_machine_tests
         COMMAND concord_animation_state_machine_tests)

add_executable(concord_input_tests
    tests/InputTests.cpp
    src/engine/input/InputMap.cpp
    src/engine/input/SdlInputCodes.cpp)
target_compile_features(concord_input_tests PRIVATE cxx_std_23)
target_include_directories(concord_input_tests PRIVATE
    ${CONCORD_TEST_INCLUDE} ${CONCORD_3RD_DIR}/SDL3)
add_test(NAME concord_input_tests COMMAND concord_input_tests)

add_executable(concord_debug_overlay_tests
    tests/DebugOverlayTests.cpp
    src/engine/debug/DebugOverlay.cpp)
target_compile_features(concord_debug_overlay_tests PRIVATE cxx_std_23)
target_include_directories(concord_debug_overlay_tests PRIVATE ${CONCORD_TEST_INCLUDE})
add_test(NAME concord_debug_overlay_tests COMMAND concord_debug_overlay_tests)

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
