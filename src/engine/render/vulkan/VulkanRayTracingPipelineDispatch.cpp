// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipeline.h"

#include "engine/render/vulkan/VulkanRayTracingScene.h"

namespace Concord {

bool RecordVulkanRayTracingDispatch(VkCommandBuffer commandBuffer,
                                    const VulkanRayTracingPipeline& pipeline,
                                    VkDescriptorSet frameDataSet,
                                    VkDescriptorSet outputSet,
                                    const VulkanRayTracingScene& scene,
                                    VkExtent2D extent) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !pipeline.IsReady() ||
        frameDataSet == VK_NULL_HANDLE || outputSet == VK_NULL_HANDLE ||
        !scene.IsReady() || extent.width == 0 || extent.height == 0) {
        return false;
    }
    const VkDescriptorSet sets[] = {frameDataSet, outputSet, scene.descriptorSet};
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipeline.layout, 0, 3, sets, 0, nullptr);
    const VkStridedDeviceAddressRegionKHR callable{};
    pipeline.cmdTraceRays(commandBuffer, &pipeline.raygenRegion, &pipeline.missRegion,
                          &pipeline.hitRegion, &callable, extent.width, extent.height, 1);
    return true;
}

} // namespace Concord
