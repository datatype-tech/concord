// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSKINNEDPIPELINE_H
#define CONCORD_VULKANSKINNEDPIPELINE_H

#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanModelAssetCache.h"
#include "engine/render/vulkan/VulkanSkinningResources.h"

#include <vulkan/vulkan.h>

namespace Concord {

struct VulkanTextureCache;

/** Graphics pipelines for imported primitives driven by joint matrices. */
struct VulkanSkinnedPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline depth = VK_NULL_HANDLE;
    VkPipeline color = VK_NULL_HANDLE;
    VkDescriptorSetLayout frameDataLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout skinningLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureLayout = VK_NULL_HANDLE;

    /** Whether the skinned depth pre-pass is available. */
    [[nodiscard]] bool HasDepth() const noexcept { return depth != VK_NULL_HANDLE; }
    /** Whether the skinned forward pass is available. */
    [[nodiscard]] bool HasColor() const noexcept { return color != VK_NULL_HANDLE; }
    /** Whether both skinned passes are available. */
    [[nodiscard]] bool IsReady() const noexcept { return HasDepth() && HasColor(); }
};

/** Creates skinned pipelines with set zero frame data and set one palette. */
bool CreateVulkanSkinnedPipeline(const VulkanContext& context, VkFormat colorFormat,
                                 VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                                 VkDescriptorSetLayout skinningLayout,
                                 VkDescriptorSetLayout textureLayout,
                                 VulkanSkinnedPipeline& pipeline);

/** Releases skinned pipeline objects. */
void DestroyVulkanSkinnedPipeline(const VulkanContext& context,
                                  VulkanSkinnedPipeline& pipeline) noexcept;

/** Records a skinned depth pass over valid snapshot ranges. */
void RecordVulkanSkinnedDepthPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                  VkImageView depthView, const VulkanSkinnedPipeline& pipeline,
                                  const RenderSceneSnapshot& snapshot,
                                  VkDescriptorSet frameDataSet,
                                  const VulkanSkinningResources& skinningResources,
                                  u32 frameIndex, const VulkanModelAssetCache& cache,
                                  const VulkanTextureCache& textureCache,
                                  bool clearDepth = false);

/** Records a skinned color pass over valid snapshot ranges. */
void RecordVulkanSkinnedColorPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                  VkImageView colorView, VkImageView depthView,
                                  const VulkanSkinnedPipeline& pipeline,
                                  const RenderSceneSnapshot& snapshot,
                                  VkDescriptorSet frameDataSet,
                                  const VulkanSkinningResources& skinningResources,
                                  u32 frameIndex, const VulkanModelAssetCache& cache,
                                  const VulkanTextureCache& textureCache,
                                  Vec3 clearColor = {}, bool clearColorTarget = false);

} // namespace Concord

#endif // CONCORD_VULKANSKINNEDPIPELINE_H
