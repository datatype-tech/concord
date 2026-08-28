// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameSync.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {

namespace {

/** Creates the per-frame semaphore and fence. */
bool CreateFrameSyncObjects(const VulkanContext& context, VulkanFrame& frame)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    const VkResult semaphoreResult =
        vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &frame.imageAvailable);
    if (semaphoreResult != VK_SUCCESS) {
        return VulkanFailed("vkCreateSemaphore", semaphoreResult);
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    const VkResult fenceResult = vkCreateFence(context.device, &fenceInfo, nullptr, &frame.inFlight);
    if (fenceResult != VK_SUCCESS) {
        vkDestroySemaphore(context.device, frame.imageAvailable, nullptr);
        frame.imageAvailable = VK_NULL_HANDLE;
        return VulkanFailed("vkCreateFence", fenceResult);
    }
    return true;
}

} // namespace

bool CreateVulkanFrameRing(const VulkanContext& context, VulkanFrameRing& ring)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context.queueFamily;

    const VkResult poolResult =
        vkCreateCommandPool(context.device, &poolInfo, nullptr, &ring.commandPool);
    if (poolResult != VK_SUCCESS) {
        return VulkanFailed("vkCreateCommandPool", poolResult);
    }

    for (VulkanFrame& frame : ring.frames) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ring.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        const VkResult allocResult =
            vkAllocateCommandBuffers(context.device, &allocInfo, &frame.commandBuffer);
        if (allocResult != VK_SUCCESS) {
            DestroyVulkanFrameRing(context, ring);
            return VulkanFailed("vkAllocateCommandBuffers", allocResult);
        }

        if (!CreateFrameSyncObjects(context, frame)) {
            DestroyVulkanFrameRing(context, ring);
            return false;
        }
    }
    return true;
}

bool RecoverVulkanFrame(const VulkanContext& context, VulkanFrame& frame)
{
    if (context.device == VK_NULL_HANDLE) {
        return false;
    }

    const VkResult idleResult = vkDeviceWaitIdle(context.device);
    if (idleResult != VK_SUCCESS) {
        return VulkanFailed("vkDeviceWaitIdle (recover frame)", idleResult);
    }
    if (frame.commandBuffer != VK_NULL_HANDLE && frame.commandBufferRecording) {
        const VkResult endResult = vkEndCommandBuffer(frame.commandBuffer);
        frame.commandBufferRecording = false;
        if (endResult != VK_SUCCESS) {
            VulkanFailed("vkEndCommandBuffer (recover frame)", endResult);
        }
    }
    if (frame.commandBuffer != VK_NULL_HANDLE) {
        const VkResult resetResult = vkResetCommandBuffer(frame.commandBuffer, 0);
        if (resetResult != VK_SUCCESS) {
            return VulkanFailed("vkResetCommandBuffer (recover frame)", resetResult);
        }
    }

    VulkanFrame replacement{};
    if (!CreateFrameSyncObjects(context, replacement)) {
        return false;
    }
    if (frame.imageAvailable != VK_NULL_HANDLE) {
        vkDestroySemaphore(context.device, frame.imageAvailable, nullptr);
    }
    if (frame.inFlight != VK_NULL_HANDLE) {
        vkDestroyFence(context.device, frame.inFlight, nullptr);
    }
    frame.imageAvailable = replacement.imageAvailable;
    frame.inFlight = replacement.inFlight;
    frame.commandBufferRecording = false;
    return true;
}

void DestroyVulkanFrameRing(const VulkanContext& context, VulkanFrameRing& ring)
{
    for (VulkanFrame& frame : ring.frames) {
        if (frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(context.device, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(context.device, frame.inFlight, nullptr);
            frame.inFlight = VK_NULL_HANDLE;
        }
        frame.commandBuffer = VK_NULL_HANDLE;
        frame.commandBufferRecording = false;
    }

    if (ring.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context.device, ring.commandPool, nullptr);
        ring.commandPool = VK_NULL_HANDLE;
    }
}

} // namespace Concord
