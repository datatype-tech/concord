// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSupport.h"

#include <cstdio>
#include <vector>

int main()
{
    Concord::VulkanRayTracingSupport queryOnly{};
    queryOnly.bufferDeviceAddress = true;
    queryOnly.accelerationStructure = true;
    queryOnly.deferredHostOperations = true;
    queryOnly.rayQuery = true;
    if (!queryOnly.IsRayQueryUsable() || queryOnly.IsUsable()) {
        return 1;
    }
    const Concord::VulkanRayTracingSupport empty =
        Concord::QueryVulkanRayTracingSupport(VK_NULL_HANDLE);
    if (empty.IsUsable() || empty.supported || empty.deferredHostOperations || empty.rayQuery ||
        empty.shaderGroupHandleSize != 0 || empty.maxShaderGroupStride != 0 ||
        empty.maxRayRecursionDepth != 0) {
        return 1;
    }
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        return 77;
    }
    Concord::u32 count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    for (VkPhysicalDevice device : devices) {
        const Concord::VulkanRayTracingSupport support =
            Concord::QueryVulkanRayTracingSupport(device);
        std::printf("rayTracing=%s bda=%s as=%s pipeline=%s deferred=%s rayQuery=%s handle=%u align=%u base=%u stride=%u recursion=%u\n",
                    support.IsUsable() ? "usable" : "unavailable",
                    support.bufferDeviceAddress ? "yes" : "no",
                    support.accelerationStructure ? "yes" : "no",
                    support.rayTracingPipeline ? "yes" : "no",
                    support.deferredHostOperations ? "yes" : "no",
                    support.rayQuery ? "yes" : "no", support.shaderGroupHandleSize,
                    support.shaderGroupHandleAlignment, support.shaderGroupBaseAlignment,
                    support.maxShaderGroupStride, support.maxRayRecursionDepth);
    }
    vkDestroyInstance(instance, nullptr);
    return 0;
}
