// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBuffer.h"

#include <cstdio>
#include <cstring>

namespace Concord {
namespace {

/** Reports a buffer-specific failure that has no VkResult to decode. */
bool BufferFailed(const char* reason) noexcept
{
    std::fprintf(stderr, "[Concord] VulkanBuffer: %s\n", reason);
    return false;
}

/** Clears partially constructed state after a failed allocation. */
void ResetBuffer(VulkanBuffer& buffer) noexcept
{
    buffer.buffer = VK_NULL_HANDLE;
    buffer.memory = VK_NULL_HANDLE;
    buffer.device = VK_NULL_HANDLE;
    buffer.size = 0;
    buffer.allocationSize = 0;
    buffer.nonCoherentAtomSize = 1;
    buffer.usage = 0;
    buffer.memoryProperties = 0;
    buffer.deviceAddress = 0;
    buffer.mapped = nullptr;
}

} // namespace

bool CreateVulkanHostBuffer(const VulkanContext& context, VkDeviceSize size,
                            VkBufferUsageFlags usage, VulkanBuffer& buffer)
{
    return CreateVulkanHostBuffer(context, size, usage, buffer, false);
}

bool CreateVulkanHostBuffer(const VulkanContext& context, VkDeviceSize size,
                            VkBufferUsageFlags usage, VulkanBuffer& buffer,
                            bool deviceAddress)
{
    VulkanBufferCreateInfo info{};
    info.size = size;
    info.usage = usage;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    info.persistentMap = true;
    info.deviceAddress = deviceAddress;
    return CreateVulkanBuffer(context, info, buffer);
}

bool UploadVulkanBuffer(VulkanBuffer& buffer, std::span<const std::byte> bytes) noexcept
{
    if (!buffer.IsReady() || bytes.size_bytes() > buffer.size ||
        (!bytes.empty() && bytes.data() == nullptr)) {
        return BufferFailed("upload range is invalid for the mapped buffer");
    }
    if (!bytes.empty()) {
        std::memcpy(buffer.mapped, bytes.data(), bytes.size_bytes());
    }
    return bytes.empty() || FlushVulkanBuffer(buffer);
}

void DestroyVulkanBuffer(const VulkanContext& context, VulkanBuffer& buffer) noexcept
{
    const VkDevice device = buffer.device != VK_NULL_HANDLE ? buffer.device : context.device;
    if (device != VK_NULL_HANDLE && buffer.mapped != nullptr && buffer.memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, buffer.memory);
    }
    if (device != VK_NULL_HANDLE && buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
    }
    if (device != VK_NULL_HANDLE && buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer.memory, nullptr);
    }
    ResetBuffer(buffer);
}

} // namespace Concord
