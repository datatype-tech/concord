// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPHYSICALDEVICE_H
#define CONCORD_VULKANPHYSICALDEVICE_H

#include "engine/core/Types.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Index meaning "no queue family on this device qualifies". */
inline constexpr u32 kInvalidQueueFamily = 0xFFFFFFFFu;

/**
 * Finds a queue family supporting both graphics and presentation.
 *
 * A single combined family is required rather than tolerating split
 * families: every desktop GPU offers one, and assuming it keeps submission
 * and presentation on one queue.
 *
 * @return The family index, or `kInvalidQueueFamily` when none qualifies.
 */
u32 FindGraphicsPresentQueue(VkPhysicalDevice device, VkSurfaceKHR surface);

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
