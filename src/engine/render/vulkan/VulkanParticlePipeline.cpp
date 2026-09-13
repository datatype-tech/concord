// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanParticlePipeline.h"

#include "engine/render/RenderParticleLimits.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <cstddef>
#include <vector>

namespace Concord {
namespace {

/** Bytes one frame of particle geometry may occupy. */
constexpr VkDeviceSize kParticleVertexBytes =
    static_cast<VkDeviceSize>(kMaxParticleVertices) * sizeof(RenderParticleVertex);

/** Creates the shared pipeline layout the two blend states are built against. */
bool CreatePipelineLayoutFor(const VulkanContext& context,
                             VkDescriptorSetLayout frameDataLayout,
                             VulkanParticlePipeline& output)
{
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &frameDataLayout;
    return vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &output.layout) ==
           VK_SUCCESS;
}

/**
 * Builds one graphics pipeline against the shared layout.
 *
 * @param destination How the billboards combine with what is behind them. Both
 *        states consume the same premultiplied output, so one fragment stage
 *        could serve both; they are separate shaders only because smoke also
 *        scatters the frame's light and emission does not.
 */
bool CreatePipeline(const VulkanContext& context, VkFormat colorFormat, VkFormat depthFormat,
                    const std::vector<u32>& vertexCode, const std::vector<u32>& fragmentCode,
                    VkBlendFactor destinationBlend, VulkanParticlePipeline& output,
                    VkPipeline& target)
{
    const VkShaderModule vertex = CreateVulkanShaderModule(context, vertexCode);
    const VkShaderModule fragment = CreateVulkanShaderModule(context, fragmentCode);
    if (vertex == VK_NULL_HANDLE || fragment == VK_NULL_HANDLE) {
        if (vertex != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, vertex, nullptr);
        if (fragment != VK_NULL_HANDLE) vkDestroyShaderModule(context.device, fragment, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    stages[1] = stages[0];
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment;

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(RenderParticleVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributes[3]{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                     static_cast<u32>(offsetof(RenderParticleVertex, position))};
    attributes[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT,
                     static_cast<u32>(offsetof(RenderParticleVertex, texcoord))};
    attributes[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                     static_cast<u32>(offsetof(RenderParticleVertex, color))};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

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
    // Billboards are built per frame with an arbitrary winding, so no face is
    // culled and the vertex order never has to be corrected on the CPU.
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    // Particles must be occluded by opaque geometry but never occlude each
    // other, which also makes their draw order irrelevant.
    depth.depthWriteEnable = VK_FALSE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    // Premultiplied output, so additive is (ONE, ONE) and scattering is
    // (ONE, ONE_MINUS_SRC_ALPHA) with the same four numbers from the shader.
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstColorBlendFactor = destinationBlend;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = destinationBlend;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    const VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = states;
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &colorFormat;
    rendering.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &rendering;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = output.layout;
    const VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &info,
                                                      nullptr, &target);
    vkDestroyShaderModule(context.device, vertex, nullptr);
    vkDestroyShaderModule(context.device, fragment, nullptr);
    if (result != VK_SUCCESS) {
        VulkanFailed("vkCreateGraphicsPipelines(particle)", result);
        return false;
    }
    return true;
}

} // namespace

bool CreateVulkanParticlePipeline(const VulkanContext& context, VkFormat colorFormat,
                                  VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                                  VulkanParticlePipeline& output)
{
    DestroyVulkanParticlePipeline(context, output);
    if (context.device == VK_NULL_HANDLE || colorFormat == VK_FORMAT_UNDEFINED ||
        depthFormat == VK_FORMAT_UNDEFINED || frameDataLayout == VK_NULL_HANDLE) {
        return false;
    }
    const std::vector<u32> vertexCode = ReadVulkanShaderCode("particle.vert.spv");
    const std::vector<u32> fragmentCode = ReadVulkanShaderCode("particle.frag.spv");
    // A frame with no scattering emitter never loads this module, but a missing
    // one still has to fail loudly rather than leave smoke drawn as fire.
    const std::vector<u32> smokeCode = ReadVulkanShaderCode("smoke.frag.spv");
    if (vertexCode.empty() || fragmentCode.empty() || smokeCode.empty()) {
        return false;
    }
    for (VulkanBuffer& buffer : output.vertexBuffers) {
        if (!CreateVulkanHostBuffer(context, kParticleVertexBytes,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, buffer)) {
            DestroyVulkanParticlePipeline(context, output);
            return false;
        }
    }
    if (!CreatePipelineLayoutFor(context, frameDataLayout, output) ||
        !CreatePipeline(context, colorFormat, depthFormat, vertexCode, fragmentCode,
                        VK_BLEND_FACTOR_ONE, output, output.pipeline) ||
        !CreatePipeline(context, colorFormat, depthFormat, vertexCode, smokeCode,
                        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, output, output.scatterPipeline) ||
        !output.IsReady()) {
        DestroyVulkanParticlePipeline(context, output);
        return false;
    }
    return true;
}

void DestroyVulkanParticlePipeline(const VulkanContext& context,
                                   VulkanParticlePipeline& pipeline) noexcept
{
    for (VulkanBuffer& buffer : pipeline.vertexBuffers) {
        DestroyVulkanBuffer(context, buffer);
    }
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.pipeline, nullptr);
        }
        if (pipeline.scatterPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.scatterPipeline, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        }
    }
    pipeline = {};
}

} // namespace Concord
