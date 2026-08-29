// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"

#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanSwapchain.h"

#include <utility>

namespace Concord {

bool VulkanRenderBackend::Impl::RecreateSwapchain()
{
    if (!window || context.device == VK_NULL_HANDLE || window->Width() == 0 ||
        window->Height() == 0) {
        return false;
    }
    if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
        frameSyncReady = false;
        return false;
    }
    VulkanSwapchain replacement{};
    VulkanDepthBuffer depthReplacement[kMaxFramesInFlight]{};
    VulkanBoxPipeline boxReplacement{};
    VulkanRayTracingOutputRing outputReplacement{};
    if (!CreateVulkanSwapchain(context, replacement, window->Width(), window->Height(),
                               window->Vsync(), swapchain.handle)) {
        return false;
    }
    for (u32 index = 0; index < kMaxFramesInFlight; ++index) {
        if (!CreateVulkanDepthBuffer(context, depthReplacement[index], replacement.extent)) {
            for (u32 cleanup = 0; cleanup <= index; ++cleanup) {
                DestroyVulkanDepthBuffer(context, depthReplacement[cleanup]);
            }
            DestroyVulkanSwapchain(context, replacement);
            return false;
        }
    }
    if (rayTracingPipeline.IsReady() && replacement.transferDestinationSupported &&
        SupportsVulkanRayTracingComposite(context, replacement.format) &&
        !CreateVulkanRayTracingOutputRing(context, rayTracingPipeline.outputLayout,
                                          replacement.extent, outputReplacement)) {
        DestroyVulkanRayTracingOutputRing(context, outputReplacement);
    }
    CreateReplacementBoxPipeline(replacement, depthReplacement, boxReplacement);
    for (VulkanDepthBuffer& buffer : depth) {
        DestroyVulkanDepthBuffer(context, buffer);
    }
    DestroyVulkanSwapchain(context, swapchain);
    DestroyVulkanBoxPipeline(context, boxPipeline);
    DestroyVulkanRayTracingOutputRing(context, rayTracingOutput);
    swapchain = std::move(replacement);
    boxPipeline = std::move(boxReplacement);
    rayTracingOutput = std::move(outputReplacement);
    for (u32 index = 0; index < kMaxFramesInFlight; ++index) {
        depth[index] = std::move(depthReplacement[index]);
    }
    acquiredImageFence = VK_NULL_HANDLE;
    ++swapchainGeneration;
    swapchainDirty = false;
    return true;
}

void VulkanRenderBackend::Impl::CreateReplacementBoxPipeline(
    const VulkanSwapchain& replacement, const VulkanDepthBuffer* depthReplacement,
    VulkanBoxPipeline& boxReplacement)
{
    if (!frameData.IsReady() || depthReplacement == nullptr) {
        return;
    }
    const VkDescriptorSetLayout shadowLayout =
        shadowPipeline.IsReady() ? shadowMaps[0].descriptorLayout : VK_NULL_HANDLE;
    const VkDescriptorSetLayout rayLayout =
        rayTracing.IsReady() && context.rayTracing.IsRayQueryUsable()
            ? rayTracing.scenes[0].descriptorLayout
            : VK_NULL_HANDLE;
    bool ready = CreateVulkanBoxPipeline(context, replacement.format, depthReplacement[0].format,
                                         frameData.layout, boxReplacement, shadowLayout, rayLayout);
    if (!ready && shadowLayout != VK_NULL_HANDLE) {
        ready = CreateVulkanBoxPipeline(context, replacement.format, depthReplacement[0].format,
                                        frameData.layout, boxReplacement, VK_NULL_HANDLE, rayLayout);
    }
    if (!ready && rayLayout != VK_NULL_HANDLE) {
        CreateVulkanBoxPipeline(context, replacement.format, depthReplacement[0].format,
                                frameData.layout, boxReplacement, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

} // namespace Concord
