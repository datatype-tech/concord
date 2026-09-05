// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendExtensions.h"

#include <type_traits>

namespace Concord {
namespace {

template <typename T>
std::uint64_t DispatchHandle(T handle) noexcept
{
    if constexpr (std::is_pointer_v<T>) {
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
    } else {
        return static_cast<std::uintptr_t>(handle);
    }
}

template <typename T>
std::uint64_t ObjectHandle(T handle) noexcept
{
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<std::uint64_t>(handle);
    } else {
        return static_cast<std::uint64_t>(handle);
    }
}

} // namespace

bool RunVulkanRenderExtensions(const VulkanContext& context,
                               const VulkanSwapchain& swapchain,
                               const VulkanDepthBuffer& depth,
                               const VulkanFrame& frame, u32 frameIndex,
                               u32 imageIndex, VkDescriptorSet frameDescriptorSet,
                               VkDescriptorSet shadowDescriptorSet,
                               const VulkanRayTracingScene* rayTracingScene,
                               u64 swapchainGeneration,
                               VulkanPassPhase phase) noexcept
{
    const bool framePhase = phase == VulkanPassPhase::BeforeScene ||
                            phase == VulkanPassPhase::AfterScene;
    if (framePhase && (imageIndex >= swapchain.images.size() ||
                       imageIndex >= swapchain.views.size() ||
                       imageIndex >= swapchain.imageLayouts.size())) {
        return false;
    }
    VulkanPassContext pass{};
    pass.structSize = static_cast<std::uint32_t>(sizeof(VulkanPassContext));
    pass.version = VulkanPassAbiVersion;
    pass.phase = phase;
    pass.frameIndex = frameIndex;
    // A swapchain image is only acquired during a frame.  Lifecycle callbacks
    // must not accidentally treat the supplied placeholder index as usable.
    pass.imageIndex = framePhase ? imageIndex : VulkanPassInvalidImageIndex;
    pass.width = swapchain.extent.width;
    pass.height = swapchain.extent.height;
    pass.colorFormat = static_cast<std::uint32_t>(swapchain.format);
    pass.depthFormat = static_cast<std::uint32_t>(depth.format);
    pass.graphicsQueueFamily = context.queueFamily;
    pass.swapchainImageCount = static_cast<std::uint32_t>(swapchain.images.size());
    pass.swapchainGeneration = swapchainGeneration;
    const bool recording = framePhase && frame.commandBufferRecording;
    const bool raySceneReady = framePhase && rayTracingScene != nullptr &&
                               rayTracingScene->IsReady();
    pass.commandBufferRecording = recording ? 1u : 0u;
    pass.featureFlags = 0;
    if (framePhase && context.rayTracing.IsRayQueryUsable()) {
        pass.featureFlags |= VulkanPassFeatureRayQuery;
    }
    if (framePhase && context.rayTracing.IsUsable()) {
        pass.featureFlags |= VulkanPassFeatureRayTracingPipeline;
    }
    if (recording) {
        pass.featureFlags |= VulkanPassFeatureCommandRecording;
    }
    if (raySceneReady) {
        pass.featureFlags |= VulkanPassFeatureRayTracingScene;
    }
    pass.instance = DispatchHandle(context.instance);
    pass.physicalDevice = DispatchHandle(context.physicalDevice);
    pass.device = DispatchHandle(context.device);
    pass.graphicsQueue = DispatchHandle(context.graphicsQueue);
    pass.commandBuffer = recording
                             ? DispatchHandle(frame.commandBuffer)
                             : 0;
    if (framePhase && !swapchain.images.empty() && imageIndex < swapchain.images.size()) {
        pass.colorImage = ObjectHandle(swapchain.images[imageIndex]);
    }
    if (framePhase && !swapchain.views.empty() && imageIndex < swapchain.views.size()) {
        pass.colorView = ObjectHandle(swapchain.views[imageIndex]);
    }
    pass.swapchain = ObjectHandle(swapchain.handle);
    if (framePhase) {
        pass.depthImage = ObjectHandle(depth.image);
        pass.depthView = ObjectHandle(depth.view);
        pass.frameDescriptorSet = ObjectHandle(frameDescriptorSet);
        pass.shadowDescriptorSet = ObjectHandle(shadowDescriptorSet);
    }
    pass.rayTracingDescriptorSet = raySceneReady
                                       ? ObjectHandle(rayTracingScene->descriptorSet)
                                       : 0;
    pass.topLevelAccelerationStructure = raySceneReady
                                             ? ObjectHandle(rayTracingScene->topLevel)
                                             : 0;
    pass.colorLayout = framePhase && !swapchain.imageLayouts.empty() &&
                               imageIndex < swapchain.imageLayouts.size()
                           ? static_cast<std::uint32_t>(swapchain.imageLayouts[imageIndex])
                           : 0u;
    pass.depthLayout = framePhase ? static_cast<std::uint32_t>(depth.layout) : 0u;
    return RunVulkanPasses(phase, pass);
}

} // namespace Concord
