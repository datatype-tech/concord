// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBoxPipeline.h"

#include "engine/core/Color.h"
#include "engine/render/vulkan/VulkanBoxPipelineInternal.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Clamps a material scalar before it crosses the push-constant ABI. */
f32 SafeMaterial(f32 value, f32 fallback, f32 minimum, f32 maximum)
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

/** Keeps an emissive scalar finite without imposing an authoring ceiling. */
f32 SafeEmissive(f32 value)
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

/** Converts one object snapshot into the compact push-constant ABI. */
VulkanBoxPushConstants MakePushConstants(const RenderObjectSnapshot& object)
{
    VulkanBoxPushConstants push{};
    const Vec3 albedo = ToLinear(object.material.albedo);
    push.model = object.model;
    push.albedo = {albedo.x, albedo.y, albedo.z,
                   static_cast<f32>(ColorA(object.material.albedo)) / 255.0f};
    push.material = {SafeMaterial(object.material.metallic, 0.0f, 0.0f, 1.0f),
                     SafeMaterial(object.material.roughness, 0.8f, 0.04f, 1.0f),
                     SafeEmissive(object.material.emissive), 0.0f};
    return push;
}

/** Configures viewport and scissor for a dynamic-rendering pass. */
void SetViewport(VkCommandBuffer commandBuffer, VkExtent2D extent)
{
    VkViewport viewport{};
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

/** Draws every visible built-in Box using frame data from the descriptor set. */
void DrawObjects(VkCommandBuffer commandBuffer, const VulkanBoxPipeline& pipeline,
                 const RenderSceneSnapshot& snapshot, VkDescriptorSet frameDataSet,
                 VkDescriptorSet shadowMapSet, VkDescriptorSet rayTracingSet)
{
    if (!snapshot.hasCamera || !pipeline.HasColor() || frameDataSet == VK_NULL_HANDLE) {
        return;
    }
    const bool useRayQuery = pipeline.HasRayQuery() && rayTracingSet != VK_NULL_HANDLE;
    const VkPipeline colorPipeline = useRayQuery ? pipeline.rayQueryColor : pipeline.color;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, colorPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0,
                            1, &frameDataSet, 0, nullptr);
    if (pipeline.shadowMapLayout != VK_NULL_HANDLE && shadowMapSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                                1, 1, &shadowMapSet, 0, nullptr);
    }
    if (useRayQuery) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                                2, 1, &rayTracingSet, 0, nullptr);
    }
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.shape != PrimitiveShape::Box) {
            continue;
        }
        const VulkanBoxPushConstants push = MakePushConstants(object);
        vkCmdPushConstants(commandBuffer, pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push), &push);
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }
}

} // namespace

void RecordVulkanBoxColorPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                              VkImageView colorView, VkImageView depthView,
                              const VulkanBoxPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet, Vec3 clearColor,
                              VkDescriptorSet shadowMapSet, VkDescriptorSet rayTracingSet)
{
    if (!pipeline.HasColor() || colorView == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE ||
        frameDataSet == VK_NULL_HANDLE) {
        return;
    }
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{clearColor.x, clearColor.y, clearColor.z, 1.0f}};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttachment;
    rendering.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &rendering);
    SetViewport(commandBuffer, extent);
    DrawObjects(commandBuffer, pipeline, snapshot, frameDataSet, shadowMapSet, rayTracingSet);
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
