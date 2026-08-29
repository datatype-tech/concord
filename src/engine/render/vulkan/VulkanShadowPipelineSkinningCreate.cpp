// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"

#include "engine/render/vulkan/VulkanShaderModule.h"

#include <vector>

namespace Concord {

bool CreateVulkanSkinnedShadowPipeline(const VulkanContext& context, VkFormat depthFormat,
                                       VkDescriptorSetLayout skinningLayout,
                                       VulkanShadowPipeline& pipeline)
{
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.skinnedDepth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.skinnedDepth, nullptr);
        }
        if (pipeline.skinnedLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.skinnedLayout, nullptr);
        }
    }
    pipeline.skinnedDepth = VK_NULL_HANDLE;
    pipeline.skinnedLayout = VK_NULL_HANDLE;
    if (context.device == VK_NULL_HANDLE || depthFormat == VK_FORMAT_UNDEFINED ||
        skinningLayout == VK_NULL_HANDLE) {
        return false;
    }
    const std::vector<u32> code = ReadVulkanShaderCode("directional_shadow_skinned.vert.spv");
    const VkShaderModule shader = CreateVulkanShaderModule(context, code);
    if (shader == VK_NULL_HANDLE) return false;
    pipeline.skinnedLayout = CreateVulkanSkinnedShadowPipelineLayout(context, skinningLayout);
    if (pipeline.skinnedLayout != VK_NULL_HANDLE) {
        pipeline.skinnedDepth = CreateVulkanSkinnedShadowDepthPipeline(
            context, depthFormat, pipeline.skinnedLayout, shader);
    }
    vkDestroyShaderModule(context.device, shader, nullptr);
    if (pipeline.HasSkinned()) return true;
    if (pipeline.skinnedDepth != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, pipeline.skinnedDepth, nullptr);
    }
    if (pipeline.skinnedLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, pipeline.skinnedLayout, nullptr);
    }
    pipeline.skinnedDepth = VK_NULL_HANDLE;
    pipeline.skinnedLayout = VK_NULL_HANDLE;
    return false;
}

} // namespace Concord
