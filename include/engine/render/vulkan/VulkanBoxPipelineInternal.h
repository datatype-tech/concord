// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANBOXPIPELINEINTERNAL_H
#define CONCORD_VULKANBOXPIPELINEINTERNAL_H

#include "engine/render/vulkan/VulkanBoxPipeline.h"

#include "engine/core/Mat4.h"
#include "engine/core/Vec4.h"

#include <cstddef>

namespace Concord {

/** Per-object values shared by the Box vertex and fragment shaders. */
struct VulkanBoxPushConstants {
    Mat4 model{};
    Vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 material{0.0f, 0.8f, 0.0f, 0.0f};
};

static_assert(sizeof(VulkanBoxPushConstants) == 96);
static_assert(offsetof(VulkanBoxPushConstants, albedo) == 64);
static_assert(offsetof(VulkanBoxPushConstants, material) == 80);

/** Creates the descriptor-backed layout shared by the optional Box pipelines. */
VkPipelineLayout CreateVulkanBoxPipelineLayout(const VulkanContext& context,
                                               VkDescriptorSetLayout frameDataLayout,
                                               VkDescriptorSetLayout shadowMapLayout = VK_NULL_HANDLE,
                                               VkDescriptorSetLayout rayTracingLayout = VK_NULL_HANDLE,
                                               VkDescriptorSetLayout* emptySetLayout = nullptr);

/** Creates a dynamic-rendering pipeline for one shader-stage configuration. */
VkPipeline CreateVulkanBoxGraphicsPipeline(const VulkanContext& context, VkFormat colorFormat,
                                           VkFormat depthFormat, VkPipelineLayout layout,
                                           VkShaderModule vertex, VkShaderModule fragment,
                                           bool withColor);

} // namespace Concord

#endif // CONCORD_VULKANBOXPIPELINEINTERNAL_H
