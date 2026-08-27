// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSwapchain.h"

#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanSurfaceFormat.h"

#include <algorithm>
#include <limits>

namespace Concord {

namespace {

/** Resolves the swapchain extent, honouring a surface that dictates its own. */
VkExtent2D ResolveExtent(const VkSurfaceCapabilitiesKHR& caps, u32 width, u32 height)
{
    if (caps.currentExtent.width != std::numeric_limits<u32>::max()) {
        return caps.currentExtent;
    }
    return VkExtent2D{
        std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
}

/** Creates one view per swapchain image. */
bool CreateImageViews(const VulkanContext& context, VulkanSwapchain& swapchain)
{
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

        const VkResult result = vkCreateImageView(context.device, &info, nullptr, &swapchain.views[i]);
        if (result != VK_SUCCESS) {
            return VulkanFailed("vkCreateImageView", result);
        }
    }
    return true;
}

/** Creates the per-image semaphore presentation waits on. */
bool CreateRenderFinishedSemaphores(const VulkanContext& context, VulkanSwapchain& swapchain)
{
    swapchain.renderFinished.resize(swapchain.images.size(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : swapchain.renderFinished) {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        const VkResult result = vkCreateSemaphore(context.device, &info, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            return VulkanFailed("vkCreateSemaphore", result);
        }
    }
    return true;
}

} // namespace

bool CreateVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain,
                           u32 width, u32 height, bool vsync)
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &caps);

    const VkSurfaceFormatKHR format = SelectSurfaceFormat(context.physicalDevice, context.surface);
    swapchain.extent = ResolveExtent(caps, width, height);
    if (swapchain.extent.width == 0 || swapchain.extent.height == 0) {
        return false;
    }

    u32 imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = context.surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = swapchain.extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = SelectPresentMode(context.physicalDevice, context.surface, vsync);
    info.clipped = VK_TRUE;

    const VkResult result = vkCreateSwapchainKHR(context.device, &info, nullptr, &swapchain.handle);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateSwapchainKHR", result);
    }

    swapchain.format = format.format;

    u32 count = 0;
    vkGetSwapchainImagesKHR(context.device, swapchain.handle, &count, nullptr);
    swapchain.images.resize(count);
    vkGetSwapchainImagesKHR(context.device, swapchain.handle, &count, swapchain.images.data());

    return CreateImageViews(context, swapchain) && CreateRenderFinishedSemaphores(context, swapchain);
}

void DestroyVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain)
{
    for (VkSemaphore semaphore : swapchain.renderFinished) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context.device, semaphore, nullptr);
        }
    }
    swapchain.renderFinished.clear();

    for (VkImageView view : swapchain.views) {
        vkDestroyImageView(context.device, view, nullptr);
    }
    swapchain.views.clear();
    swapchain.images.clear();

    if (swapchain.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context.device, swapchain.handle, nullptr);
        swapchain.handle = VK_NULL_HANDLE;
    }
}

} // namespace Concord
