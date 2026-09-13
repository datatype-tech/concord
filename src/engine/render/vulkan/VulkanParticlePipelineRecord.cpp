// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanParticlePipeline.h"

#include "engine/render/RenderParticleLimits.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace Concord {
namespace {

/** Makes the freshly written host buffer visible to vertex fetch. */
void InsertParticleVertexBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer) noexcept
{
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0,
                         nullptr);
}

/** Configures viewport and scissor for the particle pass. */
void SetViewport(VkCommandBuffer commandBuffer, VkExtent2D extent) noexcept
{
    VkViewport viewport{};
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

} // namespace

void RecordVulkanParticlePass(VkCommandBuffer commandBuffer, VulkanParticlePipeline& pipeline,
                              u32 frameIndex, const RenderParticleSnapshot& particles,
                              VkDescriptorSet frameDataSet, VkImageView colorView,
                              VkImageView depthView, VkExtent2D extent) noexcept
{
    if (!pipeline.IsReady() || commandBuffer == VK_NULL_HANDLE ||
        frameDataSet == VK_NULL_HANDLE || colorView == VK_NULL_HANDLE ||
        depthView == VK_NULL_HANDLE || frameIndex >= kMaxFramesInFlight || extent.width == 0 ||
        extent.height == 0) {
        return;
    }
    const u32 vertexCount = static_cast<u32>(
        std::min<std::size_t>(particles.vertices.size(), kMaxParticleVertices));
    if (vertexCount == 0) {
        return;
    }
    VulkanBuffer& buffer = pipeline.vertexBuffers[frameIndex];
    const std::span<const RenderParticleVertex> view(particles.vertices.data(), vertexCount);
    if (!UploadVulkanBuffer(buffer, std::as_bytes(view))) {
        return;
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

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
    InsertParticleVertexBarrier(commandBuffer, buffer.buffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1,
                            &frameDataSet, 0, nullptr);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer.buffer, &offset);

    // Additive first, scattering second. Both runs share the uploaded buffer
    // and differ only in draw offset, which is what keeps a frame of particles
    // costing one upload no matter how many emitters fed it.
    const u32 additiveCount = std::min(particles.additiveVertices, vertexCount);
    if (additiveCount != 0) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdDraw(commandBuffer, additiveCount, 1, 0, 0);
    }
    if (vertexCount > additiveCount) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline.scatterPipeline);
        vkCmdDraw(commandBuffer, vertexCount - additiveCount, 1, additiveCount, 0);
    }
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
