// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANBOXPIPELINE_H
#define CONCORD_VULKANBOXPIPELINE_H

#include "engine/core/Vec3.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Optional graphics pipelines used by the built-in Box draw path. */
struct VulkanBoxPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline depth = VK_NULL_HANDLE;
    VkPipeline color = VK_NULL_HANDLE;
    VkPipeline rayQueryColor = VK_NULL_HANDLE;
    VkDescriptorSetLayout frameDataLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout shadowMapLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rayTracingLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout emptySetLayout = VK_NULL_HANDLE;

    /** Whether a depth-only pre-pass can be recorded. */
    [[nodiscard]] bool HasDepth() const noexcept { return depth != VK_NULL_HANDLE; }

    /** Whether a color pass can be recorded. */
    [[nodiscard]] bool HasColor() const noexcept { return color != VK_NULL_HANDLE; }

    /** Whether the optional ray-query color pass can be recorded. */
    [[nodiscard]] bool HasRayQuery() const noexcept
    {
        return rayQueryColor != VK_NULL_HANDLE && rayTracingLayout != VK_NULL_HANDLE;
    }
};

/** Tries to load bundled SPIR-V and creates any supported Box pipelines. */
bool CreateVulkanBoxPipeline(const VulkanContext& context, VkFormat colorFormat,
                             VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                             VulkanBoxPipeline& pipeline,
                             VkDescriptorSetLayout shadowMapLayout = VK_NULL_HANDLE,
                             VkDescriptorSetLayout rayTracingLayout = VK_NULL_HANDLE);

/** Releases all optional Box pipeline objects. */
void DestroyVulkanBoxPipeline(const VulkanContext& context, VulkanBoxPipeline& pipeline);

/** Records a depth-only pass for the Box objects in a frame snapshot. */
void RecordVulkanBoxDepthPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                              VkImageView depthView, const VulkanBoxPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet);

/** Records a color/depth pass for the Box objects in a frame snapshot. */
void RecordVulkanBoxColorPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                              VkImageView colorView, VkImageView depthView,
                              const VulkanBoxPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet, Vec3 clearColor = {},
                              VkDescriptorSet shadowMapSet = VK_NULL_HANDLE,
                              VkDescriptorSet rayTracingSet = VK_NULL_HANDLE);

/** Makes depth writes from the pre-pass visible to the forward pass. */
void InsertVulkanBoxDepthBarrier(VkCommandBuffer commandBuffer, VkImage depthImage);

} // namespace Concord

#endif // CONCORD_VULKANBOXPIPELINE_H
