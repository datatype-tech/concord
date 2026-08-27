// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/core/Color.h"
#include "engine/ecs/Components.h"
#include "engine/render/vulkan/VulkanClearPass.h"
#include "engine/render/vulkan/VulkanDevice.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanImageBarrier.h"
#include "engine/render/vulkan/VulkanInstance.h"
#include "engine/render/vulkan/VulkanPresent.h"
#include "engine/render/vulkan/VulkanSurface.h"
#include "engine/render/vulkan/VulkanSwapchain.h"
#include "engine/scene/Scene.h"
#include "engine/window/Window.h"

#include <limits>

namespace Concord {

/**
 * Owns the Vulkan objects and the index of the image being drawn.
 *
 * Each object's creation and destruction lives in its own unit under
 * `render/vulkan`; this struct only holds them together and tracks where in
 * the frame the backend currently is.
 */
struct VulkanRenderBackend::Impl {
    Window* window = nullptr;
    VulkanContext context{};
    VulkanSwapchain swapchain{};
    VulkanFrameRing frames{};

    u32 imageIndex = 0;
    bool frameActive = false;

    /** Rebuilds the swapchain against the window's current size. */
    bool RecreateSwapchain()
    {
        vkDeviceWaitIdle(context.device);
        DestroyVulkanSwapchain(context, swapchain);
        return CreateVulkanSwapchain(context, swapchain, window->Width(), window->Height(),
                                     window->Vsync());
    }
};

VulkanRenderBackend::VulkanRenderBackend() : m_impl(std::make_unique<Impl>()) {}

VulkanRenderBackend::~VulkanRenderBackend() { Shutdown(); }

bool VulkanRenderBackend::Init(Window& window, bool enableValidation)
{
    Impl& impl = *m_impl;
    impl.window = &window;

    return CreateVulkanInstance(impl.context, enableValidation) &&
           CreateVulkanSurface(impl.context, window) &&
           CreateVulkanDevice(impl.context) &&
           CreateVulkanSwapchain(impl.context, impl.swapchain, window.Width(), window.Height(),
                                 window.Vsync()) &&
           CreateVulkanFrameRing(impl.context, impl.frames);
}

void VulkanRenderBackend::Shutdown()
{
    if (!m_impl) {
        return;
    }
    Impl& impl = *m_impl;

    if (impl.context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl.context.device);
        DestroyVulkanFrameRing(impl.context, impl.frames);
        DestroyVulkanSwapchain(impl.context, impl.swapchain);
        DestroyVulkanDevice(impl.context);
    }

    DestroyVulkanSurface(impl.context);
    DestroyVulkanInstance(impl.context);
}

bool VulkanRenderBackend::BeginFrame()
{
    Impl& impl = *m_impl;
    if (impl.context.device == VK_NULL_HANDLE || impl.swapchain.handle == VK_NULL_HANDLE) {
        return false;
    }

    if (impl.window->ConsumeResizeFlag() && !impl.RecreateSwapchain()) {
        return false;
    }

    VulkanFrame& frame = impl.frames.Current();
    vkWaitForFences(impl.context.device, 1, &frame.inFlight, VK_TRUE,
                    std::numeric_limits<u64>::max());

    const VkResult acquire = vkAcquireNextImageKHR(
        impl.context.device, impl.swapchain.handle, std::numeric_limits<u64>::max(),
        frame.imageAvailable, VK_NULL_HANDLE, &impl.imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        impl.RecreateSwapchain();
        return false;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    vkResetFences(impl.context.device, 1, &frame.inFlight);
    vkResetCommandBuffer(frame.commandBuffer, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(frame.commandBuffer, &begin) != VK_SUCCESS) {
        return false;
    }

    impl.frameActive = true;
    return true;
}

void VulkanRenderBackend::DrawScene(const Scene& scene)
{
    Impl& impl = *m_impl;
    if (!impl.frameActive) {
        return;
    }

    const VkCommandBuffer commandBuffer = impl.frames.Current().commandBuffer;
    const VkImage image = impl.swapchain.images[impl.imageIndex];

    TransitionToColorAttachment(commandBuffer, image);
    RecordClearPass(commandBuffer, impl.swapchain.views[impl.imageIndex], impl.swapchain.extent,
                    ToLinear(scene.Environment().skyColor));
    TransitionToPresent(commandBuffer, image);
}

void VulkanRenderBackend::EndFrame()
{
    Impl& impl = *m_impl;
    if (!impl.frameActive) {
        return;
    }
    impl.frameActive = false;

    if (!SubmitFrame(impl.context, impl.swapchain, impl.frames.Current(), impl.imageIndex)) {
        return;
    }

    if (PresentFrame(impl.context, impl.swapchain, impl.imageIndex)) {
        impl.RecreateSwapchain();
    }

    impl.frames.Advance();
}

void VulkanRenderBackend::WaitIdle()
{
    if (m_impl && m_impl->context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_impl->context.device);
    }
}

} // namespace Concord