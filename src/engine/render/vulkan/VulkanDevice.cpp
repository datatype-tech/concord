// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDevice.h"

#include "engine/render/vulkan/VulkanPhysicalDevice.h"
#include "engine/render/vulkan/VulkanResult.h"

#include <cstdio>

namespace Concord {

bool CreateVulkanDevice(VulkanContext& context)
{
    context.physicalDevice =
        SelectPhysicalDevice(context.instance, context.surface, context.queueFamily);

    if (context.physicalDevice == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[Concord] no Vulkan device can present to this surface\n");
        return false;
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = context.queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &dynamicRendering;

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;

    const VkResult result = vkCreateDevice(context.physicalDevice, &info, nullptr, &context.device);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDevice", result);
    }

    vkGetDeviceQueue(context.device, context.queueFamily, 0, &context.graphicsQueue);
    return true;
}

void DestroyVulkanDevice(VulkanContext& context)
{
    if (context.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context.device, nullptr);
        context.device = VK_NULL_HANDLE;
    }
}

} // namespace Concord
