// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/VulkanRenderBackendExtensions.h"
#include "engine/render/vulkan/VulkanDevice.h"
#include "engine/render/vulkan/VulkanInstance.h"
#include "engine/render/vulkan/VulkanDebugOverlay.h"
#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanSkinningResources.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"
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
        if (impl.lifecycleInitialized) {
            const bool extensionsReady = RunVulkanRenderExtensions(
                impl.context, impl.swapchain, impl.depth[0], impl.frames.Current(),
                impl.frames.currentFrame, 0,
                impl.frameData.IsReady() ? impl.frameData.sets[impl.frames.currentFrame]
                                         : VK_NULL_HANDLE,
                impl.shadowPipeline.IsReady() ? impl.shadowMaps[0].descriptorSet : VK_NULL_HANDLE,
                impl.rayTracing.IsReady() ? &impl.rayTracing.At(0) : nullptr,
                impl.swapchainGeneration, VulkanPassPhase::Shutdown);
            if (!extensionsReady) {
                std::fprintf(stderr, "[Concord] one or more Vulkan shutdown passes failed\n");
            }
        }
        DestroyVulkanTileLightCulling(impl.context, impl.tileCulling);
        DestroyVulkanBoxPipeline(impl.context, impl.boxPipeline);
        DestroyVulkanDebugOverlay(impl.context, impl.debugOverlay);
        DestroyVulkanModelPipeline(impl.context, impl.modelPipeline);
        DestroyVulkanSkinnedPipeline(impl.context, impl.skinnedPipeline);
        DestroyVulkanShadowPipeline(impl.context, impl.shadowPipeline);
        DestroyVulkanSkinningResources(impl.context, impl.skinningResources);
        impl.textureCache.Clear(impl.context);
        DestroyVulkanRayTracingOutputRing(impl.context, impl.rayTracingOutput);
        DestroyVulkanRayTracingPipeline(impl.context, impl.rayTracingPipeline);
        DestroyVulkanRayTracingSceneRing(impl.context, impl.rayTracing);
        impl.modelAssets.Clear(impl.context);
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
    impl.lifecycleInitialized = false;
    impl.swapchainGeneration = 0;
    impl.window = nullptr;
}

} // namespace Concord
