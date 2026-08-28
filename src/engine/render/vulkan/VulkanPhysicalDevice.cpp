// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanPhysicalDevice.h"

#include <cstring>
#include <vector>

namespace Concord {

u32 FindGraphicsPresentQueue(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    u32 graphicsPresentFallback = kInvalidQueueFamily;
    for (u32 i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
            continue;
        }

        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported);
        if (presentSupported == VK_TRUE) {
            if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
                return i;
            }
            if (graphicsPresentFallback == kInvalidQueueFamily) {
                graphicsPresentFallback = i;
            }
        }
    }
    return graphicsPresentFallback;
}

bool SupportsVulkanRenderDevice(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3) {
        return false;
    }

    u32 extensionCount = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                             extensions.data()) != VK_SUCCESS) {
        return false;
    }

    bool hasSwapchain = false;
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &dynamicRendering;
    vkGetPhysicalDeviceFeatures2(device, &features);
    return hasSwapchain && dynamicRendering.dynamicRendering == VK_TRUE;
}

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, u32& queueFamilyOut)
{
    u32 count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) {
        queueFamilyOut = kInvalidQueueFamily;
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    u32 fallbackQueue = kInvalidQueueFamily;

    for (VkPhysicalDevice device : devices) {
        const u32 queue = FindGraphicsPresentQueue(device, surface);
        if (queue == kInvalidQueueFamily || !SupportsVulkanRenderDevice(device)) {
            continue;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            queueFamilyOut = queue;
            return device;
        }

        if (fallback == VK_NULL_HANDLE) {
            fallback = device;
            fallbackQueue = queue;
        }
    }

    queueFamilyOut = fallbackQueue;
    return fallback;
}

} // namespace Concord
