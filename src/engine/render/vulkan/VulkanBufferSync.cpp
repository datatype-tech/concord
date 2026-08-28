// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBuffer.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <cstdio>

namespace Concord {
namespace {

/** Reports a synchronization request that cannot reach Vulkan. */
bool SynchronizationFailed(const char* reason) noexcept
{
    std::fprintf(stderr, "[Concord] VulkanBuffer: %s\n", reason);
    return false;
}

/** Rounds a logical range to the atom size required by non-coherent memory. */
VkDeviceSize AlignedRangeSize(const VulkanBuffer& buffer) noexcept
{
    if (buffer.size == 0 || buffer.allocationSize < buffer.size) {
        return 0;
    }
    const VkDeviceSize atom = buffer.nonCoherentAtomSize == 0 ? 1 : buffer.nonCoherentAtomSize;
    const VkDeviceSize remainder = buffer.size % atom;
    const VkDeviceSize padding = remainder == 0 ? 0 : atom - remainder;
    return padding > buffer.allocationSize - buffer.size ? buffer.allocationSize
                                                         : buffer.size + padding;
}

/** Flushes or invalidates a mapped range as requested by the caller. */
bool SynchronizeVulkanBuffer(VulkanBuffer& buffer, bool invalidate) noexcept
{
    if (!buffer.IsReady()) {
        return SynchronizationFailed("synchronization requested for an uninitialized buffer");
    }
    if ((buffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
        return true;
    }
    if (buffer.device == VK_NULL_HANDLE) {
        return SynchronizationFailed("non-coherent buffer has no device handle");
    }
    if (buffer.allocationSize < buffer.size) {
        return SynchronizationFailed("mapped allocation is smaller than the logical buffer");
    }
    const VkDeviceSize rangeSize = AlignedRangeSize(buffer);
    if (rangeSize == 0) {
        return true;
    }
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = buffer.memory;
    range.offset = 0;
    range.size = rangeSize;
    const VkResult result = invalidate ? vkInvalidateMappedMemoryRanges(buffer.device, 1, &range)
                                       : vkFlushMappedMemoryRanges(buffer.device, 1, &range);
    if (result != VK_SUCCESS) {
        return VulkanFailed(invalidate ? "vkInvalidateMappedMemoryRanges"
                                       : "vkFlushMappedMemoryRanges",
                            result);
    }
    return true;
}

} // namespace

bool FlushVulkanBuffer(VulkanBuffer& buffer) noexcept
{
    return SynchronizeVulkanBuffer(buffer, false);
}

bool InvalidateVulkanBuffer(VulkanBuffer& buffer) noexcept
{
    return SynchronizeVulkanBuffer(buffer, true);
}

} // namespace Concord
