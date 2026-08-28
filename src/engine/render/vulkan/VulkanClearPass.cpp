// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanClearPass.h"

namespace Concord {

void RecordClearPass(VkCommandBuffer commandBuffer, VkImageView target, VkExtent2D extent,
                     Vec3 color, VkImageView depthTarget)
{
    VkRenderingAttachmentInfo attachment{};
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = target;
    attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.color = {{color.x, color.y, color.z, 1.0f}};

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &attachment;

    VkRenderingAttachmentInfo depthAttachment{};
    if (depthTarget != VK_NULL_HANDLE) {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthTarget;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};
        rendering.pDepthAttachment = &depthAttachment;
    }

    vkCmdBeginRendering(commandBuffer, &rendering);
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
