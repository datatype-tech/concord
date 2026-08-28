// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBoxPipeline.h"

#include "engine/render/vulkan/VulkanBoxPipelineInternal.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <vector>

namespace Concord {

bool CreateVulkanBoxPipeline(const VulkanContext& context, VkFormat colorFormat,
                             VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                             VulkanBoxPipeline& pipeline,
                             VkDescriptorSetLayout shadowMapLayout,
                             VkDescriptorSetLayout rayTracingLayout)
{
    DestroyVulkanBoxPipeline(context, pipeline);
    if (context.device == VK_NULL_HANDLE || colorFormat == VK_FORMAT_UNDEFINED ||
        depthFormat == VK_FORMAT_UNDEFINED || frameDataLayout == VK_NULL_HANDLE) {
        return false;
    }
    const std::vector<u32> vertexCode = ReadVulkanShaderCode("mesh.vert.spv");
    const VkShaderModule vertex = CreateVulkanShaderModule(context, vertexCode);
    if (vertex == VK_NULL_HANDLE) {
        return false;
    }
    const char* fragmentName = shadowMapLayout == VK_NULL_HANDLE ? "solid.frag.spv"
                                                                   : "solid_shadow.frag.spv";
    const std::vector<u32> fragmentCode = ReadVulkanShaderCode(fragmentName);
    const VkShaderModule fragment = CreateVulkanShaderModule(context, fragmentCode);
    if (fragment == VK_NULL_HANDLE) {
        vkDestroyShaderModule(context.device, vertex, nullptr);
        return false;
    }
    const bool wantsRayQuery = rayTracingLayout != VK_NULL_HANDLE &&
                               context.rayTracing.IsRayQueryUsable();
    VkShaderModule rayQueryFragment = VK_NULL_HANDLE;
    if (wantsRayQuery) {
        const std::vector<u32> rayQueryCode = ReadVulkanShaderCode("solid_rayquery.frag.spv");
        rayQueryFragment = CreateVulkanShaderModule(context, rayQueryCode);
    }
    const VkDescriptorSetLayout layoutRay = rayQueryFragment == VK_NULL_HANDLE
                                                 ? VK_NULL_HANDLE
                                                 : rayTracingLayout;
    pipeline.layout = CreateVulkanBoxPipelineLayout(context, frameDataLayout, shadowMapLayout,
                                                      layoutRay, &pipeline.emptySetLayout);
    if (pipeline.layout != VK_NULL_HANDLE) {
        pipeline.depth = CreateVulkanBoxGraphicsPipeline(context, colorFormat, depthFormat,
                                                          pipeline.layout, vertex, VK_NULL_HANDLE,
                                                          false);
        pipeline.color = CreateVulkanBoxGraphicsPipeline(context, colorFormat, depthFormat,
                                                          pipeline.layout, vertex, fragment, true);
        if (rayQueryFragment != VK_NULL_HANDLE) {
            pipeline.rayQueryColor = CreateVulkanBoxGraphicsPipeline(
                context, colorFormat, depthFormat, pipeline.layout, vertex, rayQueryFragment, true);
        }
    }
    if (rayQueryFragment != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context.device, rayQueryFragment, nullptr);
    }
    vkDestroyShaderModule(context.device, fragment, nullptr);
    vkDestroyShaderModule(context.device, vertex, nullptr);
    if (!pipeline.HasDepth() || !pipeline.HasColor()) {
        DestroyVulkanBoxPipeline(context, pipeline);
        return false;
    }
    pipeline.frameDataLayout = frameDataLayout;
    pipeline.shadowMapLayout = shadowMapLayout;
    pipeline.rayTracingLayout = layoutRay;
    return true;
}

void DestroyVulkanBoxPipeline(const VulkanContext& context, VulkanBoxPipeline& pipeline)
{
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.depth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.depth, nullptr);
        }
        if (pipeline.color != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.color, nullptr);
        }
        if (pipeline.rayQueryColor != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.rayQueryColor, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        }
        if (pipeline.emptySetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, pipeline.emptySetLayout, nullptr);
        }
    }
    pipeline = {};
}

} // namespace Concord
