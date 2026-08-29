// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/core/Color.h"
#include "engine/ecs/Components.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/RenderFrameData.h"
#include "engine/render/VulkanRenderBackendShadow.h"
#include "engine/render/VulkanRenderBackendDebug.h"
#include "engine/render/VulkanRenderBackendExtensions.h"
#include "engine/render/VulkanRenderBackendRayTracing.h"
#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanClearPass.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanImageBarrier.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"
#include "engine/scene/Scene.h"
namespace Concord {
void VulkanRenderBackend::DrawScene(const Scene& scene)
{
    Impl& impl = *m_impl;
    if (!impl.frameActive || impl.imageIndex >= impl.swapchain.images.size() || impl.imageIndex >= impl.swapchain.views.size() ||
        impl.imageIndex >= impl.swapchain.imageLayouts.size()) {
        impl.AbortFrame();
        return;
    }
    VulkanFrame& frame = impl.frames.Current();
    VulkanRayTracingScene& rayTracing = impl.rayTracing.At(impl.frames.currentFrame);
    const VkCommandBuffer commandBuffer = frame.commandBuffer;
    const VkImage image = impl.swapchain.images[impl.imageIndex];
    VulkanDepthBuffer& depth = impl.depth[impl.frames.currentFrame];
    if (image == VK_NULL_HANDLE || depth.image == VK_NULL_HANDLE ||
        depth.view == VK_NULL_HANDLE) {
        impl.AbortFrame();
        return;
    }
    const f32 aspect = impl.swapchain.extent.height == 0 ? 1.0f : static_cast<f32>(impl.swapchain.extent.width) /
                                                               static_cast<f32>(impl.swapchain.extent.height);
    const RenderSceneSnapshot snapshot = ExtractRenderScene(scene, aspect);
    bool hasModelObjects = false;
    bool hasBoxObjects = false;
    bool hasStaticModelObjects = false;
    bool hasSkinnedModelObjects = false;
    const bool modelUploadsReady =
        impl.PrepareModelAssets(snapshot, hasModelObjects, hasBoxObjects,
                                hasStaticModelObjects, hasSkinnedModelObjects);
    const bool textureUploadsReady = !hasModelObjects ||
                                     impl.textureCache.RecordUploads(commandBuffer);
    const bool skinningUploadReady =
        impl.UploadSkinningFrame(snapshot, impl.frames.currentFrame);
    impl.visibleObjectCount = snapshot.objects.size();
    frame.renderData = BuildRenderFrameData(snapshot);
    const bool tileInBounds = impl.swapchain.extent.width <= kMaxTileColumns * kTileSizePixels &&
                              impl.swapchain.extent.height <= kMaxTileRows * kTileSizePixels;
    const bool tileEnabled = snapshot.hasCamera && impl.frameData.IsTileReady() &&
                             impl.tileCulling.IsReady() && tileInBounds;
    frame.renderData.header.reserved = tileEnabled ? kRenderFrameFlagTileLights : 0u;
    VulkanShadowMap& shadowMap = impl.shadowMaps[impl.frames.currentFrame];
    const bool shadowBindingReady = impl.shadowPipeline.IsReady() &&
                                    impl.boxPipeline.shadowMapLayout != VK_NULL_HANDLE && shadowMap.IsReady();
    const bool shadowResourcesReady = impl.frameData.IsReady() && shadowBindingReady;
    const VulkanDirectionalShadowState shadowState = PrepareVulkanDirectionalShadowFrame(
        frame.renderData, snapshot, shadowResourcesReady);
    if (impl.frameData.IsReady() &&
        !UploadVulkanFrameData(impl.frameData, impl.frames.currentFrame, frame.renderData)) {
        impl.AbortFrame();
        return;
    }
    const VkDescriptorSet frameDataSet = impl.frameData.IsReady()
                                             ? impl.frameData.sets[impl.frames.currentFrame]
                                             : VK_NULL_HANDLE;
    const bool modelRasterReady = hasStaticModelObjects && modelUploadsReady &&
                                  textureUploadsReady && impl.textureCache.IsReady() &&
                                  impl.modelPipeline.IsReady() &&
                                  frameDataSet != VK_NULL_HANDLE;
    const bool skinnedRasterReady = hasSkinnedModelObjects && modelUploadsReady &&
                                    textureUploadsReady && impl.textureCache.IsReady() &&
                                    skinningUploadReady && impl.skinnedPipeline.IsReady() &&
                                    impl.skinningResources.IsReady() &&
                                    frameDataSet != VK_NULL_HANDLE;
    bool rayTracingBuilt = false;
    const bool rayTracingRendered = RecordVulkanRayTracingFrame(
        impl.context, commandBuffer, rayTracing, snapshot, impl.rayTracingPipeline,
        impl.rayTracingOutput, impl.boxPipeline, frameDataSet, impl.frames.currentFrame,
        rayTracingBuilt, hasModelObjects ? &impl.modelAssets : nullptr);
    TransitionToColorAttachment(commandBuffer, image,
                                impl.swapchain.imageLayouts[impl.imageIndex]);
    impl.swapchain.imageLayouts[impl.imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    TransitionToDepthAttachment(commandBuffer, depth.image, depth.layout);
    depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const bool rayTracingComposited =
        rayTracingRendered && impl.swapchain.transferDestinationSupported &&
        CompositeVulkanRayTracingFrame(
            impl.context, commandBuffer, impl.rayTracingOutput, impl.frames.currentFrame, image,
            impl.swapchain.format, impl.swapchain.imageLayouts[impl.imageIndex],
            impl.swapchain.extent);
    RunVulkanRenderExtensions(impl.context, impl.swapchain, depth, frame,
                              impl.frames.currentFrame, impl.imageIndex,
                              frameDataSet,
                              shadowBindingReady ? shadowMap.descriptorSet : VK_NULL_HANDLE,
                              rayTracingBuilt && rayTracing.IsReady() ? &rayTracing : nullptr,
                              impl.swapchainGeneration,
                              VulkanPassPhase::BeforeScene);
    const Vec3 skyColor = ToLinear(snapshot.environment.skyColor);
    impl.hasCamera = frame.renderData.header.cameraValid != 0;
    impl.lightCount = frame.renderData.header.lightCount;
    const bool canDrawBoxes = !rayTracingComposited && snapshot.hasCamera &&
                              hasBoxObjects && impl.boxPipeline.HasDepth() &&
                              impl.boxPipeline.HasColor() && frameDataSet != VK_NULL_HANDLE;
    const bool canDrawModels = !rayTracingComposited && snapshot.hasCamera && modelRasterReady;
    const bool canDrawSkinned = !rayTracingComposited && snapshot.hasCamera &&
                                skinnedRasterReady;
    impl.RecordRasterPasses(snapshot, shadowState, frameDataSet, skyColor, tileEnabled,
                            shadowBindingReady, rayTracingBuilt, rayTracingComposited,
                            canDrawBoxes, canDrawModels, canDrawSkinned);
    RunVulkanRenderExtensions(impl.context, impl.swapchain, depth, frame,
                              impl.frames.currentFrame, impl.imageIndex,
                              frameDataSet,
                              shadowBindingReady ? shadowMap.descriptorSet : VK_NULL_HANDLE,
                              rayTracingBuilt && rayTracing.IsReady() ? &rayTracing : nullptr,
                              impl.swapchainGeneration,
                              VulkanPassPhase::AfterScene);
    TransitionToPresent(commandBuffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    impl.swapchain.imageLayouts[impl.imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

} // namespace Concord
