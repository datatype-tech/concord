// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKENDSHADOW_H
#define CONCORD_VULKANRENDERBACKENDSHADOW_H

#include "engine/render/RenderFrameData.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanShadowMap.h"
#include "engine/render/vulkan/VulkanShadowMath.h"
#include "engine/render/vulkan/VulkanShadowPipeline.h"

namespace Concord {

/** Applies the selected directional shadow transform and feature bits to a frame packet. */
[[nodiscard]] VulkanDirectionalShadowState PrepareVulkanDirectionalShadowFrame(
    RenderFrameData& frame, const RenderSceneSnapshot& snapshot, bool resourcesReady) noexcept;

/** Records one optional shadow-map pass and leaves its image sampled-ready. */
void RecordVulkanDirectionalShadowPass(VkCommandBuffer commandBuffer, VulkanShadowMap& shadowMap,
                                       const VulkanShadowPipeline& pipeline,
                                       const RenderSceneSnapshot& snapshot,
                                       const Mat4& lightViewProjection,
                                       const VulkanModelAssetCache* modelAssets = nullptr,
                                       const VulkanSkinningResources* skinningResources = nullptr,
                                       u32 skinningFrameIndex = 0) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRENDERBACKENDSHADOW_H
