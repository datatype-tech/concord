// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANMODELPIPELINE_H
#define CONCORD_VULKANMODELPIPELINE_H

#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanModelAsset.h"
#include "engine/render/vulkan/VulkanModelAssetCache.h"
#include "engine/render/vulkan/VulkanTextureCache.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Graphics pipelines for indexed imported model primitives. */
struct VulkanModelPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline depth = VK_NULL_HANDLE;
    VkPipeline color = VK_NULL_HANDLE;
    VkDescriptorSetLayout frameDataLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureLayout = VK_NULL_HANDLE;

    /** Whether the model depth pre-pass is available. */
    [[nodiscard]] bool HasDepth() const noexcept { return depth != VK_NULL_HANDLE; }

    /** Whether the model forward pass is available. */
    [[nodiscard]] bool HasColor() const noexcept { return color != VK_NULL_HANDLE; }

    /** Whether both passes required for a complete draw are available. */
    [[nodiscard]] bool IsReady() const noexcept { return HasDepth() && HasColor(); }
};

/** Creates static model pipelines for the supplied swapchain formats. */
bool CreateVulkanModelPipeline(const VulkanContext& context, VkFormat colorFormat,
                               VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                               VkDescriptorSetLayout textureLayout,
                               VulkanModelPipeline& pipeline);

/** Releases model pipeline objects. */
void DestroyVulkanModelPipeline(const VulkanContext& context,
                                VulkanModelPipeline& pipeline) noexcept;

/** Records indexed model depth draws, optionally clearing the depth target. */
void RecordVulkanModelDepthPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                VkImageView depthView, const VulkanModelPipeline& pipeline,
                                const RenderSceneSnapshot& snapshot,
                                VkDescriptorSet frameDataSet, const VulkanModelAssetCache& cache,
                                const VulkanTextureCache& textureCache,
                                bool clearDepth = false);

/** Records indexed model color draws, optionally clearing the color target. */
void RecordVulkanModelColorPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                VkImageView colorView, VkImageView depthView,
                                const VulkanModelPipeline& pipeline,
                                const RenderSceneSnapshot& snapshot,
                                VkDescriptorSet frameDataSet, const VulkanModelAssetCache& cache,
                                const VulkanTextureCache& textureCache,
                                Vec3 clearColor = {}, bool clearColorTarget = false);

} // namespace Concord

#endif // CONCORD_VULKANMODELPIPELINE_H
