// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/vulkan/VulkanPresent.h"
#include "engine/window/WindowAccess.h"

#include <limits>

namespace Concord {

bool VulkanRenderBackend::BeginFrame()
{
    Impl& impl = *m_impl;
    if (impl.context.device == VK_NULL_HANDLE || !impl.window || !impl.frameSyncReady) {
        return false;
    }
    if (impl.imageAcquirePending) {
        impl.frameSyncReady = false;
        return false;
    }

    if (WindowAccess::ConsumeResizeFlag(*impl.window)) {
        impl.swapchainDirty = true;
    }
    if (impl.swapchainDirty || impl.swapchain.handle == VK_NULL_HANDLE) {
        if (!impl.RecreateSwapchain()) {
            return false;
        }
    }

    VulkanFrame& frame = impl.frames.Current();
    if (frame.commandBuffer == VK_NULL_HANDLE || frame.imageAvailable == VK_NULL_HANDLE ||
        frame.inFlight == VK_NULL_HANDLE) {
        impl.frameSyncReady = false;
        return false;
    }
    const VkResult waitResult = vkWaitForFences(
        impl.context.device, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<u64>::max());
    if (waitResult != VK_SUCCESS) {
        VulkanFailed("vkWaitForFences", waitResult);
        impl.frameSyncReady = false;
        return false;
    }
    const VkResult acquire = vkAcquireNextImageKHR(
        impl.context.device, impl.swapchain.handle, std::numeric_limits<u64>::max(),
        frame.imageAvailable, VK_NULL_HANDLE, &impl.imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        impl.swapchainDirty = true;
        impl.RecreateSwapchain();
        return false;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        VulkanFailed("vkAcquireNextImageKHR", acquire);
        impl.frameSyncReady = false;
        return false;
    }
    impl.imageAcquirePending = true;

    if (impl.imageIndex >= impl.swapchain.imagesInFlight.size()) {
        impl.HandleFrameFailure(frame);
        return false;
    }
    impl.acquiredImageFence = impl.swapchain.imagesInFlight[impl.imageIndex];
    if (impl.acquiredImageFence != VK_NULL_HANDLE &&
        impl.acquiredImageFence != frame.inFlight) {
        const VkResult imageWait =
            vkWaitForFences(impl.context.device, 1, &impl.acquiredImageFence, VK_TRUE,
                            std::numeric_limits<u64>::max());
        if (imageWait != VK_SUCCESS) {
            VulkanFailed("vkWaitForFences (swapchain image)", imageWait);
            impl.HandleFrameFailure(frame);
            return false;
        }
    }
    const VkResult resetResult = vkResetCommandBuffer(frame.commandBuffer, 0);
    if (resetResult != VK_SUCCESS) {
        VulkanFailed("vkResetCommandBuffer", resetResult);
        impl.HandleFrameFailure(frame);
        return false;
    }
    frame.commandBufferRecording = false;

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkResult beginResult = vkBeginCommandBuffer(frame.commandBuffer, &begin);
    if (beginResult != VK_SUCCESS) {
        VulkanFailed("vkBeginCommandBuffer", beginResult);
        impl.HandleFrameFailure(frame);
        return false;
    }
    frame.commandBufferRecording = true;

    if (acquire == VK_SUBOPTIMAL_KHR) {
        impl.swapchainDirty = true;
    }
    impl.frameActive = true;
    return true;
}

void VulkanRenderBackend::EndFrame()
{
    Impl& impl = *m_impl;
    if (!impl.frameActive) {
        return;
    }
    impl.frameActive = false;

    if (!SubmitFrame(impl.context, impl.swapchain, impl.frames.Current(), impl.imageIndex)) {
        impl.HandleFrameFailure(impl.frames.Current());
        return;
    }
    impl.imageAcquirePending = false;
    if (impl.imageIndex < impl.swapchain.imagesInFlight.size()) {
        impl.swapchain.imagesInFlight[impl.imageIndex] = impl.frames.Current().inFlight;
    } else {
        impl.swapchainDirty = true;
    }
    impl.acquiredImageFence = VK_NULL_HANDLE;

    if (PresentFrame(impl.context, impl.swapchain, impl.imageIndex)) {
        impl.swapchainDirty = true;
    }
    if (impl.swapchainDirty) {
        impl.RecreateSwapchain();
    }
    impl.frames.Advance();
}

void VulkanRenderBackend::WaitIdle()
{
    if (!m_impl || m_impl->context.device == VK_NULL_HANDLE) {
        return;
    }
    if (m_impl->frameActive) {
        m_impl->AbortFrame();
        return;
    }
    const VkResult idleResult = vkDeviceWaitIdle(m_impl->context.device);
    if (idleResult != VK_SUCCESS) {
        VulkanFailed("vkDeviceWaitIdle", idleResult);
        m_impl->frameSyncReady = false;
    }
}

} // namespace Concord
