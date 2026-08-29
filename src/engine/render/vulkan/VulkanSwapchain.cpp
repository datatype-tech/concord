// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSwapchain.h"

#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanSurfaceFormat.h"
#include "engine/render/vulkan/VulkanSwapchainResources.h"

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
    return VkExtent2D{std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                      std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)};
}
} // namespace
bool CreateVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain,
                           u32 width, u32 height, bool vsync, VkSwapchainKHR oldSwapchain)
{
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE ||
        context.surface == VK_NULL_HANDLE) {
        return false;
    }
    VkSurfaceCapabilitiesKHR caps{};
    const VkResult capsResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        context.physicalDevice, context.surface, &caps);
    if (capsResult != VK_SUCCESS) {
        return VulkanFailed("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", capsResult);
    }
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
    swapchain.transferDestinationSupported =
        (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    if (swapchain.transferDestinationSupported) {
        info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = SelectPresentMode(context.physicalDevice, context.surface, vsync);
    info.clipped = VK_TRUE;
    info.oldSwapchain = oldSwapchain;
    const VkResult result = vkCreateSwapchainKHR(context.device, &info, nullptr, &swapchain.handle);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateSwapchainKHR", result);
    }

    swapchain.format = format.format;
    u32 count = 0;
    const VkResult countResult =
        vkGetSwapchainImagesKHR(context.device, swapchain.handle, &count, nullptr);
    if (countResult != VK_SUCCESS || count == 0) {
        DestroyVulkanSwapchain(context, swapchain);
        return countResult == VK_SUCCESS ? false : VulkanFailed("vkGetSwapchainImagesKHR", countResult);
    }
    swapchain.images.resize(count);
    const VkResult imagesResult = vkGetSwapchainImagesKHR(
        context.device, swapchain.handle, &count, swapchain.images.data());
    if (imagesResult != VK_SUCCESS && imagesResult != VK_INCOMPLETE) {
        DestroyVulkanSwapchain(context, swapchain);
        return VulkanFailed("vkGetSwapchainImagesKHR", imagesResult);
    }
    swapchain.images.resize(count);
    swapchain.imageLayouts.assign(swapchain.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    if (!CreateVulkanSwapchainImageViews(context, swapchain) ||
        !CreateVulkanRenderFinishedSemaphores(context, swapchain)) {
        DestroyVulkanSwapchain(context, swapchain);
        return false;
    }
    swapchain.imagesInFlight.assign(swapchain.images.size(), VK_NULL_HANDLE);
    return true;
}
void DestroyVulkanSwapchain(const VulkanContext& context, VulkanSwapchain& swapchain)
{
    if (context.device != VK_NULL_HANDLE) {
        for (VkSemaphore semaphore : swapchain.renderFinished) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(context.device, semaphore, nullptr);
            }
        }

        for (VkImageView view : swapchain.views) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(context.device, view, nullptr);
            }
        }
        if (swapchain.handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(context.device, swapchain.handle, nullptr);
        }
    }
    swapchain.renderFinished.clear();
    swapchain.imagesInFlight.clear();
    swapchain.imageLayouts.clear();
    swapchain.views.clear();
    swapchain.images.clear();
    swapchain.handle = VK_NULL_HANDLE;
    swapchain.format = VK_FORMAT_UNDEFINED;
    swapchain.extent = {};
    swapchain.transferDestinationSupported = false;
}

} // namespace Concord
