// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "VulkanRayTracingSceneGpuSupport.h"

#include "engine/render/vulkan/VulkanRayTracingScene.h"
#include "engine/render/vulkan/VulkanRayTracingSupport.h"

#include <vector>

int main()
{
    VkInstance instance = VK_NULL_HANDLE;
    if (!ConcordTest::CreateInstance(instance)) {
        return 77;
    }
    Concord::u32 count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Concord::VulkanRayTracingSupport support{};
    Concord::u32 queueFamily = 0xffffffffu;
    for (VkPhysicalDevice candidate : devices) {
        const auto candidateSupport = Concord::QueryVulkanRayTracingSupport(candidate);
        const Concord::u32 family = ConcordTest::FindGraphicsFamily(candidate);
        if (candidateSupport.IsUsable() && family != 0xffffffffu) {
            physicalDevice = candidate;
            support = candidateSupport;
            queueFamily = family;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (!ConcordTest::CreateRayTracingDevice(physicalDevice, queueFamily, device)) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    Concord::VulkanContext context{};
    context.instance = instance;
    context.physicalDevice = physicalDevice;
    context.device = device;
    context.queueFamily = queueFamily;
    context.rayTracing = support;
    Concord::VulkanRayTracingScene scene{};
    int status = Concord::CreateVulkanRayTracingScene(context, scene) ? 0 : 1;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (status == 0) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamily;
        status = vkCreateCommandPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS ? 0 : 1;
    }
    if (status == 0) {
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        status = vkAllocateCommandBuffers(device, &allocate, &command) == VK_SUCCESS ? 0 : 1;
    }
    if (status == 0) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        status = vkBeginCommandBuffer(command, &begin) == VK_SUCCESS ? 0 : 1;
        if (status == 0 && !Concord::RecordVulkanRayTracingSceneBuild(command, scene)) {
            status = 1;
        }
        if (status == 0) {
            Concord::InsertVulkanRayTracingSceneReadBarrier(
                command, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            status = vkEndCommandBuffer(command) == VK_SUCCESS ? 0 : 1;
        }
    }
    if (status == 0) {
        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, queueFamily, 0, &queue);
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        status = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS ? 0 : 1;
        if (status == 0) {
            status = vkQueueWaitIdle(queue) == VK_SUCCESS ? 0 : 1;
        }
    }
    Concord::DestroyVulkanRayTracingScene(context, scene);
    if (pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pool, nullptr);
    }
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return status;
}
