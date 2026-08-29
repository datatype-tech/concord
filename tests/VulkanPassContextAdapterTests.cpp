// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CRender.h"
#include "engine/render/VulkanRenderBackendExtensions.h"

#include <cstdint>

namespace {
struct Capture {
    Concord::VulkanPassContext context{};
};

bool CapturePass(const Concord::VulkanPassContext& context, void* userData)
{
    static_cast<Capture*>(userData)->context = context;
    return true;
}
}

int main()
{
    Capture capture;
    Concord::ClearVulkanPasses();
    if (!Concord::RegisterVulkanPass({
            .name = "lifecycle",
            .phase = Concord::VulkanPassPhase::Initialize,
            .callback = CapturePass,
            .userData = &capture})) {
        return 1;
    }

    Concord::VulkanContext context{};
    Concord::VulkanSwapchain swapchain{};
    swapchain.extent = {1280, 720};
    swapchain.images.resize(1);
    swapchain.views.resize(1);
    swapchain.imageLayouts.resize(1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    Concord::VulkanDepthBuffer depth{};
    Concord::VulkanFrame frame{};
    frame.commandBufferRecording = true;
    frame.commandBuffer = Concord::VulkanHandle<VkCommandBuffer>(1);
    if (!Concord::RunVulkanRenderExtensions(
            context, swapchain, depth, frame, 3, 0, VK_NULL_HANDLE, VK_NULL_HANDLE,
            nullptr, 9, Concord::VulkanPassPhase::Initialize)) {
        return 1;
    }
    const auto& observed = capture.context;
    if (observed.phase != Concord::VulkanPassPhase::Initialize ||
        observed.imageIndex != Concord::VulkanPassInvalidImageIndex ||
        observed.colorImage != 0 || observed.colorView != 0 || observed.colorLayout != 0 ||
        observed.depthImage != 0 || observed.depthView != 0 || observed.depthLayout != 0 ||
        observed.frameDescriptorSet != 0 || observed.shadowDescriptorSet != 0 ||
        observed.rayTracingDescriptorSet != 0 || observed.topLevelAccelerationStructure != 0 ||
        observed.featureFlags != 0 || observed.commandBuffer != 0 ||
        observed.commandBufferRecording != 0 || observed.width != 1280 ||
        observed.height != 720) {
        return 1;
    }

    Concord::ClearVulkanPasses();
    if (!Concord::RegisterVulkanPass({
            .name = "frame",
            .phase = Concord::VulkanPassPhase::BeforeScene,
            .callback = CapturePass,
            .userData = &capture})) {
        return 1;
    }
    swapchain.handle = Concord::VulkanHandle<VkSwapchainKHR>(0x11);
    swapchain.images[0] = Concord::VulkanHandle<VkImage>(0x12);
    swapchain.views[0] = Concord::VulkanHandle<VkImageView>(0x13);
    depth.image = Concord::VulkanHandle<VkImage>(0x14);
    depth.view = Concord::VulkanHandle<VkImageView>(0x15);
    depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    frame.commandBuffer = Concord::VulkanHandle<VkCommandBuffer>(0x16);
    frame.commandBufferRecording = true;
    if (!Concord::RunVulkanRenderExtensions(
            context, swapchain, depth, frame, 4, 0, VK_NULL_HANDLE, VK_NULL_HANDLE,
            nullptr, 10, Concord::VulkanPassPhase::BeforeScene)) {
        return 1;
    }
    if (capture.context.imageIndex != 0 || capture.context.colorImage != 0x12 ||
        capture.context.colorView != 0x13 || capture.context.depthImage != 0x14 ||
        capture.context.depthView != 0x15 || capture.context.commandBuffer != 0x16 ||
        capture.context.commandBufferRecording != 1 ||
        (capture.context.featureFlags & Concord::VulkanPassFeatureCommandRecording) == 0 ||
        capture.context.depthLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        return 1;
    }
    Concord::ClearVulkanPasses();
    return 0;
}
