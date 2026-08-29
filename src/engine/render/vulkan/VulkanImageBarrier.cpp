// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanImageBarrier.h"

namespace Concord {

namespace {

/** Builds the barrier shared by both transitions, covering the whole image. */
VkImageMemoryBarrier MakeColorBarrier(VkImage image)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    return barrier;
}

} // namespace

void TransitionToColorAttachment(VkCommandBuffer commandBuffer, VkImage image,
                                 VkImageLayout oldLayout)
{
    VkImageMemoryBarrier barrier = MakeColorBarrier(image);
    barrier.oldLayout = oldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
                             oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                                ? 0
                                : VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    const VkPipelineStageFlags sourceStage =
        oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                  ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                  : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    vkCmdPipelineBarrier(commandBuffer, sourceStage,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);
}

void TransitionToDepthAttachment(VkCommandBuffer commandBuffer, VkImage image,
                                 VkImageLayout oldLayout)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.srcAccessMask = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                ? 0
                                : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    const VkPipelineStageFlags sourceStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                                 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                 : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    vkCmdPipelineBarrier(commandBuffer, sourceStage,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void InsertDepthWriteBarrier(VkCommandBuffer commandBuffer, VkImage image)
{
    if (commandBuffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE) return;
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = barrier.oldLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void InsertColorWriteBarrier(VkCommandBuffer commandBuffer, VkImage image)
{
    if (commandBuffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE) return;
    VkImageMemoryBarrier barrier = MakeColorBarrier(image);
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = barrier.oldLayout;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}

void TransitionToPresent(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout)
{
    VkImageMemoryBarrier barrier = MakeColorBarrier(image);
    barrier.oldLayout = oldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

} // namespace Concord
