// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowMap.h"

#include "engine/render/vulkan/VulkanShadowMapInternal.h"
#include "engine/render/vulkan/VulkanResult.h"

#include <limits>

namespace Concord {
namespace {

/** Chooses a depth format that also supports sampled reads. */
VkFormat SelectShadowFormat(VkPhysicalDevice device)
{
    constexpr VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT,
                                       VK_FORMAT_D16_UNORM};
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                               VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

/** Finds a device-local memory type accepted by an image allocation. */
u32 FindDeviceLocalType(const VulkanContext& context, u32 typeBits)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        const bool accepted = (typeBits & (1u << i)) != 0;
        const bool local = (properties.memoryTypes[i].propertyFlags &
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        if (accepted && local) {
            return i;
        }
    }
    return std::numeric_limits<u32>::max();
}

/** Rejects dimensions that exceed the device's 2D image limit. */
bool IsExtentSupported(VkPhysicalDevice device, VkExtent2D extent)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    return extent.width <= properties.limits.maxImageDimension2D &&
           extent.height <= properties.limits.maxImageDimension2D;
}

/** Creates and allocates the depth image backing the map. */
bool CreateShadowImage(const VulkanContext& context, VkExtent2D extent,
                       VulkanShadowMap& shadowMap)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = shadowMap.format;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &shadowMap.image);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateImage(shadow)", result);
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, shadowMap.image, &requirements);
    const u32 memoryType = FindDeviceLocalType(context, requirements.memoryTypeBits);
    if (memoryType == std::numeric_limits<u32>::max()) {
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &shadowMap.memory);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkAllocateMemory(shadow)", result);
    }
    result = vkBindImageMemory(context.device, shadowMap.image, shadowMap.memory, 0);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkBindImageMemory(shadow)", result);
    }
    return true;
}

} // namespace

bool CreateVulkanShadowMap(const VulkanContext& context, VkExtent2D extent,
                           VulkanShadowMap& shadowMap)
{
    DestroyVulkanShadowMap(context, shadowMap);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE ||
        extent.width == 0 || extent.height == 0 ||
        !IsExtentSupported(context.physicalDevice, extent)) {
        return false;
    }
    shadowMap.format = SelectShadowFormat(context.physicalDevice);
    if (shadowMap.format == VK_FORMAT_UNDEFINED || !CreateShadowImage(context, extent, shadowMap)) {
        DestroyVulkanShadowMap(context, shadowMap);
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowMap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = shadowMap.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    const VkResult viewResult =
        vkCreateImageView(context.device, &viewInfo, nullptr, &shadowMap.view);
    if (viewResult != VK_SUCCESS) {
        DestroyVulkanShadowMap(context, shadowMap);
        return VulkanFailed("vkCreateImageView(shadow)", viewResult);
    }
    shadowMap.extent = extent;
    shadowMap.device = context.device;
    if (!CreateVulkanShadowMapDescriptors(context, shadowMap)) {
        DestroyVulkanShadowMap(context, shadowMap);
        return false;
    }
    return true;
}

} // namespace Concord
