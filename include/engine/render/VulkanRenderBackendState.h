// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_VULKANRENDERBACKENDSTATE_H
#define CONCORD_VULKANRENDERBACKENDSTATE_H
#include "engine/render/VulkanRenderBackend.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanSkinningResources.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanTileLightCulling.h"
#include "engine/render/vulkan/VulkanShadowMap.h"
#include "engine/render/vulkan/VulkanShadowPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"
#include "engine/render/vulkan/VulkanRayTracingPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanSwapchain.h"
#include "engine/window/Window.h"

namespace Concord {

struct VulkanDirectionalShadowState;

/** Groups the native objects owned by the Vulkan backend implementation. */
struct VulkanRenderBackend::Impl {
    Window* window = nullptr;
    VulkanContext context{};
    VulkanSwapchain swapchain{};
    VulkanDepthBuffer depth[kMaxFramesInFlight]{};
    VulkanBoxPipeline boxPipeline{};
    VulkanModelPipeline modelPipeline{};
    VulkanModelAssetCache modelAssets{};
    VulkanSkinningResources skinningResources{};
    VulkanSkinnedPipeline skinnedPipeline{};
    VulkanFrameDataResources frameData{};
    VulkanTileLightCulling tileCulling{};
    VulkanShadowMap shadowMaps[kMaxFramesInFlight]{};
    VulkanShadowPipeline shadowPipeline{};
    VulkanRayTracingSceneRing rayTracing{};
    VulkanRayTracingPipeline rayTracingPipeline{};
    VulkanRayTracingOutputRing rayTracingOutput{};
    VulkanFrameRing frames{};
    u32 imageIndex = 0;
    VkFence acquiredImageFence = VK_NULL_HANDLE;
    bool frameActive = false;
    bool swapchainDirty = false;
    bool frameSyncReady = false;
    bool imageAcquirePending = false, lifecycleInitialized = false;
    u64 swapchainGeneration = 0;
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
    bool RecreateSwapchain();
    void CreateReplacementBoxPipeline(const VulkanSwapchain& replacement,
                                      const VulkanDepthBuffer* depthReplacement,
                                      VulkanBoxPipeline& boxReplacement);
    void CreateReplacementModelPipeline(const VulkanSwapchain& replacement,
                                        const VulkanDepthBuffer* depthReplacement,
                                        VulkanModelPipeline& modelReplacement);
    void CreateReplacementSkinnedPipeline(const VulkanSwapchain& replacement,
                                          const VulkanDepthBuffer* depthReplacement,
                                          VulkanSkinnedPipeline& skinnedReplacement);
    void CreateModelPipelines();
    bool PrepareModelAssets(const RenderSceneSnapshot& snapshot,
                            bool& hasModelObjects, bool& hasBoxObjects,
                            bool& hasStaticModelObjects, bool& hasSkinnedModelObjects);
    bool UploadSkinningFrame(const RenderSceneSnapshot& snapshot, u32 frameIndex) noexcept;
    void RecordRasterPasses(const RenderSceneSnapshot& snapshot,
                            const VulkanDirectionalShadowState& shadowState,
                            VkDescriptorSet frameDataSet, Vec3 skyColor, bool tileEnabled,
                            bool shadowBindingReady, bool rayTracingBuilt,
                            bool rayTracingComposited, bool canDrawBoxes,
                            bool canDrawModels, bool canDrawSkinned);
};

} // namespace Concord
#endif // CONCORD_VULKANRENDERBACKENDSTATE_H
