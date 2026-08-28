// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDepthBuffer.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <limits>

namespace Concord {

namespace {

/** Chooses the first supported depth format with an attachment feature. */
VkFormat SelectDepthFormat(VkPhysicalDevice device)
{
    constexpr VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

/** Finds a device-local memory type accepted by an image allocation. */
u32 FindMemoryType(const VulkanContext& context, u32 typeBits)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        const bool accepted = (typeBits & (1u << i)) != 0;
        const bool deviceLocal =
            (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        if (accepted && deviceLocal) {
            return i;
        }
    }
    return std::numeric_limits<u32>::max();
}

} // namespace

bool CreateVulkanDepthBuffer(const VulkanContext& context, VulkanDepthBuffer& depth,
                             VkExtent2D extent)
{
    if (context.device == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
        return false;
    }
    depth.format = SelectDepthFormat(context.physicalDevice);
    if (depth.format == VK_FORMAT_UNDEFINED) {
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depth.format;
    imageInfo.extent = VkExtent3D{extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &depth.image);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateImage(depth)", result);
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, depth.image, &requirements);
    const u32 memoryType = FindMemoryType(context, requirements.memoryTypeBits);
    if (memoryType == std::numeric_limits<u32>::max()) {
        DestroyVulkanDepthBuffer(context, depth);
        return false;
    }

    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &depth.memory);
    if (result != VK_SUCCESS) {
        DestroyVulkanDepthBuffer(context, depth);
        return VulkanFailed("vkAllocateMemory(depth)", result);
    }
    result = vkBindImageMemory(context.device, depth.image, depth.memory, 0);
    if (result != VK_SUCCESS) {
        DestroyVulkanDepthBuffer(context, depth);
        return VulkanFailed("vkBindImageMemory(depth)", result);
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depth.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depth.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    result = vkCreateImageView(context.device, &viewInfo, nullptr, &depth.view);
    if (result != VK_SUCCESS) {
        DestroyVulkanDepthBuffer(context, depth);
        return VulkanFailed("vkCreateImageView(depth)", result);
    }
    depth.extent = extent;
    return true;
}

void DestroyVulkanDepthBuffer(const VulkanContext& context, VulkanDepthBuffer& depth)
{
    if (context.device != VK_NULL_HANDLE && depth.view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, depth.view, nullptr);
    }
    if (context.device != VK_NULL_HANDLE && depth.image != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, depth.image, nullptr);
    }
    if (context.device != VK_NULL_HANDLE && depth.memory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, depth.memory, nullptr);
    }
    depth = {};
}

} // namespace Concord
