// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipeline.h"

#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanShaderModule.h"
#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"

#include <vector>

namespace Concord {
namespace {
/** Creates a pipeline layout containing only the vertex shadow ABI. */
VkPipelineLayout CreateLayout(const VulkanContext& context)
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.size = sizeof(VulkanShadowPushConstants);
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &range;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    return vkCreatePipelineLayout(context.device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}
/** Creates a depth-only dynamic-rendering graphics pipeline. */
VkPipeline CreateDepthPipeline(const VulkanContext& context, VkFormat depthFormat,
                               VkPipelineLayout layout, VkShaderModule vertex)
{
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = vertex;
    stage.pName = "main";
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.depthAttachmentFormat = depthFormat;
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &rendering;
    info.stageCount = 1;
    info.pStages = &stage;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pDynamicState = &dynamic;
    info.layout = layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        VulkanFailed("vkCreateGraphicsPipelines(shadow)", result);
        return VK_NULL_HANDLE;
    }
    return pipeline;
}
} // namespace
bool CreateVulkanShadowPipeline(const VulkanContext& context, VkFormat depthFormat,
                                VulkanShadowPipeline& pipeline)
{
    DestroyVulkanShadowPipeline(context, pipeline);
    if (context.device == VK_NULL_HANDLE || depthFormat == VK_FORMAT_UNDEFINED) {
        return false;
    }
    const std::vector<u32> code = ReadVulkanShaderCode("directional_shadow.vert.spv");
    const std::vector<u32> modelCode = ReadVulkanShaderCode("directional_shadow_model.vert.spv");
    const VkShaderModule shader = CreateVulkanShaderModule(context, code);
    const VkShaderModule modelShader = CreateVulkanShaderModule(context, modelCode);
    if (shader == VK_NULL_HANDLE) {
        if (modelShader != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, modelShader, nullptr);
        return false;
    }
    pipeline.layout = CreateLayout(context);
    if (pipeline.layout != VK_NULL_HANDLE) {
        pipeline.depth = CreateDepthPipeline(context, depthFormat, pipeline.layout, shader);
    }
    if (modelShader != VK_NULL_HANDLE) {
        pipeline.modelLayout = CreateVulkanModelShadowPipelineLayout(context);
        if (pipeline.modelLayout != VK_NULL_HANDLE) {
            pipeline.modelDepth = CreateVulkanModelShadowDepthPipeline(
                context, depthFormat, pipeline.modelLayout, modelShader);
        }
    }
    if (modelShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context.device, modelShader, nullptr);
    }
    vkDestroyShaderModule(context.device, shader, nullptr);
    if (!pipeline.IsReady()) {
        DestroyVulkanShadowPipeline(context, pipeline);
        return false;
    }
    return true;
}
void DestroyVulkanShadowPipeline(const VulkanContext& context,
                                 VulkanShadowPipeline& pipeline) noexcept
{
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.depth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.depth, nullptr);
        }
        if (pipeline.modelDepth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.modelDepth, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        }
        if (pipeline.modelLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.modelLayout, nullptr);
        }
    }
    pipeline = {};
}

} // namespace Concord
