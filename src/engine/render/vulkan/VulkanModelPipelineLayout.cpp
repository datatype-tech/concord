// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

namespace Concord {

VkPipelineLayout CreateVulkanModelPipelineLayout(const VulkanContext& context,
                                                 VkDescriptorSetLayout frameDataLayout)
{
    if (context.device == VK_NULL_HANDLE || frameDataLayout == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.size = sizeof(VulkanModelPushConstants);
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &frameDataLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    return vkCreatePipelineLayout(context.device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}

} // namespace Concord
