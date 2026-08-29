// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendRayTracing.h"

#include "engine/render/VulkanRenderBackendDebug.h"

namespace Concord {

bool RecordVulkanRayTracingFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                 VulkanRayTracingScene& scene,
                                 const RenderSceneSnapshot& snapshot,
                                 const VulkanRayTracingPipeline& pipeline,
                                 VulkanRayTracingOutputRing& outputRing,
                                 const VulkanBoxPipeline& boxPipeline,
                                 VkDescriptorSet frameDataSet, u32 frameIndex,
                                 bool& sceneBuilt) noexcept
{
    sceneBuilt = false;
    const bool pipelineConsumer = pipeline.IsReady() && outputRing.IsReady();
    const bool queryConsumer = context.rayTracing.IsRayQueryUsable() &&
                               boxPipeline.HasRayQuery();
    if (commandBuffer == VK_NULL_HANDLE || !scene.IsReady() || !snapshot.hasCamera ||
        snapshot.objects.empty() || (!pipelineConsumer && !queryConsumer)) {
        return false;
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
    if (!built || !pipeline.IsReady() || !outputRing.IsReady() ||
        frameDataSet == VK_NULL_HANDLE) {
        return false;
    }
    VulkanRayTracingOutput& output = outputRing.At(frameIndex);
    PrepareVulkanRayTracingOutput(commandBuffer, output);
    BeginVulkanDebugLabel(context, commandBuffer, "Concord.RayTracingTrace", {0.9f, 0.2f, 0.8f});
    const bool traced = RecordVulkanRayTracingDispatch(commandBuffer, pipeline, frameDataSet,
                                                       output.descriptorSet, scene, output.extent);
    EndVulkanDebugLabel(context, commandBuffer);
    return traced;
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
