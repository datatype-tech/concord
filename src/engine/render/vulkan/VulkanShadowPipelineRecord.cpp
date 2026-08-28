// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipeline.h"

#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"

namespace Concord {
namespace {

/** Configures the dynamic viewport for the shadow target. */
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

/** Records the built-in boxes that opt into shadow casting. */
void DrawShadowCasters(VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
                       const RenderSceneSnapshot& snapshot, const Mat4& lightViewProjection)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.depth);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.shape != PrimitiveShape::Box || !object.castShadow) {
            continue;
        }
        const VulkanShadowPushConstants push{lightViewProjection, object.model};
        vkCmdPushConstants(commandBuffer, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(push), &push);
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }
}

} // namespace

void RecordVulkanShadowPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                            VkImageView depthView, const VulkanShadowPipeline& pipeline,
                            const RenderSceneSnapshot& snapshot,
                            const Mat4& lightViewProjection) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE || !pipeline.IsReady() ||
        extent.width == 0 || extent.height == 0) {
        return;
    }
    VkRenderingAttachmentInfo attachment{};
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = depthView;
    attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.pDepthAttachment = &attachment;
    vkCmdBeginRendering(commandBuffer, &rendering);
    SetViewport(commandBuffer, extent);
    DrawShadowCasters(commandBuffer, pipeline, snapshot, lightViewProjection);
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
