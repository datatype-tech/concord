// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowMap.h"

namespace Concord {
namespace {

/** Emits an image barrier for the single depth subresource. */
void Barrier(VkCommandBuffer commandBuffer, VulkanShadowMap& shadowMap,
             VkImageLayout newLayout, VkPipelineStageFlags sourceStage,
             VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess,
             VkAccessFlags destinationAccess) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || shadowMap.image == VK_NULL_HANDLE ||
        shadowMap.layout == newLayout) {
        return;
    }
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = shadowMap.layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = shadowMap.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    shadowMap.layout = newLayout;
}

} // namespace

void TransitionVulkanShadowMapToDepth(VkCommandBuffer commandBuffer,
                                      VulkanShadowMap& shadowMap) noexcept
{
    const VkPipelineStageFlags sourceStage = shadowMap.layout == VK_IMAGE_LAYOUT_UNDEFINED
                                                  ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                  : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    const VkAccessFlags sourceAccess = shadowMap.layout == VK_IMAGE_LAYOUT_UNDEFINED
                                           ? 0
                                           : VK_ACCESS_SHADER_READ_BIT;
    Barrier(commandBuffer, shadowMap, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            sourceStage, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            sourceAccess, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
}

void TransitionVulkanShadowMapToRead(VkCommandBuffer commandBuffer,
                                     VulkanShadowMap& shadowMap) noexcept
{
    const bool initialized = shadowMap.layout != VK_IMAGE_LAYOUT_UNDEFINED;
    Barrier(commandBuffer, shadowMap, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            initialized ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                       : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            initialized ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0,
            VK_ACCESS_SHADER_READ_BIT);
}

void InvalidateVulkanShadowMapLayouts(VulkanShadowMap* shadowMaps, u32 count) noexcept
{
    if (!shadowMaps) {
        return;
    }
    for (u32 i = 0; i < count; ++i) {
        shadowMaps[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

} // namespace Concord
