// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackend.h"

#include "engine/render/VulkanRenderBackendState.h"

#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanSwapchain.h"

#include <cstdio>
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
    VulkanModelPipeline modelReplacement{};
    VulkanSkinnedPipeline skinnedReplacement{};
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
    const bool outputWasReady = rayTracingOutput.IsReady();
    const bool outputSupported =
        rayTracingPipeline.IsReady() && replacement.transferDestinationSupported &&
        SupportsVulkanRayTracingComposite(context, replacement.format);
    const bool outputReady =
        !outputSupported ||
        CreateVulkanRayTracingOutputRing(context, rayTracingPipeline.outputLayout,
                                         replacement.extent, outputReplacement);
    if (outputWasReady && !outputSupported) {
        std::fprintf(stderr,
                     "[Concord] swapchain recreation failed: ray tracing output unsupported\n");
        for (VulkanDepthBuffer& buffer : depthReplacement) {
            DestroyVulkanDepthBuffer(context, buffer);
        }
        DestroyVulkanSwapchain(context, replacement);
        return false;
    }
    if (outputWasReady && !outputReady) {
        std::fprintf(stderr,
                     "[Concord] swapchain recreation failed: ray tracing output unavailable\n");
        DestroyVulkanRayTracingOutputRing(context, outputReplacement);
        for (VulkanDepthBuffer& buffer : depthReplacement) {
            DestroyVulkanDepthBuffer(context, buffer);
        }
        DestroyVulkanSwapchain(context, replacement);
        return false;
    }
    const bool boxWasReady = boxPipeline.HasDepth() && boxPipeline.HasColor();
    const bool modelWasReady = modelPipeline.IsReady();
    const bool skinnedWasReady = skinnedPipeline.IsReady();
    const bool boxReady =
        CreateReplacementBoxPipeline(replacement, depthReplacement, boxReplacement);
    const bool modelReady =
        CreateReplacementModelPipeline(replacement, depthReplacement, modelReplacement);
    const bool skinnedReady =
        CreateReplacementSkinnedPipeline(replacement, depthReplacement, skinnedReplacement);
    if (boxWasReady && !boxReady) {
        std::fprintf(stderr,
                     "[Concord] swapchain recreation failed: box pipeline unavailable\n");
        DestroyVulkanBoxPipeline(context, boxReplacement);
        DestroyVulkanModelPipeline(context, modelReplacement);
        DestroyVulkanSkinnedPipeline(context, skinnedReplacement);
        DestroyVulkanRayTracingOutputRing(context, outputReplacement);
        for (VulkanDepthBuffer& buffer : depthReplacement) {
            DestroyVulkanDepthBuffer(context, buffer);
        }
        DestroyVulkanSwapchain(context, replacement);
        return false;
    }
    if (!modelReady && modelWasReady) {
        std::fprintf(stderr,
                     "[Concord] model shader artifacts unavailable after resize; imported models disabled\n");
    }
    if (!skinnedReady && skinnedWasReady) {
        std::fprintf(stderr,
                     "[Concord] skinned shader artifacts unavailable after resize; skinned models disabled\n");
    }
    for (VulkanDepthBuffer& buffer : depth) {
        DestroyVulkanDepthBuffer(context, buffer);
    }
    DestroyVulkanSwapchain(context, swapchain);
    DestroyVulkanBoxPipeline(context, boxPipeline);
    DestroyVulkanModelPipeline(context, modelPipeline);
    DestroyVulkanSkinnedPipeline(context, skinnedPipeline);
    DestroyVulkanRayTracingOutputRing(context, rayTracingOutput);
    swapchain = std::move(replacement);
    boxPipeline = std::move(boxReplacement);
    modelPipeline = std::move(modelReplacement);
    skinnedPipeline = std::move(skinnedReplacement);
    rayTracingOutput = std::move(outputReplacement);
    for (u32 index = 0; index < kMaxFramesInFlight; ++index) {
        depth[index] = std::move(depthReplacement[index]);
    }
    acquiredImageFence = VK_NULL_HANDLE;
    ++swapchainGeneration;
    swapchainDirty = false;
    return true;
}

bool VulkanRenderBackend::Impl::CreateReplacementSkinnedPipeline(
    const VulkanSwapchain& replacement, const VulkanDepthBuffer* depthReplacement,
    VulkanSkinnedPipeline& skinnedReplacement)
{
    if (!frameData.IsReady() || skinningResources.layout == VK_NULL_HANDLE ||
        depthReplacement == nullptr) {
        return true;
    }
    return CreateVulkanSkinnedPipeline(context, replacement.format, depthReplacement[0].format,
                                       frameData.layout, skinningResources.layout,
                                       textureCache.DescriptorLayout(), skinnedReplacement);
}

bool VulkanRenderBackend::Impl::CreateReplacementModelPipeline(
    const VulkanSwapchain& replacement, const VulkanDepthBuffer* depthReplacement,
    VulkanModelPipeline& modelReplacement)
{
    if (!frameData.IsReady() || depthReplacement == nullptr) {
        return true;
    }
    return CreateVulkanModelPipeline(context, replacement.format, depthReplacement[0].format,
                                     frameData.layout, textureCache.DescriptorLayout(),
                                     modelReplacement);
}

bool VulkanRenderBackend::Impl::CreateReplacementBoxPipeline(
    const VulkanSwapchain& replacement, const VulkanDepthBuffer* depthReplacement,
    VulkanBoxPipeline& boxReplacement)
{
    if (!frameData.IsReady() || depthReplacement == nullptr) {
        return false;
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
        ready = CreateVulkanBoxPipeline(context, replacement.format, depthReplacement[0].format,
                                        frameData.layout, boxReplacement, VK_NULL_HANDLE,
                                        VK_NULL_HANDLE);
    }
    return ready;
}

} // namespace Concord
