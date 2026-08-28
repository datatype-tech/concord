// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSwapchainResources.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {

bool CreateVulkanSwapchainImageViews(const VulkanContext& context,
                                     VulkanSwapchain& swapchain)
{
    if (swapchain.images.empty()) {
        return false;
    }
    swapchain.views.resize(swapchain.images.size());
    for (usize i = 0; i < swapchain.images.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = swapchain.images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchain.format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        const VkResult result = vkCreateImageView(context.device, &info, nullptr,
                                                  &swapchain.views[i]);
        if (result != VK_SUCCESS) {
            DestroyVulkanSwapchain(context, swapchain);
            return VulkanFailed("vkCreateImageView", result);
        }
    }
    return true;
}

bool CreateVulkanRenderFinishedSemaphores(const VulkanContext& context,
                                          VulkanSwapchain& swapchain)
{
    if (swapchain.images.empty()) {
        return false;
    }
    swapchain.renderFinished.resize(swapchain.images.size(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : swapchain.renderFinished) {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        const VkResult result = vkCreateSemaphore(context.device, &info, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            DestroyVulkanSwapchain(context, swapchain);
            return VulkanFailed("vkCreateSemaphore", result);
        }
    }
    return true;
}

} // namespace Concord
