// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWPIPELINEINTERNAL_H
#define CONCORD_VULKANSHADOWPIPELINEINTERNAL_H

#include "engine/render/vulkan/VulkanShadowPipeline.h"

#include "engine/core/Mat4.h"
#include "engine/render/SkinningData.h"

#include <cstddef>

namespace Concord {

/** Push-constant values consumed by the directional shadow vertex shader. */
struct VulkanShadowPushConstants {
    Mat4 lightViewProjection{};
    Mat4 model{};
};

static_assert(sizeof(VulkanShadowPushConstants) == 128);
static_assert(offsetof(VulkanShadowPushConstants, model) == 64);

/** Push-constant values consumed by the indexed model shadow shader. */
struct VulkanModelShadowPushConstants {
    Mat4 lightViewProjection{};
    Mat4 model{};
};

static_assert(sizeof(VulkanModelShadowPushConstants) == 128);
static_assert(offsetof(VulkanModelShadowPushConstants, model) == 64);

/** Push values for a skinned shadow draw; model already includes light projection. */
struct alignas(16) VulkanSkinnedShadowPushConstants {
    Mat4 model{};
    SkinningPaletteRange palette{};
};

static_assert(sizeof(VulkanSkinnedShadowPushConstants) == 80);
static_assert(offsetof(VulkanSkinnedShadowPushConstants, palette) == 64);

/** Creates the optional indexed model shadow pipeline. */
VkPipelineLayout CreateVulkanModelShadowPipelineLayout(const VulkanContext& context);

/** Creates a depth-only pipeline for imported model vertices. */
VkPipeline CreateVulkanModelShadowDepthPipeline(const VulkanContext& context,
                                                VkFormat depthFormat,
                                                VkPipelineLayout layout,
                                                VkShaderModule vertex);

/** Records static imported model casters within an active depth rendering scope. */
void RecordVulkanModelShadowCasters(
    VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
    const RenderSceneSnapshot& snapshot, const Mat4& lightViewProjection,
    const VulkanModelAssetCache& modelAssets) noexcept;

/** Makes imported model uploads visible before the shadow rendering scope begins. */
void InsertVulkanModelShadowInputBarrier(
    VkCommandBuffer commandBuffer, const RenderSceneSnapshot& snapshot,
    const VulkanModelAssetCache& modelAssets) noexcept;

/** Creates the optional GPU-skinned model shadow pipeline. */
VkPipelineLayout CreateVulkanSkinnedShadowPipelineLayout(
    const VulkanContext& context, VkDescriptorSetLayout skinningLayout);

/** Creates a depth-only pipeline for GPU-skinned model vertices. */
VkPipeline CreateVulkanSkinnedShadowDepthPipeline(const VulkanContext& context,
                                                  VkFormat depthFormat,
                                                  VkPipelineLayout layout,
                                                  VkShaderModule vertex);

/** Loads and installs the optional GPU-skinned shadow pipeline. */
bool CreateVulkanSkinnedShadowPipeline(const VulkanContext& context, VkFormat depthFormat,
                                       VkDescriptorSetLayout skinningLayout,
                                       VulkanShadowPipeline& pipeline);

/** Records skinned model casters using the current frame's joint palette. */
void RecordVulkanSkinnedShadowCasters(
    VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
    const RenderSceneSnapshot& snapshot, const Mat4& lightViewProjection,
    const VulkanSkinningResources& skinningResources, u32 frameIndex,
    const VulkanModelAssetCache& modelAssets) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSHADOWPIPELINEINTERNAL_H
