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
#include "engine/render/vulkan/VulkanParticlePipeline.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"
#include "engine/render/vulkan/VulkanSkyPipeline.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"

namespace Concord {

void VulkanRenderBackend::Impl::RecordRasterPasses(
    const RenderSceneSnapshot& snapshot, const VulkanDirectionalShadowState& shadowState,
    VkDescriptorSet frameDataSet, Vec3 skyColor, bool tileEnabled, bool shadowBindingReady,
    bool rayTracingBuilt, bool rayTracingComposited, bool canDrawBoxes, bool canDrawModels,
    bool canDrawSkinned, VkImage targetImage, VkImageView targetView, VkExtent2D targetExtent, VulkanDepthBuffer& depthBuffer)
{
    const VkCommandBuffer commandBuffer = frames.Current().commandBuffer;
    const VulkanTexture* skybox = nullptr;
    if (!rayTracingComposited && !snapshot.environment.skybox.empty() && skyPipeline.IsReady() &&
        frameDataSet != VK_NULL_HANDLE) {
        skybox = textureCache.FindResident(snapshot.environment.skybox);
    }

    VulkanShadowMap& shadowMap = shadowMaps[frames.currentFrame];
    VulkanRayTracingScene& rayScene = rayTracing.At(frames.currentFrame);
    if (canDrawBoxes || canDrawModels || canDrawSkinned) {
        if (shadowState.enabled) {
            BeginVulkanDebugLabel(context, commandBuffer, "Concord.DirectionalShadow",
                                  {0.9f, 0.7f, 0.2f});
            RecordVulkanDirectionalShadowPass(commandBuffer, shadowMap, shadowPipeline,
                                              snapshot, shadowState.viewProjection, &modelAssets,
                                              &skinningResources, frames.currentFrame);
            EndVulkanDebugLabel(context, commandBuffer);
        } else if (shadowBindingReady) {
            TransitionVulkanShadowMapToRead(commandBuffer, shadowMap);
        }
        if (tileEnabled) {
            BeginVulkanDebugLabel(context, commandBuffer, "Concord.TileLightCulling",
                                  {0.8f, 0.3f, 0.9f});
            RecordVulkanTileLightCulling(commandBuffer, targetExtent, tileCulling,
                                         frameDataSet);
            InsertVulkanTileLightBarrier(commandBuffer,
                                         frameData.tileBuffers[frames.currentFrame].buffer);
            EndVulkanDebugLabel(context, commandBuffer);
        }
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.DepthPrepass",
                              {0.2f, 0.5f, 1.0f});
        if (canDrawBoxes) {
            RecordVulkanBoxDepthPass(commandBuffer, targetExtent, depthBuffer.view,
                                     boxPipeline, snapshot, frameDataSet);
        }
        if (canDrawModels) {
            if (canDrawBoxes) InsertDepthWriteBarrier(commandBuffer, depthBuffer.image);
            RecordVulkanModelDepthPass(commandBuffer, targetExtent, depthBuffer.view,
                                       modelPipeline, snapshot, frameDataSet, modelAssets,
                                       textureCache, !canDrawBoxes);
        }
        if (canDrawSkinned) {
            if (canDrawBoxes || canDrawModels) {
                InsertDepthWriteBarrier(commandBuffer, depthBuffer.image);
            }
            RecordVulkanSkinnedDepthPass(commandBuffer, targetExtent, depthBuffer.view,
                                         skinnedPipeline, snapshot, frameDataSet,
                                         skinningResources, frames.currentFrame, modelAssets,
                                         textureCache,
                                         !canDrawBoxes && !canDrawModels);
        }
        InsertVulkanBoxDepthBarrier(commandBuffer, depthBuffer.image);
        EndVulkanDebugLabel(context, commandBuffer);
        const bool skyDrawn = skybox != nullptr &&
                              RecordVulkanSkyPass(commandBuffer, targetExtent, targetView, skyPipeline,
                                                  frameDataSet, skybox->descriptorSet);
        if (skyDrawn) InsertColorWriteBarrier(commandBuffer, targetImage);
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.ForwardPass",
                              {1.0f, 0.4f, 0.2f});
        if (canDrawBoxes) {
            RecordVulkanBoxColorPass(commandBuffer, targetExtent, targetView,
                                     depthBuffer.view, boxPipeline, snapshot, frameDataSet,
                                     skyColor,
                                     shadowBindingReady ? shadowMap.descriptorSet : VK_NULL_HANDLE,
                                     rayTracingBuilt && boxPipeline.HasRayQuery() &&
                                             rayScene.IsReady()
                                         ? rayScene.descriptorSet
                                         : VK_NULL_HANDLE,
                                     skyDrawn);
        }
        if (canDrawModels) {
            if (canDrawBoxes || skyDrawn) InsertColorWriteBarrier(commandBuffer, targetImage);
            RecordVulkanModelColorPass(commandBuffer, targetExtent,
                                       targetView, depthBuffer.view,
                                       modelPipeline, snapshot, frameDataSet, modelAssets,
                                       textureCache, skyColor, !(canDrawBoxes || skyDrawn));
        }
        if (canDrawSkinned) {
            if (canDrawBoxes || canDrawModels || skyDrawn) {
                InsertColorWriteBarrier(commandBuffer, targetImage);
            }
            RecordVulkanSkinnedColorPass(commandBuffer, targetExtent,
                                         targetView, depthBuffer.view,
                                         skinnedPipeline, snapshot, frameDataSet,
                                         skinningResources, frames.currentFrame, modelAssets,
                                         textureCache,
                                         skyColor, !(canDrawBoxes || canDrawModels || skyDrawn));
        }
        EndVulkanDebugLabel(context, commandBuffer);
    } else if (!rayTracingComposited) {
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.ClearPass", {0.2f, 0.8f, 0.4f});
        RecordClearPass(commandBuffer, targetView, targetExtent, skyColor,
                        depthBuffer.view);
        if (skybox != nullptr) {
            RecordVulkanSkyPass(commandBuffer, targetExtent, targetView, skyPipeline, frameDataSet,
                                skybox->descriptorSet);
        }
        EndVulkanDebugLabel(context, commandBuffer);
    }
    // The colour attachment is already in COLOR_ATTACHMENT_OPTIMAL here on
    // every path, so particles compose over whichever shading path ran.
    if (particlePipeline.IsReady() && snapshot.particles.particleCount != 0) {
        BeginVulkanDebugLabel(context, commandBuffer, "Concord.ParticlePass", {1.0f, 0.85f, 0.3f});
        RecordVulkanParticlePass(commandBuffer, particlePipeline, frames.currentFrame,
                                 snapshot.particles, frameDataSet, targetView,
                                 depthBuffer.view, targetExtent);
        EndVulkanDebugLabel(context, commandBuffer);
    }
}

} // namespace Concord
