// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPHYSICALDEVICE_H
#define CONCORD_VULKANPHYSICALDEVICE_H

#include "engine/core/Types.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace Concord {

/** Index meaning "no queue family on this device qualifies". */
inline constexpr u32 kInvalidQueueFamily = 0xFFFFFFFFu;

/**
 * Finds a queue family supporting graphics and presentation.
 *
 * A single combined family is required rather than tolerating split
 * families: every desktop GPU offers one, and assuming it keeps submission
 * and presentation on one queue. A compute-capable candidate is preferred so
 * optional acceleration-structure and tile-culling commands can share it.
 *
 * @return The family index, or `kInvalidQueueFamily` when none qualifies.
 */
u32 FindGraphicsPresentQueue(VkPhysicalDevice device, VkSurfaceKHR surface);

/** Whether a selected queue family can record acceleration-structure builds. */
inline bool QueueFamilySupportsCompute(VkPhysicalDevice device, u32 queueFamily) noexcept
{
    if (device == VK_NULL_HANDLE || queueFamily == kInvalidQueueFamily) {
        return false;
    }
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (queueFamily >= count) {
        return false;
    }
    try {
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
        return (families[queueFamily].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
    } catch (...) {
        return false;
    }
}

/** Whether a device satisfies the backend's Vulkan 1.3 render requirements. */
bool SupportsVulkanRenderDevice(VkPhysicalDevice device);

/**
 * Picks the best device that can present to `surface`.
 *
 * Prefers a discrete GPU, falling back to the first device that qualifies.
 *
 * @param queueFamilyOut Receives the chosen device's queue family index.
 * @return The chosen device, or VK_NULL_HANDLE when none can present.
 */
VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, u32& queueFamilyOut);

} // namespace Concord

#endif // CONCORD_VULKANPHYSICALDEVICE_H
