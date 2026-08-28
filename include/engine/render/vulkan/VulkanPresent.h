// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPRESENT_H
#define CONCORD_VULKANPRESENT_H

#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanSwapchain.h"

namespace Concord {

/**
 * Submits the recorded command buffer to the graphics queue.
 *
 * Waits on the frame's image-available semaphore and signals the semaphore
 * belonging to the acquired image, so presentation can key off the image
 * rather than the frame slot.
 *
 * @return False when the command buffer could not be closed or submitted.
 */
bool SubmitFrame(const VulkanContext& context, const VulkanSwapchain& swapchain,
                 VulkanFrame& frame, u32 imageIndex);

/**
 * Queues the acquired image for presentation.
 *
 * @return True when presentation did not complete normally and the caller
 *         should discard and rebuild the swapchain before acquiring again.
 */
bool PresentFrame(const VulkanContext& context, const VulkanSwapchain& swapchain, u32 imageIndex);

} // namespace Concord

#endif // CONCORD_VULKANPRESENT_H
