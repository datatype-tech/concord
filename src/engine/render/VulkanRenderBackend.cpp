// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"
#include "engine/render/VulkanRenderBackendExtensions.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanDebugOverlay.h"
#include "engine/render/vulkan/VulkanDevice.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanInstance.h"
#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanShaderModule.h"
#include "engine/render/vulkan/VulkanSurface.h"
#include "engine/render/vulkan/VulkanSwapchain.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"

#include <cstdio>

namespace Concord {
VulkanRenderBackend::VulkanRenderBackend() : m_impl(std::make_unique<Impl>()) {}
bool VulkanRenderBackend::Init(Window& window, const RenderBackendInit& init)
{
    Shutdown();
    Impl& impl = *m_impl;
    impl.window = &window;
    const bool initialized =
        CreateVulkanInstance(impl.context, init.enableValidation) &&
        CreateVulkanSurface(impl.context, window) && CreateVulkanDevice(impl.context) &&
        CreateVulkanSwapchain(impl.context, impl.swapchain, window.Width(), window.Height(),
                              window.Vsync());
    if (!initialized) {
        Shutdown();
        return false;
    }
    for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
        if (!CreateVulkanDepthBuffer(impl.context, impl.depth[i], impl.swapchain.extent)) {
            Shutdown();
            return false;
        }
    }
    if (!CreateVulkanFrameRing(impl.context, impl.frames)) {
        Shutdown();
        return false;
    }
    impl.frameSyncReady = true;
    impl.swapchainGeneration = 1;
    const bool rayQueryShaderAvailable =
        !ReadVulkanShaderCode("solid_rayquery.frag.spv").empty();
    const bool rayPipelineShaderAvailable =
        !ReadVulkanShaderCode("raygen.rgen.spv").empty() &&
        !ReadVulkanShaderCode("raymiss.rmiss.spv").empty() &&
        !ReadVulkanShaderCode("rayhit.rchit.spv").empty();
    const bool wantsRayScene =
        init.enableRayTracing &&
        ((rayQueryShaderAvailable && impl.context.rayTracing.IsRayQueryUsable()) ||
         (rayPipelineShaderAvailable && impl.context.rayTracing.IsUsable()));
    if (wantsRayScene &&
        !CreateVulkanRayTracingSceneRing(impl.context, impl.rayTracing)) {
        std::fprintf(stderr, "[Concord] ray-tracing acceleration structures unavailable; "
                             "using raster path\n");
    }
    if (!CreateVulkanFrameDataResources(impl.context, impl.frameData)) {
        std::fprintf(stderr, "[Concord] frame data buffer unavailable; using clear-only fallback\n");
    } else {
        if (!impl.textureCache.Initialize(impl.context)) {
            std::fprintf(stderr,
                         "[Concord] texture resources unavailable; imported models disabled\n");
        }
        if (rayPipelineShaderAvailable && impl.rayTracing.IsReady() &&
            CreateVulkanRayTracingPipeline(impl.context, impl.frameData.layout,
                                           impl.rayTracing.scenes[0].descriptorLayout,
                                           impl.rayTracingPipeline)) {
            if (!impl.swapchain.transferDestinationSupported ||
                !SupportsVulkanRayTracingComposite(impl.context, impl.swapchain.format) ||
                !CreateVulkanRayTracingOutputRing(impl.context,
                                                  impl.rayTracingPipeline.outputLayout,
                                                  impl.swapchain.extent, impl.rayTracingOutput)) {
                DestroyVulkanRayTracingPipeline(impl.context, impl.rayTracingPipeline);
            }
        }
        bool shadowsReady = true;
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            if (!CreateVulkanShadowMap(impl.context,
                                       {kDirectionalShadowMapSize, kDirectionalShadowMapSize},
                                       impl.shadowMaps[i])) {
                shadowsReady = false;
                break;
            }
        }
        if (shadowsReady) {
            shadowsReady = CreateVulkanShadowPipeline(impl.context, impl.shadowMaps[0].format,
                                                      impl.shadowPipeline);
        }
        if (!shadowsReady) {
            for (VulkanShadowMap& map : impl.shadowMaps) {
                DestroyVulkanShadowMap(impl.context, map);
            }
            DestroyVulkanShadowPipeline(impl.context, impl.shadowPipeline);
            std::fprintf(stderr, "[Concord] shadow resources unavailable; using unshadowed path\n");
        }
        const VkDescriptorSetLayout shadowLayout =
            shadowsReady ? impl.shadowMaps[0].descriptorLayout : VK_NULL_HANDLE;
        const VkDescriptorSetLayout rayTracingLayout =
            impl.rayTracing.IsReady() && impl.context.rayTracing.IsRayQueryUsable()
                ? impl.rayTracing.scenes[0].descriptorLayout
                : VK_NULL_HANDLE;
        bool boxReady = CreateVulkanBoxPipeline(impl.context, impl.swapchain.format,
                                                impl.depth[0].format, impl.frameData.layout,
                                                impl.boxPipeline, shadowLayout, rayTracingLayout);
        if (!boxReady && shadowLayout != VK_NULL_HANDLE) {
            boxReady = CreateVulkanBoxPipeline(impl.context, impl.swapchain.format,
                                               impl.depth[0].format, impl.frameData.layout,
                                               impl.boxPipeline, VK_NULL_HANDLE,
                                               rayTracingLayout);
        }
        if (!boxReady && rayTracingLayout != VK_NULL_HANDLE) {
            boxReady = CreateVulkanBoxPipeline(impl.context, impl.swapchain.format,
                                               impl.depth[0].format, impl.frameData.layout,
                                               impl.boxPipeline, VK_NULL_HANDLE, VK_NULL_HANDLE);
        }
        if (!boxReady) {
            std::fprintf(stderr,
                         "[Concord] Box shader artifacts unavailable; using clear-only fallback\n");
        } else if (!CreateVulkanTileLightCulling(impl.context, impl.frameData.layout,
                                                  impl.tileCulling)) {
            std::fprintf(stderr, "[Concord] tile shader unavailable; using all-light fallback\n");
        }
        impl.CreateModelPipelines();
    }
    const bool extensionsReady = RunVulkanRenderExtensions(
        impl.context, impl.swapchain, impl.depth[0], impl.frames.Current(),
        impl.frames.currentFrame, 0,
        impl.frameData.IsReady() ? impl.frameData.sets[impl.frames.currentFrame]
                                 : VK_NULL_HANDLE,
        impl.shadowPipeline.IsReady() ? impl.shadowMaps[0].descriptorSet : VK_NULL_HANDLE,
        impl.rayTracing.IsReady() ? &impl.rayTracing.At(0) : nullptr,
        impl.swapchainGeneration, VulkanPassPhase::Initialize);
    if (!extensionsReady) {
        std::fprintf(stderr, "[Concord] one or more Vulkan initialize passes failed\n");
    }
    if (!CreateVulkanDebugOverlay(impl.context, impl.swapchain.format, impl.debugOverlay)) {
        std::fprintf(stderr, "[Concord] debug overlay unavailable; on-screen stats disabled\n");
    }
    impl.lifecycleInitialized = true;
    return true;
}

void VulkanRenderBackend::SetDebugOverlay(const DebugOverlayFrame* overlay)
{
    m_impl->debugOverlayFrame = overlay;
}

RenderBackendStats VulkanRenderBackend::LastFrameStats() const
{
    const Impl& impl = *m_impl;
    RenderBackendStats stats{};
    stats.width = impl.swapchain.extent.width;
    stats.height = impl.swapchain.extent.height;
    stats.visibleObjects = static_cast<u32>(impl.visibleObjectCount);
    stats.lights = static_cast<u32>(impl.lightCount);
    stats.rayTracingActive = impl.rayTracingCompositedLastFrame;
    return stats;
}
} // namespace Concord
