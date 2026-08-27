// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSURFACEFORMAT_H
#define CONCORD_VULKANSURFACEFORMAT_H

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * Chooses the swapchain's color format.
 *
 * Prefers 8-bit BGRA in sRGB so the hardware performs the encode on write;
 * falls back to whatever the surface advertises first.
 */
VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface);

/**
 * Chooses the presentation mode for the requested vsync policy.
 *
 * FIFO is the only mode required to exist, so it backs the vsync-on path and
 * every fallback. With vsync off, mailbox is preferred over immediate: it
 * drops stale frames instead of tearing.
 */
VkPresentModeKHR SelectPresentMode(VkPhysicalDevice device, VkSurfaceKHR surface, bool vsync);

} // namespace Concord

#endif // CONCORD_VULKANSURFACEFORMAT_H
