// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBoxPipeline.h"

#include "engine/render/vulkan/VulkanBoxPipelineInternal.h"

namespace Concord {
namespace {

/** Configures the dynamic viewport used by both optional passes. */
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

/** Emits one indexed-by-convention unit-cube draw using its model push value. */
void DrawObjects(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkPipeline pipeline,
                 const RenderSceneSnapshot& snapshot, VkDescriptorSet frameDataSet)
{
    if (pipeline == VK_NULL_HANDLE || !snapshot.hasCamera || frameDataSet == VK_NULL_HANDLE) {
        return;
    }
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                            &frameDataSet, 0, nullptr);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.shape != PrimitiveShape::Box) {
            continue;
        }
        VulkanBoxPushConstants push{};
        push.model = object.model;
        vkCmdPushConstants(commandBuffer, layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push), &push);
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }
}

/** Starts a depth-only dynamic rendering scope. */
VkRenderingInfo BeginDepthRendering(VkRenderingAttachmentInfo& attachment, VkExtent2D extent,
                                    VkImageView depthView)
{
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = depthView;
    attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.extent = extent;
    info.layerCount = 1;
    info.pDepthAttachment = &attachment;
    return info;
}

} // namespace

void RecordVulkanBoxDepthPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                              VkImageView depthView, const VulkanBoxPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet)
{
    if (!pipeline.HasDepth() || depthView == VK_NULL_HANDLE) {
        return;
    }
    VkRenderingAttachmentInfo depthAttachment{};
    const VkRenderingInfo rendering = BeginDepthRendering(depthAttachment, extent, depthView);
    vkCmdBeginRendering(commandBuffer, &rendering);
    SetViewport(commandBuffer, extent);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.depth);
    DrawObjects(commandBuffer, pipeline.layout, pipeline.depth, snapshot, frameDataSet);
    vkCmdEndRendering(commandBuffer);
}

void InsertVulkanBoxDepthBarrier(VkCommandBuffer commandBuffer, VkImage depthImage)
{
    if (depthImage == VK_NULL_HANDLE) {
        return;
    }
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = depthImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace Concord
