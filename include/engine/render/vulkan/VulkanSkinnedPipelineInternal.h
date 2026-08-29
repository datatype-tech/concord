// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSKINNEDPIPELINEINTERNAL_H
#define CONCORD_VULKANSKINNEDPIPELINEINTERNAL_H

#include "engine/render/vulkan/VulkanSkinnedPipeline.h"

#include "engine/render/SkinningData.h"

namespace Concord {

/** Creates the frame, palette and texture set layout with push constants. */
VkPipelineLayout CreateVulkanSkinnedPipelineLayout(const VulkanContext& context,
                                                   VkDescriptorSetLayout frameDataLayout,
                                                   VkDescriptorSetLayout skinningLayout,
                                                   VkDescriptorSetLayout textureLayout);

/** Creates one dynamic-rendering skinned graphics pipeline. */
VkPipeline CreateVulkanSkinnedGraphicsPipeline(const VulkanContext& context,
                                               VkFormat colorFormat, VkFormat depthFormat,
                                               VkPipelineLayout layout, VkShaderModule vertex,
                                               VkShaderModule fragment, bool withColor);

/** Records indexed skinned ranges in the active graphics scope. */
void RecordVulkanSkinnedDraws(VkCommandBuffer commandBuffer,
                              const VulkanSkinnedPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet,
                              const VulkanSkinningResources& resources, u32 frameIndex,
                              const VulkanModelAssetCache& cache,
                              const VulkanTextureCache& textureCache);

/** Makes imported vertex and palette uploads visible to shader stages. */
void InsertVulkanSkinnedInputBarrier(VkCommandBuffer commandBuffer,
                                      const RenderSceneSnapshot& snapshot,
                                      const VulkanSkinningResources& resources,
                                      u32 frameIndex, const VulkanModelAssetCache& cache);

/** Sets viewport and scissor for one skinned pass. */
void SetVulkanSkinnedViewport(VkCommandBuffer commandBuffer, VkExtent2D extent);

} // namespace Concord

#endif // CONCORD_VULKANSKINNEDPIPELINEINTERNAL_H
