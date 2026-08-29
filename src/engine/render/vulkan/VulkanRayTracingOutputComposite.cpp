// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingOutput.h"

#include <limits>

namespace Concord {
namespace {

/** Records a layout transition for one color image. */
void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                     VkImageLayout newLayout, VkPipelineStageFlags sourceStage,
                     VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess,
                     VkAccessFlags destinationAccess) noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}

} // namespace

bool SupportsVulkanRayTracingComposite(const VulkanContext& context,
                                       VkFormat swapchainFormat) noexcept
{
    if (context.physicalDevice == VK_NULL_HANDLE || swapchainFormat == VK_FORMAT_UNDEFINED) {
        return false;
    }
    VkFormatProperties sourceProperties{};
    VkFormatProperties destinationProperties{};
    vkGetPhysicalDeviceFormatProperties(context.physicalDevice, kVulkanRayTracingOutputFormat,
                                        &sourceProperties);
    vkGetPhysicalDeviceFormatProperties(context.physicalDevice, swapchainFormat,
                                        &destinationProperties);
    const VkFormatFeatureFlags sourceFeatures = sourceProperties.optimalTilingFeatures;
    constexpr VkFormatFeatureFlags sourceRequired = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                                     VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                                     VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    constexpr VkFormatFeatureFlags destinationRequired = VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                                          VK_FORMAT_FEATURE_BLIT_DST_BIT;
    return (sourceFeatures & sourceRequired) == sourceRequired &&
           (destinationProperties.optimalTilingFeatures & destinationRequired) ==
               destinationRequired;
}

bool CompositeVulkanRayTracingOutput(const VulkanContext& context,
                                     VkCommandBuffer commandBuffer,
                                     VulkanRayTracingOutput& output,
                                     VkImage swapchainImage,
                                     VkFormat swapchainFormat,
                                     VkImageLayout swapchainLayout,
                                     VkExtent2D extent) noexcept
{
    if (context.physicalDevice == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE ||
        !output.IsReady() || swapchainImage == VK_NULL_HANDLE || extent.width == 0 ||
        extent.height == 0 || output.extent.width != extent.width ||
        output.extent.height != extent.height || swapchainFormat == VK_FORMAT_UNDEFINED ||
        !SupportsVulkanRayTracingComposite(context, swapchainFormat) ||
        extent.width > static_cast<u32>(std::numeric_limits<i32>::max()) ||
        extent.height > static_cast<u32>(std::numeric_limits<i32>::max())) {
        return false;
    }
    const bool outputUndefined = output.layout == VK_IMAGE_LAYOUT_UNDEFINED;
    const bool outputGeneral = output.layout == VK_IMAGE_LAYOUT_GENERAL;
    TransitionImage(commandBuffer, output.image, output.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    outputUndefined ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                     : outputGeneral ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
                                                      : VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    outputUndefined ? 0
                                    : outputGeneral ? VK_ACCESS_SHADER_WRITE_BIT
                                                     : VK_ACCESS_TRANSFER_READ_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT);
    const bool swapchainUndefined = swapchainLayout == VK_IMAGE_LAYOUT_UNDEFINED;
    TransitionImage(commandBuffer, swapchainImage, swapchainLayout,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    swapchainUndefined ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    swapchainUndefined ? 0 : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.srcOffsets[1] = {static_cast<i32>(output.extent.width),
                          static_cast<i32>(output.extent.height), 1};
    blit.dstOffsets[1] = {static_cast<i32>(extent.width), static_cast<i32>(extent.height), 1};
    vkCmdBlitImage(commandBuffer, output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_NEAREST);
    TransitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    output.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    return true;
}

} // namespace Concord
