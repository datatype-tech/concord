// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/render/vulkan/VulkanUiPipeline.h"

#include "engine/render/vulkan/VulkanShaderModule.h"

#include <imgui.h>

#include <cstddef>
#include <cstdio>

namespace Concord {

void DestroyVulkanUiPipeline(const VulkanContext& context, VulkanUiPipeline& pipeline)
{
    if (pipeline.pipeline) vkDestroyPipeline(context.device, pipeline.pipeline, nullptr);
    if (pipeline.layout) vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
    if (pipeline.descriptorLayout) vkDestroyDescriptorSetLayout(context.device, pipeline.descriptorLayout, nullptr);
    pipeline = {};
}

bool CreateVulkanUiPipeline(const VulkanContext& context, VkFormat format, VulkanUiPipeline& pipeline)
{
    const auto vertex = CreateVulkanShaderModule(context, ReadVulkanShaderCode("ui_toolkit.vert.spv"));
    const auto fragment = CreateVulkanShaderModule(context, ReadVulkanShaderCode("ui_toolkit.frag.spv"));
    if (!vertex || !fragment) {
        if (vertex) vkDestroyShaderModule(context.device, vertex, nullptr);
        if (fragment) vkDestroyShaderModule(context.device, fragment, nullptr);
        std::fprintf(stderr, "[Concord] native UI shaders are missing; stage ui_toolkit.vert.spv and ui_toolkit.frag.spv\n");
        return false;
    }

    // Identical definitions make these layouts compatible with the descriptor
    // sets and push constants bound by the unmodified ImGui Vulkan backend.
    VkDescriptorSetLayoutBinding binding{};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorInfo.bindingCount = 1;
    descriptorInfo.pBindings = &binding;
    VkResult result = vkCreateDescriptorSetLayout(context.device, &descriptorInfo, nullptr, &pipeline.descriptorLayout);
    if (result == VK_SUCCESS) {
        const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &pipeline.descriptorLayout;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        result = vkCreatePipelineLayout(context.device, &layout, nullptr, &pipeline.layout);
    }
    if (result == VK_SUCCESS) {
        const VkBool32 linearize = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
        const VkSpecializationMapEntry mapping{0, 0, sizeof(linearize)};
        const VkSpecializationInfo specialization{1, &mapping, sizeof(linearize), &linearize};
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex;
        stages[0].pName = "main";
        stages[0].pSpecializationInfo = &specialization;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment;
        stages[1].pName = "main";

        const VkVertexInputBindingDescription vertexBinding{0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX};
        const VkVertexInputAttributeDescription attributes[]{
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)},
            {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)},
        };
        VkPipelineVertexInputStateCreateInfo vertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertices.vertexBindingDescriptionCount = 1;
        vertices.pVertexBindingDescriptions = &vertexBinding;
        vertices.vertexAttributeDescriptionCount = 3;
        vertices.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        const VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = states;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &format;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.pNext = &rendering;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertices;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &samples;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = pipeline.layout;
        result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline.pipeline);
    }
    vkDestroyShaderModule(context.device, vertex, nullptr);
    vkDestroyShaderModule(context.device, fragment, nullptr);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "[Concord] native UI pipeline creation failed: %d\n", static_cast<int>(result));
        DestroyVulkanUiPipeline(context, pipeline);
    }
    return result == VK_SUCCESS;
}

} // namespace Concord
