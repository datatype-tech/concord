// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBuffer.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <cstdio>

namespace Concord {
namespace {

struct MemoryType {
    u32 index = VK_MAX_MEMORY_TYPES;
    VkMemoryPropertyFlags properties = 0;
};

/** Finds a compatible memory type, preferring the requested property subset. */
MemoryType FindMemoryType(const VulkanContext& context, u32 typeBits,
                          VkMemoryPropertyFlags required,
                          VkMemoryPropertyFlags preferred)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    MemoryType fallback{};
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) == 0) {
            continue;
        }
        const VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
        if ((flags & required) != required) {
            continue;
        }
        if ((flags & preferred) == preferred) {
            return {i, flags};
        }
        if (fallback.index == VK_MAX_MEMORY_TYPES) {
            fallback = {i, flags};
        }
    }
    return fallback;
}

/** Reports a buffer-specific failure that has no VkResult to decode. */
bool BufferFailed(const char* reason) noexcept
{
    std::fprintf(stderr, "[Concord] VulkanBuffer: %s\n", reason);
    return false;
}

} // namespace

bool CreateVulkanBuffer(const VulkanContext& context, const VulkanBufferCreateInfo& info,
                        VulkanBuffer& buffer)
{
    DestroyVulkanBuffer(context, buffer);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE) {
        return BufferFailed("creation requires valid Vulkan device handles");
    }
    if (info.size == 0 || info.usage == 0) {
        return BufferFailed("creation requires non-zero size and usage");
    }
    if (info.deviceAddress && !context.rayTracing.bufferDeviceAddress) {
        return BufferFailed("device address requested without an enabled feature");
    }

    VkBufferUsageFlags usage = info.usage;
    if (info.deviceAddress) {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    VkMemoryPropertyFlags required = info.requiredMemoryProperties;
    if (info.persistentMap) {
        required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = info.size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(context.device, &bufferInfo, nullptr, &buffer.buffer);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateBuffer", result);
    }
    buffer.device = context.device;
    buffer.size = info.size;
    buffer.usage = usage;

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context.device, buffer.buffer, &requirements);
    const MemoryType memoryType =
        FindMemoryType(context, requirements.memoryTypeBits, required,
                       info.preferredMemoryProperties);
    if (memoryType.index == VK_MAX_MEMORY_TYPES) {
        DestroyVulkanBuffer(context, buffer);
        return BufferFailed("no compatible memory type");
    }

    VkMemoryAllocateFlagsInfo flagsInfo{};
    if (info.deviceAddress) {
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = info.deviceAddress ? &flagsInfo : nullptr;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType.index;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &buffer.memory);
    if (result != VK_SUCCESS) {
        DestroyVulkanBuffer(context, buffer);
        return VulkanFailed("vkAllocateMemory", result);
    }
    result = vkBindBufferMemory(context.device, buffer.buffer, buffer.memory, 0);
    if (result != VK_SUCCESS) {
        DestroyVulkanBuffer(context, buffer);
        return VulkanFailed("vkBindBufferMemory", result);
    }
    if (info.persistentMap) {
        result = vkMapMemory(context.device, buffer.memory, 0, requirements.size, 0,
                             &buffer.mapped);
        if (result != VK_SUCCESS) {
            DestroyVulkanBuffer(context, buffer);
            return VulkanFailed("vkMapMemory", result);
        }
    }
    if (info.deviceAddress) {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer.buffer;
        buffer.deviceAddress = vkGetBufferDeviceAddress(context.device, &addressInfo);
        if (buffer.deviceAddress == 0) {
            DestroyVulkanBuffer(context, buffer);
            return BufferFailed("vkGetBufferDeviceAddress returned zero");
        }
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context.physicalDevice, &properties);
    buffer.allocationSize = requirements.size;
    buffer.nonCoherentAtomSize = properties.limits.nonCoherentAtomSize;
    buffer.memoryProperties = memoryType.properties;
    return true;
}

} // namespace Concord
