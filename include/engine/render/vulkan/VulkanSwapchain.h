// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSWAPCHAIN_H
#define CONCORD_VULKANSWAPCHAIN_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <vector>

namespace Concord {

/**
 * The presentable images and their views.
 *
 * Also owns one render-finished semaphore per image rather than per frame in
 * flight: presentation waits on the semaphore belonging to the image being
 * shown, and image indices need not follow frame order.
 */
struct VulkanSwapchain {
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> renderFinished;
};

/**
 * Creates the swapchain sized to the surface's current extent.
 *
 * @param width  Fallback width used when the surface reports no fixed extent.
 * @param height Fallback height for the same case.
 * @param vsync  Requested presentation policy.
 * @return False when creation failed or the window has zero area.
 */
bool CreateVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain,
                           u32 width, u32 height, bool vsync);

/** Destroys the swapchain, its views and its semaphores. */
void DestroyVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain);

} // namespace Concord

#endif // CONCORD_VULKANSWAPCHAIN_H
