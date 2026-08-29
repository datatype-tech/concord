// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingOutputInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <limits>

namespace Concord {
namespace {

/** Finds a device-local memory type accepted by an image allocation. */
u32 FindMemoryType(const VulkanContext& context, u32 typeBits) noexcept
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 index = 0; index < properties.memoryTypeCount; ++index) {
        const auto flags = properties.memoryTypes[index].propertyFlags;
        if ((typeBits & (1u << index)) != 0 && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            return index;
        }
    }
    return std::numeric_limits<u32>::max();
}

} // namespace

void DestroyVulkanRayTracingOutputImage(const VulkanContext& context,
                                        VulkanRayTracingOutput& output) noexcept
{
    const VkDevice device = output.device != VK_NULL_HANDLE ? output.device : context.device;
    if (device != VK_NULL_HANDLE && output.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, output.view, nullptr);
    }
    if (device != VK_NULL_HANDLE && output.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, output.image, nullptr);
    }
    if (device != VK_NULL_HANDLE && output.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, output.memory, nullptr);
    }
    output = {};
}

bool CreateVulkanRayTracingOutputImage(const VulkanContext& context, VkExtent2D extent,
                                       VulkanRayTracingOutput& output)
{
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE ||
        extent.width == 0 || extent.height == 0) {
        return false;
    }
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(context.physicalDevice,
                                        kVulkanRayTracingOutputFormat, &formatProperties);
    constexpr VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                                       VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                                       VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures) {
        return false;
    }
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = kVulkanRayTracingOutputFormat;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &output.image);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateImage(ray tracing output)", result);
    }
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, output.image, &requirements);
    const u32 memoryType = FindMemoryType(context, requirements.memoryTypeBits);
    if (memoryType == std::numeric_limits<u32>::max()) {
        DestroyVulkanRayTracingOutputImage(context, output);
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &output.memory);
    if (result != VK_SUCCESS) {
        DestroyVulkanRayTracingOutputImage(context, output);
        return VulkanFailed("vkAllocateMemory(ray tracing output)", result);
    }
    result = vkBindImageMemory(context.device, output.image, output.memory, 0);
    if (result != VK_SUCCESS) {
        DestroyVulkanRayTracingOutputImage(context, output);
        return VulkanFailed("vkBindImageMemory(ray tracing output)", result);
    }
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = output.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = kVulkanRayTracingOutputFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    result = vkCreateImageView(context.device, &viewInfo, nullptr, &output.view);
    if (result != VK_SUCCESS) {
        DestroyVulkanRayTracingOutputImage(context, output);
        return VulkanFailed("vkCreateImageView(ray tracing output)", result);
    }
    output.device = context.device;
    output.extent = extent;
    return true;
}

} // namespace Concord
