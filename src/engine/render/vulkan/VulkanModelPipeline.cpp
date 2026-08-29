// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelPipeline.h"

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

namespace Concord {

bool CreateVulkanModelPipeline(const VulkanContext& context, VkFormat colorFormat,
                               VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                               VulkanModelPipeline& pipeline)
{
    DestroyVulkanModelPipeline(context, pipeline);
    if (context.device == VK_NULL_HANDLE || colorFormat == VK_FORMAT_UNDEFINED ||
        depthFormat == VK_FORMAT_UNDEFINED || frameDataLayout == VK_NULL_HANDLE) {
        return false;
    }
    const std::vector<u32> vertexCode = ReadVulkanShaderCode("model.vert.spv");
    const std::vector<u32> fragmentCode = ReadVulkanShaderCode("solid.frag.spv");
    const VkShaderModule vertex = CreateVulkanShaderModule(context, vertexCode);
    const VkShaderModule fragment = CreateVulkanShaderModule(context, fragmentCode);
    if (vertex == VK_NULL_HANDLE || fragment == VK_NULL_HANDLE) {
        if (vertex != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, vertex, nullptr);
        if (fragment != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, fragment, nullptr);
        return false;
    }
    pipeline.layout = CreateVulkanModelPipelineLayout(context, frameDataLayout);
    if (pipeline.layout != VK_NULL_HANDLE) {
        pipeline.depth = CreateVulkanModelGraphicsPipeline(context, colorFormat, depthFormat,
                                                            pipeline.layout, vertex,
                                                            VK_NULL_HANDLE, false);
        pipeline.color = CreateVulkanModelGraphicsPipeline(context, colorFormat, depthFormat,
                                                            pipeline.layout, vertex, fragment, true);
    }
    vkDestroyShaderModule(context.device, fragment, nullptr);
    vkDestroyShaderModule(context.device, vertex, nullptr);
    if (!pipeline.IsReady()) {
        DestroyVulkanModelPipeline(context, pipeline);
        return false;
    }
    pipeline.frameDataLayout = frameDataLayout;
    return true;
}

void DestroyVulkanModelPipeline(const VulkanContext& context,
                                VulkanModelPipeline& pipeline) noexcept
{
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.depth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.depth, nullptr);
        }
        if (pipeline.color != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.color, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        }
    }
    pipeline = {};
}

} // namespace Concord
