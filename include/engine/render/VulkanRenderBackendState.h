// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKENDSTATE_H
#define CONCORD_VULKANRENDERBACKENDSTATE_H

#include "engine/render/VulkanRenderBackend.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"
#include "engine/render/vulkan/VulkanShadowMap.h"
#include "engine/render/vulkan/VulkanShadowPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanSwapchain.h"
#include "engine/window/Window.h"
#include <utility>
namespace Concord {
/** Groups the native objects owned by the Vulkan backend implementation. */
struct VulkanRenderBackend::Impl {
    Window* window = nullptr;
    VulkanContext context{};
    VulkanSwapchain swapchain{};
    VulkanDepthBuffer depth[kMaxFramesInFlight]{};
    VulkanBoxPipeline boxPipeline{};
    VulkanFrameDataResources frameData{};
    VulkanTileLightCulling tileCulling{};
    VulkanShadowMap shadowMaps[kMaxFramesInFlight]{};
    VulkanShadowPipeline shadowPipeline{};
    VulkanRayTracingSceneRing rayTracing{};
    VulkanFrameRing frames{};
    u32 imageIndex = 0;
    VkFence acquiredImageFence = VK_NULL_HANDLE;
    bool frameActive = false;
    bool swapchainDirty = false;
    bool frameSyncReady = false;
    bool imageAcquirePending = false;
    usize visibleObjectCount = 0;
    usize lightCount = 0;
    bool hasCamera = false;
    bool RecoverAcquiredFrame(VulkanFrame& frame) noexcept
    {
        frameActive = false;
        swapchainDirty = true;
        InvalidateVulkanShadowMapLayouts(shadowMaps, kMaxFramesInFlight);
        const VkFence staleFence = frame.inFlight;
        const bool syncReady = RecoverVulkanFrame(context, frame);
        if (!syncReady) {
            frameSyncReady = false;
            return false;
        }
        if (staleFence != VK_NULL_HANDLE) {
            for (VkFence& imageFence : swapchain.imagesInFlight) {
                if (imageFence == staleFence) {
                    imageFence = VK_NULL_HANDLE;
                }
            }
        }
        imageAcquirePending = false;
        acquiredImageFence = VK_NULL_HANDLE;
        frameSyncReady = true;
        return true;
    }
    void HandleFrameFailure(VulkanFrame& frame)
    {
        if (RecoverAcquiredFrame(frame)) {
            RecreateSwapchain();
        }
    }
    void AbortFrame() noexcept
    {
        if (frameActive || imageAcquirePending) {
            RecoverAcquiredFrame(frames.Current());
        } else {
            acquiredImageFence = VK_NULL_HANDLE;
        }
    }
    bool RecreateSwapchain()
    {
        if (!window || context.device == VK_NULL_HANDLE || window->Width() == 0 ||
            window->Height() == 0) {
            return false;
        }
        const VkResult idleResult = vkDeviceWaitIdle(context.device);
        if (idleResult != VK_SUCCESS) {
            VulkanFailed("vkDeviceWaitIdle (recreate swapchain)", idleResult);
            frameSyncReady = false;
            return false;
        }
        VulkanSwapchain replacement{};
        VulkanDepthBuffer depthReplacement[kMaxFramesInFlight]{};
        VulkanBoxPipeline boxReplacement{};
        if (!CreateVulkanSwapchain(context, replacement, window->Width(), window->Height(),
                                   window->Vsync(), swapchain.handle)) {
            DestroyVulkanSwapchain(context, replacement);
            return false;
        }
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            if (!CreateVulkanDepthBuffer(context, depthReplacement[i], replacement.extent)) {
                for (u32 j = 0; j <= i; ++j) {
                    DestroyVulkanDepthBuffer(context, depthReplacement[j]);
                }
                DestroyVulkanSwapchain(context, replacement);
                return false;
            }
        }
        if (frameData.IsReady()) {
            const VkDescriptorSetLayout shadowLayout =
                shadowPipeline.IsReady() ? shadowMaps[0].descriptorLayout : VK_NULL_HANDLE;
            const VkDescriptorSetLayout rayTracingLayout =
                rayTracing.IsReady() && context.rayTracing.IsRayQueryUsable()
                    ? rayTracing.scenes[0].descriptorLayout
                    : VK_NULL_HANDLE;
            bool boxReady = CreateVulkanBoxPipeline(context, replacement.format,
                                                    depthReplacement[0].format, frameData.layout,
                                                    boxReplacement, shadowLayout,
                                                    rayTracingLayout);
            if (!boxReady && shadowLayout != VK_NULL_HANDLE) {
                boxReady = CreateVulkanBoxPipeline(context, replacement.format,
                                                   depthReplacement[0].format, frameData.layout,
                                                   boxReplacement, VK_NULL_HANDLE,
                                                   rayTracingLayout);
            }
            if (!boxReady && rayTracingLayout != VK_NULL_HANDLE) {
                boxReady = CreateVulkanBoxPipeline(context, replacement.format,
                                                   depthReplacement[0].format, frameData.layout,
                                                   boxReplacement, VK_NULL_HANDLE,
                                                   VK_NULL_HANDLE);
            }
        }
        for (VulkanDepthBuffer& buffer : depth) {
            DestroyVulkanDepthBuffer(context, buffer);
        }
        DestroyVulkanSwapchain(context, swapchain);
        DestroyVulkanBoxPipeline(context, boxPipeline);
        swapchain = std::move(replacement);
        boxPipeline = std::move(boxReplacement);
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            depth[i] = std::move(depthReplacement[i]);
        }
        acquiredImageFence = VK_NULL_HANDLE;
        swapchainDirty = false;
        return true;
    }
};
} // namespace Concord
#endif // CONCORD_VULKANRENDERBACKENDSTATE_H
