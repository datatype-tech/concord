// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanPostProcess.h"

namespace Concord {
namespace {

constexpr u32 kPostProcessWorkgroupSize = 8;

void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                     VkImageLayout newLayout, VkPipelineStageFlags sourceStage,
                     VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess,
                     VkAccessFlags destinationAccess) noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);
}

/**
 * Points the slot's source binding at the frame being graded.
 *
 * Which image the pass reads is decided per frame, because it is whichever
 * slot the trace just wrote; only the destination is fixed at creation.
 */
void BindSource(VkDevice device, VulkanPostProcessRing& ring, u32 frameIndex,
                const VulkanRayTracingOutput& source) noexcept
{
    VkDescriptorImageInfo sourceInfo{ring.sampler, source.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = ring.items[frameIndex].descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &sourceInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace

bool RecordVulkanPostProcess(VkCommandBuffer commandBuffer, VulkanPostProcessRing& ring,
                             u32 frameIndex, const VulkanRayTracingOutput& source,
                             const VulkanPostProcessConstants& constants) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !ring.IsReady() || !source.IsReady() ||
        ring.device == VK_NULL_HANDLE || frameIndex >= kMaxFramesInFlight) {
        return false;
    }
    VulkanPostProcess& target = ring.items[frameIndex];
    if (target.extent.width != source.extent.width ||
        target.extent.height != source.extent.height) {
        return false;
    }
    BindSource(ring.device, ring, frameIndex, source);
    // The trace left raw radiance in a storage image; this stage reads it as a
    // sampled one, which the sampler needs to see as a read rather than a write.
    TransitionImage(commandBuffer, source.image, source.layout, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT);
    const bool targetUndefined = target.layout == VK_IMAGE_LAYOUT_UNDEFINED;
    const bool targetFromTransfer = target.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    TransitionImage(commandBuffer, target.image, target.layout, VK_IMAGE_LAYOUT_GENERAL,
                    targetUndefined ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                    : targetFromTransfer ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                         : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    targetUndefined ? 0
                    : targetFromTransfer ? VK_ACCESS_TRANSFER_READ_BIT
                                         : VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT);
    target.layout = VK_IMAGE_LAYOUT_GENERAL;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ring.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ring.pipelineLayout, 0,
                            1, &target.descriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, ring.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(constants), &constants);
    vkCmdDispatch(commandBuffer,
                  (target.extent.width + kPostProcessWorkgroupSize - 1u) /
                      kPostProcessWorkgroupSize,
                  (target.extent.height + kPostProcessWorkgroupSize - 1u) /
                      kPostProcessWorkgroupSize,
                  1);

    // Left where a blit expects it, so the composite transitions from the
    // layout the image is actually in.
    TransitionImage(commandBuffer, target.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT);
    target.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    return true;
}

} // namespace Concord
