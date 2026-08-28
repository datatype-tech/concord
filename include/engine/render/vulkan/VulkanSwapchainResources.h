// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSWAPCHAINRESOURCES_H
#define CONCORD_VULKANSWAPCHAINRESOURCES_H

#include "engine/render/vulkan/VulkanSwapchain.h"

namespace Concord {

/** Creates image views for every image in a candidate swapchain. */
bool CreateVulkanSwapchainImageViews(const VulkanContext& context,
                                     VulkanSwapchain& swapchain);

/** Creates one presentation semaphore for every candidate swapchain image. */
bool CreateVulkanRenderFinishedSemaphores(const VulkanContext& context,
                                          VulkanSwapchain& swapchain);

} // namespace Concord

#endif // CONCORD_VULKANSWAPCHAINRESOURCES_H
