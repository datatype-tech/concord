// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "VulkanRayTracingSceneGpuSupport.h"

#include <vector>

namespace ConcordTest {

bool CreateInstance(VkInstance& instance)
{
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &application;
    return vkCreateInstance(&info, nullptr, &instance) == VK_SUCCESS;
}

Concord::u32 FindGraphicsFamily(VkPhysicalDevice device)
{
    Concord::u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (Concord::u32 i = 0; i < count; ++i) {
        if ((families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            return i;
        }
    }
    return 0xffffffffu;
}

bool CreateRayTracingDevice(VkPhysicalDevice physicalDevice, Concord::u32 queueFamily,
                            VkDevice& device)
{
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = queueFamily;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkPhysicalDeviceBufferDeviceAddressFeatures address{};
    address.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    address.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{};
    acceleration.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration.accelerationStructure = VK_TRUE;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    pipeline.rayTracingPipeline = VK_TRUE;
    address.pNext = &acceleration;
    acceleration.pNext = &pipeline;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &address;
    const char* extensions[] = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    info.enabledExtensionCount = 3;
    info.ppEnabledExtensionNames = extensions;
    return vkCreateDevice(physicalDevice, &info, nullptr, &device) == VK_SUCCESS;
}

} // namespace ConcordTest
