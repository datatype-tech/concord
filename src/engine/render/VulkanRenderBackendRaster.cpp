// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendState.h"

#include "engine/render/VulkanRenderBackendDebug.h"
#include "engine/render/VulkanRenderBackendShadow.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanClearPass.h"
#include "engine/render/vulkan/VulkanImageBarrier.h"
#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"

namespace Concord {

void VulkanRenderBackend::Impl::RecordRasterPasses(
    const RenderSceneSnapshot& snapshot, const VulkanDirectionalShadowState& shadowState,
    VkDescriptorSet frameDataSet, Vec3 skyColor, bool tileEnabled, bool shadowBindingReady,
    bool rayTracingBuilt, bool rayTracingComposited, bool canDrawBoxes, bool canDrawModels,
    bool canDrawSkinned)
{
    const VkCommandBuffer commandBuffer = frames.Current().commandBuffer;
    VulkanDepthBuffer& depthBuffer = depth[frames.currentFrame];
    VulkanShadowMap& shadowMap = shadowMaps[frames.currentFrame];
    VulkanRayTracingScene& rayScene = rayTracing.At(frames.currentFrame);
    if (canDrawBoxes || canDrawModels || canDrawSkinned) {
        if (shadowState.enabled) {
            BeginVulkanDebugLabel(context, commandBuffer, "Concord.DirectionalShadow",
                                  {0.9f, 0.7f, 0.2f});
            RecordVulkanDirectionalShadowPass(commandBuffer, shadowMap, shadowPipeline,
                                              snapshot, shadowState.viewProjection, &modelAssets);
            EndVulkanDebugLabel(context, commandBuffer);
        } else if (shadowBindingReady) {
            TransitionVulkanShadowMapToRead(commandBuffer, shadowMap);
        }
        if (tileEnabled) {
            BeginVulkanDebugLabel(context, commandBuffer, "Concord.TileLightCulling",
                                  {0.8f, 0.3f, 0.9f});
            RecordVulkanTileLightCulling(commandBuffer, swapchain.extent, tileCulling,
                                         frameDataSet);
            InsertVulkanTileLightBarrier(commandBuffer,
                                         frameData.tileBuffers[frames.currentFrame].buffer);
            EndVulkanDebugLabel(context, commandBuffer);
        }
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.DepthPrepass",
                              {0.2f, 0.5f, 1.0f});
        if (canDrawBoxes) {
            RecordVulkanBoxDepthPass(commandBuffer, swapchain.extent, depthBuffer.view,
                                     boxPipeline, snapshot, frameDataSet);
        }
        if (canDrawModels) {
            if (canDrawBoxes) InsertDepthWriteBarrier(commandBuffer, depthBuffer.image);
            RecordVulkanModelDepthPass(commandBuffer, swapchain.extent, depthBuffer.view,
                                       modelPipeline, snapshot, frameDataSet, modelAssets,
                                       !canDrawBoxes);
        }
        if (canDrawSkinned) {
            if (canDrawBoxes || canDrawModels) {
                InsertDepthWriteBarrier(commandBuffer, depthBuffer.image);
            }
            RecordVulkanSkinnedDepthPass(commandBuffer, swapchain.extent, depthBuffer.view,
                                         skinnedPipeline, snapshot, frameDataSet,
                                         skinningResources, frames.currentFrame, modelAssets,
                                         !canDrawBoxes && !canDrawModels);
        }
        InsertVulkanBoxDepthBarrier(commandBuffer, depthBuffer.image);
        EndVulkanDebugLabel(context, commandBuffer);
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.ForwardPass",
                              {1.0f, 0.4f, 0.2f});
        if (canDrawBoxes) {
            RecordVulkanBoxColorPass(commandBuffer, swapchain.extent, swapchain.views[imageIndex],
                                     depthBuffer.view, boxPipeline, snapshot, frameDataSet,
                                     skyColor,
                                     shadowBindingReady ? shadowMap.descriptorSet : VK_NULL_HANDLE,
                                     rayTracingBuilt && boxPipeline.HasRayQuery() &&
                                             rayScene.IsReady()
                                         ? rayScene.descriptorSet
                                         : VK_NULL_HANDLE);
        }
        if (canDrawModels) {
            if (canDrawBoxes) InsertColorWriteBarrier(commandBuffer, swapchain.images[imageIndex]);
            RecordVulkanModelColorPass(commandBuffer, swapchain.extent,
                                       swapchain.views[imageIndex], depthBuffer.view,
                                       modelPipeline, snapshot, frameDataSet, modelAssets,
                                       skyColor, !canDrawBoxes);
        }
        if (canDrawSkinned) {
            if (canDrawBoxes || canDrawModels) {
                InsertColorWriteBarrier(commandBuffer, swapchain.images[imageIndex]);
            }
            RecordVulkanSkinnedColorPass(commandBuffer, swapchain.extent,
                                         swapchain.views[imageIndex], depthBuffer.view,
                                         skinnedPipeline, snapshot, frameDataSet,
                                         skinningResources, frames.currentFrame, modelAssets,
                                         skyColor, !canDrawBoxes && !canDrawModels);
        }
        EndVulkanDebugLabel(context, commandBuffer);
    } else if (!rayTracingComposited) {
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.ClearPass", {0.2f, 0.8f, 0.4f});
        RecordClearPass(commandBuffer, swapchain.views[imageIndex], swapchain.extent, skyColor,
                        depthBuffer.view);
        EndVulkanDebugLabel(context, commandBuffer);
    }
}

} // namespace Concord
