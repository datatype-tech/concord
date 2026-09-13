// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendRayTracing.h"

#include "engine/render/VulkanRenderBackendDebug.h"
#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

namespace Concord {

bool RecordVulkanRayTracingFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                 VulkanRayTracingScene& scene,
                                 const RenderSceneSnapshot& snapshot,
                                 const VulkanRayTracingPipeline& pipeline,
                                 VulkanRayTracingOutputRing& outputRing,
                                 const VulkanBoxPipeline& boxPipeline,
                                 VkDescriptorSet frameDataSet, u32 frameIndex,
                                 bool& sceneBuilt,
                                 const VulkanModelAssetCache* modelAssets,
                                 VulkanRayTracingTextures& textures,
                                 const VulkanTextureCache& textureCache) noexcept
{
    sceneBuilt = false;
    // Skinned models refit their own per-frame BLAS, so an animated frame no
    // longer downgrades the whole image to the raster path.
    const bool pipelineConsumer =
        pipeline.IsReady() && outputRing.IsReady();
    const bool queryConsumer = context.rayTracing.IsRayQueryUsable() &&
                               boxPipeline.HasRayQuery();
    if (commandBuffer == VK_NULL_HANDLE || !scene.IsReady() || !snapshot.hasCamera ||
        snapshot.objects.empty() || (!pipelineConsumer && !queryConsumer)) {
        return false;
    }
    if (modelAssets != nullptr &&
        !EnsureVulkanRayTracingModelPrimitives(context, scene, snapshot, *modelAssets)) {
        return false;
    }
    if (!UpdateVulkanRayTracingSkinnedGeometry(scene, snapshot)) {
        return false;
    }
    // The sampler slots are filled by the ensure call above, so the array has
    // to be resolved after it. A pipeline built without a texture layout owns
    // only three sets and must not be handed a fourth.
    VkDescriptorSet textureSet = VK_NULL_HANDLE;
    if (textures.IsReady()) {
        if (!UpdateVulkanRayTracingTextures(context, textures, textureCache, scene.textureSlots)) {
            return false;
        }
        textureSet = textures.set;
    }
    scene.includeNonShadowCasters = pipelineConsumer;
    BeginVulkanDebugLabel(context, commandBuffer, "Concord.RayTracingBuild", {0.2f, 0.9f, 0.8f});
    const bool built = RecordVulkanRayTracingSceneBuild(commandBuffer, scene, &snapshot);
    sceneBuilt = built;
    if (built) {
        if (pipelineConsumer) {
            InsertVulkanRayTracingSceneReadBarrier(
                commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        } else if (queryConsumer) {
            InsertVulkanRayTracingSceneReadBarrier(
                commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
    }
    EndVulkanDebugLabel(context, commandBuffer);
    if (!built || !pipelineConsumer || !pipeline.IsReady() || !outputRing.IsReady() ||
        frameDataSet == VK_NULL_HANDLE) {
        return false;
    }
    VulkanRayTracingOutput& output = outputRing.At(frameIndex);
    PrepareVulkanRayTracingOutput(commandBuffer, output);
    BeginVulkanDebugLabel(context, commandBuffer, "Concord.RayTracingTrace", {0.9f, 0.2f, 0.8f});
    const bool traced = RecordVulkanRayTracingDispatch(commandBuffer, pipeline, frameDataSet,
                                                       output.descriptorSet, scene, output.extent,
                                                       textureSet);
    EndVulkanDebugLabel(context, commandBuffer);
    return traced;
}

bool CompositeVulkanPostProcessFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                     const VulkanPostProcessRing& postProcess, u32 frameIndex,
                                     VkImage swapchainImage, VkFormat swapchainFormat,
                                     VkImageLayout swapchainLayout, VkExtent2D extent) noexcept
{
    if (!postProcess.IsReady() || frameIndex >= kMaxFramesInFlight) {
        return false;
    }
    const VulkanPostProcess& graded = postProcess.items[frameIndex];
    return CompositeVulkanColorImage(context, commandBuffer, graded.image, graded.extent,
                                     graded.layout, swapchainImage, swapchainFormat,
                                     swapchainLayout, extent);
}

bool CompositeVulkanRayTracingFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                    VulkanRayTracingOutputRing& outputRing, u32 frameIndex,
                                    VkImage swapchainImage, VkFormat swapchainFormat,
                                    VkImageLayout swapchainLayout, VkExtent2D extent) noexcept
{
    if (!outputRing.IsReady()) {
        return false;
    }
    return CompositeVulkanRayTracingOutput(context, commandBuffer, outputRing.At(frameIndex),
                                           swapchainImage, swapchainFormat, swapchainLayout,
                                           extent);
}

} // namespace Concord
