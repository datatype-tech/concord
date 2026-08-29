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
void TransitionToColorAttachment(VkCommandBuffer commandBuffer, VkImage image,
                                 VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED);

/** Transitions a depth image into the layout used by dynamic rendering. */
void TransitionToDepthAttachment(VkCommandBuffer commandBuffer, VkImage image,
                                 VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED);

/** Orders depth writes from adjacent dynamic-rendering passes. */
void InsertDepthWriteBarrier(VkCommandBuffer commandBuffer, VkImage image);

/** Orders color attachment writes before a following load/store pass. */
void InsertColorWriteBarrier(VkCommandBuffer commandBuffer, VkImage image);

/**
 * Transitions a swapchain image from colour attachment to the layout the
 * presentation engine requires.
 */
void TransitionToPresent(VkCommandBuffer commandBuffer, VkImage image,
                         VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

} // namespace Concord

#endif // CONCORD_VULKANIMAGEBARRIER_H
