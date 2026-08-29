// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTextureInternal.h"

namespace Concord {

void TransitionVulkanTextureToTransfer(VkCommandBuffer commandBuffer,
                                       VulkanTexture& texture) noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
    texture.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
}

void TransitionVulkanTextureToShaderRead(VkCommandBuffer commandBuffer,
                                         VulkanTexture& texture,
                                         VkPipelineStageFlags readStages) noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         readStages == 0 ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : readStages,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

bool RecordVulkanTextureUpload(VkCommandBuffer commandBuffer, VulkanTexture& texture,
                               VkPipelineStageFlags readStages) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !texture.IsReady() ||
        !texture.staging.IsReady() || texture.uploadRecorded || texture.image == VK_NULL_HANDLE ||
        texture.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
        return false;
    }
    VkBufferMemoryBarrier stagingBarrier{};
    stagingBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    stagingBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    stagingBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    stagingBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stagingBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stagingBarrier.buffer = texture.staging.buffer;
    stagingBarrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &stagingBarrier, 0, nullptr);
    TransitionVulkanTextureToTransfer(commandBuffer, texture);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {texture.extent.width, texture.extent.height, 1};
    vkCmdCopyBufferToImage(commandBuffer, texture.staging.buffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    TransitionVulkanTextureToShaderRead(commandBuffer, texture, readStages);
    texture.uploadRecorded = true;
    return true;
}

} // namespace Concord
