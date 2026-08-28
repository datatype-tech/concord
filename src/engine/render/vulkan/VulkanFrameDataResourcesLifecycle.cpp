// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameDataResources.h"

namespace Concord {

bool BindVulkanFrameData(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                         const VulkanFrameDataResources& resources,
                         u32 frameIndex) noexcept
{
    if (!resources.IsReady() || commandBuffer == VK_NULL_HANDLE ||
        pipelineLayout == VK_NULL_HANDLE || frameIndex >= kMaxFramesInFlight) {
        return false;
    }
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                            &resources.sets[frameIndex], 0, nullptr);
    return true;
}

void DestroyVulkanFrameDataResources(const VulkanContext& context,
                                     VulkanFrameDataResources& resources) noexcept
{
    if (context.device != VK_NULL_HANDLE && resources.pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, resources.pool, nullptr);
    }
    if (context.device != VK_NULL_HANDLE && resources.layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, resources.layout, nullptr);
    }
    resources.pool = VK_NULL_HANDLE;
    resources.layout = VK_NULL_HANDLE;
    for (VkDescriptorSet& set : resources.sets) {
        set = VK_NULL_HANDLE;
    }
    for (VulkanBuffer& buffer : resources.buffers) {
        DestroyVulkanBuffer(context, buffer);
    }
    for (VulkanBuffer& buffer : resources.tileBuffers) {
        DestroyVulkanBuffer(context, buffer);
    }
}

} // namespace Concord
