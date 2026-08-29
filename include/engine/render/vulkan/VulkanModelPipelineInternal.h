// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANMODELPIPELINEINTERNAL_H
#define CONCORD_VULKANMODELPIPELINEINTERNAL_H

#include "engine/render/vulkan/VulkanModelPipeline.h"

#include "engine/core/Mat4.h"
#include "engine/core/Vec4.h"

#include <cstddef>

namespace Concord {

/** Per-object ABI shared by the static model vertex and fragment shaders. */
struct VulkanModelPushConstants {
    Mat4 model{};
    Vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 material{0.0f, 0.8f, 0.0f, 0.0f};
};

static_assert(sizeof(VulkanModelPushConstants) == 96);
static_assert(offsetof(VulkanModelPushConstants, albedo) == 64);
static_assert(offsetof(VulkanModelPushConstants, material) == 80);

/** Creates the descriptor and push-constant layout for model pipelines. */
VkPipelineLayout CreateVulkanModelPipelineLayout(const VulkanContext& context,
                                                VkDescriptorSetLayout frameDataLayout);

/** Creates one dynamic-rendering model graphics pipeline. */
VkPipeline CreateVulkanModelGraphicsPipeline(const VulkanContext& context,
                                             VkFormat colorFormat, VkFormat depthFormat,
                                             VkPipelineLayout layout, VkShaderModule vertex,
                                             VkShaderModule fragment, bool withColor);

/** Records all valid indexed model ranges in a dynamic-rendering scope. */
void RecordVulkanModelDraws(VkCommandBuffer commandBuffer,
                            const VulkanModelPipeline& pipeline,
                            const RenderSceneSnapshot& snapshot,
                            VkDescriptorSet frameDataSet,
                            const VulkanModelAssetCache& cache);

/** Makes host uploads visible to indexed vertex input and material reads. */
void InsertVulkanModelInputBarrier(VkCommandBuffer commandBuffer,
                                    const RenderSceneSnapshot& snapshot,
                                    const VulkanModelAssetCache& cache);

/** Sets the viewport and scissor shared by model passes. */
void SetVulkanModelViewport(VkCommandBuffer commandBuffer, VkExtent2D extent);

} // namespace Concord

#endif // CONCORD_VULKANMODELPIPELINEINTERNAL_H
