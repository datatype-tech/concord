// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelPipeline.h"

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

namespace Concord {
namespace {

VkRenderingInfo MakeDepthRendering(VkRenderingAttachmentInfo& attachment, VkExtent2D extent,
                                   VkImageView depthView, bool clear)
{
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = depthView;
    attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.pDepthAttachment = &attachment;
    return rendering;
}

} // namespace

void RecordVulkanModelDepthPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                VkImageView depthView, const VulkanModelPipeline& pipeline,
                                const RenderSceneSnapshot& snapshot,
                                VkDescriptorSet frameDataSet,
                                const VulkanModelAssetCache& cache, bool clearDepth)
{
    if (!pipeline.HasDepth() || commandBuffer == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE ||
        frameDataSet == VK_NULL_HANDLE) return;
    InsertVulkanModelInputBarrier(commandBuffer, snapshot, cache);
    VkRenderingAttachmentInfo attachment{};
    const VkRenderingInfo rendering = MakeDepthRendering(attachment, extent, depthView,
                                                          clearDepth);
    vkCmdBeginRendering(commandBuffer, &rendering);
    SetVulkanModelViewport(commandBuffer, extent);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.depth);
    RecordVulkanModelDraws(commandBuffer, pipeline, snapshot, frameDataSet, cache);
    vkCmdEndRendering(commandBuffer);
}

void RecordVulkanModelColorPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                VkImageView colorView, VkImageView depthView,
                                const VulkanModelPipeline& pipeline,
                                const RenderSceneSnapshot& snapshot,
                                VkDescriptorSet frameDataSet,
                                const VulkanModelAssetCache& cache, Vec3 clearColor,
                                bool clearColorTarget)
{
    if (!pipeline.HasColor() || commandBuffer == VK_NULL_HANDLE || colorView == VK_NULL_HANDLE ||
        depthView == VK_NULL_HANDLE || frameDataSet == VK_NULL_HANDLE) return;
    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = colorView;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = clearColorTarget ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{clearColor.x, clearColor.y, clearColor.z, 1.0f}};
    VkRenderingAttachmentInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth.imageView = depthView;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    rendering.pDepthAttachment = &depth;
    vkCmdBeginRendering(commandBuffer, &rendering);
    SetVulkanModelViewport(commandBuffer, extent);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.color);
    RecordVulkanModelDraws(commandBuffer, pipeline, snapshot, frameDataSet, cache);
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
