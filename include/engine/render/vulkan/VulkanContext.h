// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANCONTEXT_H
#define CONCORD_VULKANCONTEXT_H

#include "engine/core/Types.h"

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * The device-level handles every other Vulkan helper needs.
 *
 * Passed by reference rather than owned, so the helpers stay free functions
 * over plain data instead of a class hierarchy that would have to thread
 * ownership through every call.
 */
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    u32 queueFamily = 0;

    /** Whether the validation layer was actually enabled at instance creation. */
    bool validationEnabled = false;
};

} // namespace Concord

#endif // CONCORD_VULKANCONTEXT_H
