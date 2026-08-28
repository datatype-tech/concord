// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBoxPipelineInternal.h"

namespace Concord {
namespace {

/** Creates a descriptor layout with no bindings for an unused set slot. */
VkDescriptorSetLayout CreateEmptySetLayout(const VulkanContext& context)
{
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    return vkCreateDescriptorSetLayout(context.device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}

} // namespace

VkPipelineLayout CreateVulkanBoxPipelineLayout(const VulkanContext& context,
                                               VkDescriptorSetLayout frameDataLayout,
                                               VkDescriptorSetLayout shadowMapLayout,
                                               VkDescriptorSetLayout rayTracingLayout,
                                               VkDescriptorSetLayout* emptySetLayout)
{
    if (context.device == VK_NULL_HANDLE || frameDataLayout == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    if (emptySetLayout != nullptr) {
        *emptySetLayout = VK_NULL_HANDLE;
    }
    VkDescriptorSetLayout placeholder = VK_NULL_HANDLE;
    if (rayTracingLayout != VK_NULL_HANDLE && shadowMapLayout == VK_NULL_HANDLE) {
        if (emptySetLayout == nullptr) {
            return VK_NULL_HANDLE;
        }
        placeholder = CreateEmptySetLayout(context);
        if (placeholder == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        *emptySetLayout = placeholder;
        shadowMapLayout = placeholder;
    }
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.size = sizeof(VulkanBoxPushConstants);
    VkDescriptorSetLayout layouts[3] = {frameDataLayout, shadowMapLayout, rayTracingLayout};
    const u32 layoutCount = 1u + (shadowMapLayout != VK_NULL_HANDLE ? 1u : 0u) +
                            (rayTracingLayout != VK_NULL_HANDLE ? 1u : 0u);
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = layoutCount;
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &range;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(context.device, &info, nullptr, &layout) == VK_SUCCESS) {
        return layout;
    }
    if (placeholder != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, placeholder, nullptr);
        *emptySetLayout = VK_NULL_HANDLE;
    }
    return VK_NULL_HANDLE;
}

} // namespace Concord
