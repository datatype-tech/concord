// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANDEVICE_H
#define CONCORD_VULKANDEVICE_H

#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/**
 * Selects a physical device and creates the logical device and queue.
 *
 * Requires `context.instance` and `context.surface` to be valid, and fills
 * in `physicalDevice`, `device`, `graphicsQueue` and `queueFamily`.
 *
 * Enables dynamic rendering, which lets the backend draw without
 * VkRenderPass or VkFramebuffer objects.
 *
 * @return False when no device qualifies or creation failed.
 */
bool CreateVulkanDevice(VulkanContext& context);

/** Destroys the logical device and clears the handle. */
void DestroyVulkanDevice(VulkanContext& context);

} // namespace Concord

#endif // CONCORD_VULKANDEVICE_H
