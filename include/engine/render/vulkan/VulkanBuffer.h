// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANBUFFER_H
#define CONCORD_VULKANBUFFER_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <cstddef>
#include <span>

namespace Concord {

/** Describes a Vulkan buffer allocation and its optional mapping/address flags. */
struct VulkanBufferCreateInfo {
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags requiredMemoryProperties = 0;
    VkMemoryPropertyFlags preferredMemoryProperties = 0;
    bool persistentMap = false;
    bool deviceAddress = false;
};

/** A Vulkan buffer with optional persistent mapping and device address. */
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize allocationSize = 0;
    VkDeviceSize nonCoherentAtomSize = 1;
    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags memoryProperties = 0;
    VkDeviceAddress deviceAddress = 0;
    void* mapped = nullptr;

    /** Whether the Vulkan buffer has a live allocation bound to it. */
    [[nodiscard]] bool IsBound() const noexcept
    {
        return buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE;
    }

    /** Whether the allocation is currently mapped into host address space. */
    [[nodiscard]] bool IsMapped() const noexcept { return mapped != nullptr; }

    /** Whether a valid device address was queried for this buffer. */
    [[nodiscard]] bool HasDeviceAddress() const noexcept
    {
        return IsBound() && deviceAddress != 0;
    }

    /** Returns the cached device address, or zero when address support is absent. */
    [[nodiscard]] VkDeviceAddress GetDeviceAddress() const noexcept { return deviceAddress; }

    /** Whether the buffer can receive an upload through its mapped pointer. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return IsBound() && IsMapped();
    }
};

/** Creates a buffer allocation according to the supplied mapping/address policy. */
bool CreateVulkanBuffer(const VulkanContext& context, const VulkanBufferCreateInfo& info,
                        VulkanBuffer& buffer);

/** Creates a host-visible buffer and persistently maps its allocation. */
bool CreateVulkanHostBuffer(const VulkanContext& context, VkDeviceSize size,
                            VkBufferUsageFlags usage, VulkanBuffer& buffer);

/** Copies bytes into a persistently mapped buffer and flushes non-coherent memory. */
bool UploadVulkanBuffer(VulkanBuffer& buffer, std::span<const std::byte> bytes) noexcept;

/** Flushes the logical buffer range for host-to-device visibility. */
bool FlushVulkanBuffer(VulkanBuffer& buffer) noexcept;

/** Invalidates the logical buffer range for device-to-host visibility. */
bool InvalidateVulkanBuffer(VulkanBuffer& buffer) noexcept;

/** Releases the buffer and its mapped memory. */
void DestroyVulkanBuffer(const VulkanContext& context, VulkanBuffer& buffer) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANBUFFER_H
