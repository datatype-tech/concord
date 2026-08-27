// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANIMAGEBARRIER_H
#define CONCORD_VULKANIMAGEBARRIER_H

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * Transitions a swapchain image from its undefined state to a colour
 * attachment, ready to be rendered into.
 */
void TransitionToColorAttachment(VkCommandBuffer commandBuffer, VkImage image);

/**
 * Transitions a swapchain image from colour attachment to the layout the
 * presentation engine requires.
 */
void TransitionToPresent(VkCommandBuffer commandBuffer, VkImage image);

} // namespace Concord

#endif // CONCORD_VULKANIMAGEBARRIER_H
