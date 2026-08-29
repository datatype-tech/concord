// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTextureInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <limits>

namespace Concord {
namespace {

VkFormat SelectFormat(VkPhysicalDevice device) noexcept
{
    constexpr VkFormat candidates[] = {VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_FORMAT_R8G8B8A8_UNORM};
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                               VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);
        if ((properties.optimalTilingFeatures & required) == required) return format;
    }
    return VK_FORMAT_UNDEFINED;
}

u32 FindDeviceLocalType(const VulkanContext& context, u32 typeBits) noexcept
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 index = 0; index < properties.memoryTypeCount; ++index) {
        const bool accepted = (typeBits & (1u << index)) != 0;
        const bool local = (properties.memoryTypes[index].propertyFlags &
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        if (accepted && local) return index;
    }
    return std::numeric_limits<u32>::max();
}

} // namespace

bool CreateVulkanTextureImage(const VulkanContext& context, const ImageAsset& source,
                              VulkanTexture& texture)
{
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE ||
        !source.IsValid()) return false;
    texture.format = SelectFormat(context.physicalDevice);
    if (texture.format == VK_FORMAT_UNDEFINED) return false;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = texture.format;
    imageInfo.extent = {source.width, source.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &texture.image);
    if (result != VK_SUCCESS) return VulkanFailed("vkCreateImage(texture)", result);
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, texture.image, &requirements);
    const u32 memoryType = FindDeviceLocalType(context, requirements.memoryTypeBits);
    if (memoryType == std::numeric_limits<u32>::max()) return false;
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &texture.memory);
    if (result != VK_SUCCESS) return VulkanFailed("vkAllocateMemory(texture)", result);
    result = vkBindImageMemory(context.device, texture.image, texture.memory, 0);
    if (result != VK_SUCCESS) return VulkanFailed("vkBindImageMemory(texture)", result);
    texture.device = context.device;
    texture.extent = {source.width, source.height};
    texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    return true;
}

} // namespace Concord
