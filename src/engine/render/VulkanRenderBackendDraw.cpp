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
#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanClearPass.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanImageBarrier.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"
#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"
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
    impl.visibleObjectCount = snapshot.objects.size();
    frame.renderData = BuildRenderFrameData(snapshot);
    bool rayTracingBuilt = false;
    if (rayTracing.IsReady() && !snapshot.objects.empty()) {
        BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.RayTracingBuild",
                              {0.2f, 0.9f, 0.8f});
        rayTracingBuilt = RecordVulkanRayTracingSceneBuild(commandBuffer, rayTracing, &snapshot);
        if (rayTracingBuilt) {
            InsertVulkanRayTracingSceneReadBarrier(commandBuffer);
        }
        EndVulkanDebugLabel(impl.context, commandBuffer);
    }
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

    TransitionToColorAttachment(commandBuffer, image,
                                impl.swapchain.imageLayouts[impl.imageIndex]);
    impl.swapchain.imageLayouts[impl.imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    TransitionToDepthAttachment(commandBuffer, depth.image, depth.layout);
    depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    const Vec3 skyColor = ToLinear(snapshot.environment.skyColor);
    impl.hasCamera = frame.renderData.header.cameraValid != 0;
    impl.lightCount = frame.renderData.header.lightCount;
    const bool canDrawBoxes = snapshot.hasCamera && !snapshot.objects.empty() && impl.boxPipeline.HasDepth() &&
                              impl.boxPipeline.HasColor() && frameDataSet != VK_NULL_HANDLE;
    if (canDrawBoxes) {
        if (shadowState.enabled) {
            BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.DirectionalShadow",
                                  {0.9f, 0.7f, 0.2f});
            RecordVulkanDirectionalShadowPass(commandBuffer, shadowMap, impl.shadowPipeline,
                                              snapshot, shadowState.viewProjection);
            EndVulkanDebugLabel(impl.context, commandBuffer);
        } else if (shadowBindingReady) {
            TransitionVulkanShadowMapToRead(commandBuffer, shadowMap);
        }
        if (tileEnabled) {
            BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.TileLightCulling",
                                  {0.8f, 0.3f, 0.9f});
            RecordVulkanTileLightCulling(commandBuffer, impl.swapchain.extent, impl.tileCulling,
                                         frameDataSet);
            InsertVulkanTileLightBarrier(commandBuffer,
                                         impl.frameData.tileBuffers[impl.frames.currentFrame].buffer);
            EndVulkanDebugLabel(impl.context, commandBuffer);
        }
        BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.DepthPrepass", {0.2f, 0.5f, 1.0f});
        RecordVulkanBoxDepthPass(commandBuffer, impl.swapchain.extent, depth.view,
                                 impl.boxPipeline, snapshot, frameDataSet);
        InsertVulkanBoxDepthBarrier(commandBuffer, depth.image);
        EndVulkanDebugLabel(impl.context, commandBuffer);
        BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.ForwardPass", {1.0f, 0.4f, 0.2f});
        RecordVulkanBoxColorPass(commandBuffer, impl.swapchain.extent,
                                 impl.swapchain.views[impl.imageIndex], depth.view,
                                 impl.boxPipeline, snapshot, frameDataSet, skyColor,
                                 shadowBindingReady ? shadowMap.descriptorSet : VK_NULL_HANDLE,
                                 rayTracingBuilt && impl.boxPipeline.HasRayQuery() &&
                                      rayTracing.IsReady()
                                      ? rayTracing.descriptorSet
                                     : VK_NULL_HANDLE);
        EndVulkanDebugLabel(impl.context, commandBuffer);
    } else {
        BeginVulkanDebugLabel(impl.context, commandBuffer, "Concord.ClearPass", {0.2f, 0.8f, 0.4f});
        RecordClearPass(commandBuffer, impl.swapchain.views[impl.imageIndex], impl.swapchain.extent,
                        skyColor, depth.view);
        EndVulkanDebugLabel(impl.context, commandBuffer);
    }
    TransitionToPresent(commandBuffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    impl.swapchain.imageLayouts[impl.imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

} // namespace Concord
