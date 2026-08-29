// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendShadow.h"

#include "engine/render/vulkan/VulkanImageBarrier.h"

namespace Concord {

VulkanDirectionalShadowState PrepareVulkanDirectionalShadowFrame(
    RenderFrameData& frame, const RenderSceneSnapshot& snapshot, bool resourcesReady) noexcept
{
    frame.header.reserved &= ~(kRenderFrameFlagDirectionalShadow | kRenderFrameShadowLightMask);
    frame.shadowViewProjection = Mat4::Identity();
    if (!resourcesReady) {
        return {};
    }
    const VulkanDirectionalShadowState state = BuildVulkanDirectionalShadowState(snapshot);
    if (!state.enabled || state.lightIndex >= frame.header.lightCount || state.lightIndex > 255) {
        return {};
    }
    frame.shadowViewProjection = state.viewProjection;
    frame.header.reserved |= kRenderFrameFlagDirectionalShadow |
                             (static_cast<u32>(state.lightIndex) << kRenderFrameShadowLightShift);
    return state;
}

void RecordVulkanDirectionalShadowPass(VkCommandBuffer commandBuffer, VulkanShadowMap& shadowMap,
                                       const VulkanShadowPipeline& pipeline,
                                       const RenderSceneSnapshot& snapshot,
                                       const Mat4& lightViewProjection,
                                       const VulkanModelAssetCache* modelAssets) noexcept
{
    if (!shadowMap.IsReady() || !pipeline.IsReady() || snapshot.objects.empty()) {
        return;
    }
    TransitionVulkanShadowMapToDepth(commandBuffer, shadowMap);
    RecordVulkanShadowPass(commandBuffer, shadowMap.extent, shadowMap.view, pipeline, snapshot,
                            lightViewProjection, modelAssets);
    TransitionVulkanShadowMapToRead(commandBuffer, shadowMap);
}

} // namespace Concord
