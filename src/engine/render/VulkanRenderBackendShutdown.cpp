// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/vulkan/VulkanDevice.h"
#include "engine/render/vulkan/VulkanInstance.h"
#include "engine/render/vulkan/VulkanSurface.h"

namespace Concord {

VulkanRenderBackend::~VulkanRenderBackend() { Shutdown(); }

void VulkanRenderBackend::Shutdown()
{
    if (!m_impl) {
        return;
    }
    Impl& impl = *m_impl;
    if (impl.context.device != VK_NULL_HANDLE) {
        if (impl.frameActive || impl.imageAcquirePending) {
            impl.AbortFrame();
        }
        const VkResult idleResult = vkDeviceWaitIdle(impl.context.device);
        if (idleResult != VK_SUCCESS) {
            VulkanFailed("vkDeviceWaitIdle (shutdown)", idleResult);
        }
        DestroyVulkanTileLightCulling(impl.context, impl.tileCulling);
        DestroyVulkanBoxPipeline(impl.context, impl.boxPipeline);
        DestroyVulkanRayTracingSceneRing(impl.context, impl.rayTracing);
        DestroyVulkanShadowPipeline(impl.context, impl.shadowPipeline);
        for (VulkanShadowMap& map : impl.shadowMaps) {
            DestroyVulkanShadowMap(impl.context, map);
        }
        DestroyVulkanFrameDataResources(impl.context, impl.frameData);
        DestroyVulkanFrameRing(impl.context, impl.frames);
        for (VulkanDepthBuffer& buffer : impl.depth) {
            DestroyVulkanDepthBuffer(impl.context, buffer);
        }
        DestroyVulkanSwapchain(impl.context, impl.swapchain);
        DestroyVulkanDevice(impl.context);
    }
    DestroyVulkanSurface(impl.context);
    DestroyVulkanInstance(impl.context);
    impl.context = {};
    impl.swapchain = {};
    impl.frames = {};
    impl.imageIndex = 0;
    impl.acquiredImageFence = VK_NULL_HANDLE;
    impl.frameActive = false;
    impl.swapchainDirty = false;
    impl.frameSyncReady = false;
    impl.imageAcquirePending = false;
    impl.window = nullptr;
}

} // namespace Concord
