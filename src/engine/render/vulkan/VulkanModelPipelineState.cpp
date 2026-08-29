// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

#include "engine/asset/ModelAsset.h"

#include <cstddef>

namespace Concord {

VkPipeline CreateVulkanModelGraphicsPipeline(const VulkanContext& context,
                                             VkFormat colorFormat, VkFormat depthFormat,
                                             VkPipelineLayout layout, VkShaderModule vertex,
                                             VkShaderModule fragment, bool withColor)
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(ModelVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ModelVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ModelVertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ModelVertex, texcoord)},
    };
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    u32 stageCount = 1;
    if (withColor && fragment != VK_NULL_HANDLE) {
        stages[1].sType = stages[0].sType;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment;
        stages[1].pName = "main";
        stageCount = 2;
    }
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
    depth.depthWriteEnable = withColor ? VK_FALSE : VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = 0xFu;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = withColor ? 1u : 0u;
    blend.pAttachments = withColor ? &blendAttachment : nullptr;
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = withColor ? 1u : 0u;
    rendering.pColorAttachmentFormats = withColor ? &colorFormat : nullptr;
    rendering.depthAttachmentFormat = depthFormat;
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &rendering;
    info.stageCount = stageCount;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    return vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &info, nullptr,
                                     &pipeline) == VK_SUCCESS
               ? pipeline
               : VK_NULL_HANDLE;
}

} // namespace Concord
