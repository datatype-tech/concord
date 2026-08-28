// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDevice.h"

#include "engine/render/vulkan/VulkanPhysicalDevice.h"
#include "engine/render/vulkan/VulkanResult.h"

#include <cstdio>
#include <vector>

namespace Concord {
namespace {

/** Creates a logical device with the requested optional ray-tracing chain. */
VkResult CreateLogicalDevice(VulkanContext& context, const VulkanRayTracingSupport& support,
                             bool enableRayTracing, bool enableRayQuery)
{
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = context.queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceDynamicRenderingFeatures dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &dynamic;

    VkPhysicalDeviceBufferDeviceAddressFeatures address{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipeline{};
    VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{};
    const bool pipelineEnabled = enableRayTracing && support.IsUsable();
    const bool queryEnabled = enableRayQuery && support.IsRayQueryUsable();
    void* chain = nullptr;
    if (queryEnabled) {
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        rayQuery.rayQuery = VK_TRUE;
        chain = &rayQuery;
    }
    if (pipelineEnabled) {
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        pipeline.rayTracingPipeline = VK_TRUE;
        pipeline.pNext = chain;
        chain = &pipeline;
    }
    if (pipelineEnabled || queryEnabled) {
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        address.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        address.bufferDeviceAddress = VK_TRUE;
        acceleration.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        acceleration.accelerationStructure = VK_TRUE;
        acceleration.pNext = chain;
        address.pNext = &acceleration;
        dynamic.pNext = &address;
    }

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = static_cast<u32>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    return vkCreateDevice(context.physicalDevice, &info, nullptr, &context.device);
}

} // namespace

bool CreateVulkanDevice(VulkanContext& context)
{
    context.physicalDevice =
        SelectPhysicalDevice(context.instance, context.surface, context.queueFamily);

    if (context.physicalDevice == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[Concord] no Vulkan 1.3 device supports presentation, "
                             "swapchain, and dynamic rendering\n");
        return false;
    }

    context.rayTracing = QueryVulkanRayTracingSupport(context.physicalDevice);
    if (!QueueFamilySupportsCompute(context.physicalDevice, context.queueFamily)) {
        if (context.rayTracing.IsRayQueryUsable() || context.rayTracing.IsUsable()) {
            std::fprintf(stderr,
                         "[Concord] selected Vulkan queue lacks compute; disabling ray tracing\n");
        }
        context.rayTracing = {};
    }
    const bool tryRayTracing = context.rayTracing.IsUsable();
    const bool tryRayQuery = context.rayTracing.IsRayQueryUsable();
    VkResult result =
        CreateLogicalDevice(context, context.rayTracing, tryRayTracing, tryRayQuery);
    if (result != VK_SUCCESS && (tryRayTracing || tryRayQuery)) {
        std::fprintf(stderr,
                     "[Concord] ray-tracing device features rejected; falling back to raster\n");
        context.device = VK_NULL_HANDLE;
        context.rayTracing = {};
        result = CreateLogicalDevice(context, {}, false, false);
    }
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDevice", result);
    }

    vkGetDeviceQueue(context.device, context.queueFamily, 0, &context.graphicsQueue);
    if (tryRayTracing && context.rayTracing.IsUsable()) {
        std::fprintf(stderr, "[Concord] Vulkan ray tracing enabled (recursion=%u)\n",
                     context.rayTracing.maxRayRecursionDepth);
    }
    if (!tryRayTracing && tryRayQuery && context.rayTracing.IsRayQueryUsable()) {
        std::fprintf(stderr, "[Concord] Vulkan ray query enabled\n");
    }
    return true;
}

void DestroyVulkanDevice(VulkanContext& context)
{
    if (context.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context.device, nullptr);
        context.device = VK_NULL_HANDLE;
    }
    context.rayTracing = {};
}

} // namespace Concord
