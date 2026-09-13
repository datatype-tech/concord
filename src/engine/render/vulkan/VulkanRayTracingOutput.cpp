// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingOutput.h"

#include "engine/render/vulkan/VulkanRayTracingOutputInternal.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace Concord {
f32 VulkanRenderScale() noexcept
{
    static const f32 scale = [] {
        const char* value = std::getenv("CONCORD_RENDER_SCALE");
        if (value == nullptr || *value == '\0') {
            return kVulkanDefaultRenderScale;
        }
        const f64 parsed = std::strtod(value, nullptr);
        // A value outside the range is reported rather than silently clamped,
        // because a caller who asked for half resolution and got full
        // resolution has no other way to find out why nothing got faster.
        if (!(parsed > 0.0) || parsed > 1.0) {
            std::fprintf(stderr,
                         "[Concord] CONCORD_RENDER_SCALE=%s is outside 0..1; using %.2f\n",
                         value, static_cast<f64>(kVulkanDefaultRenderScale));
            return kVulkanDefaultRenderScale;
        }
        return static_cast<f32>(parsed);
    }();
    return scale;
}

VkExtent2D ResolveVulkanRenderExtent(VkExtent2D output) noexcept
{
    const f32 scale = VulkanRenderScale();
    VkExtent2D extent{};
    extent.width = static_cast<u32>(static_cast<f32>(output.width) * scale + 0.5f);
    extent.height = static_cast<u32>(static_cast<f32>(output.height) * scale + 0.5f);
    extent.width = extent.width != 0 ? extent.width : 1u;
    extent.height = extent.height != 0 ? extent.height : 1u;
    return extent;
}

bool CreateVulkanRayTracingOutputRing(const VulkanContext& context,
                                      VkDescriptorSetLayout descriptorLayout,
                                      VkExtent2D extent,
                                      VulkanRayTracingOutputRing& ring)
{
    DestroyVulkanRayTracingOutputRing(context, ring);
    if (context.device == VK_NULL_HANDLE || descriptorLayout == VK_NULL_HANDLE ||
        extent.width == 0 || extent.height == 0) {
        return false;
    }
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxFramesInFlight};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxFramesInFlight;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &ring.descriptorPool) != VK_SUCCESS) {
        return false;
    }
    ring.device = context.device;
    for (VulkanRayTracingOutput& output : ring.outputs) {
        if (!CreateVulkanRayTracingOutputImage(context, extent, output)) {
            DestroyVulkanRayTracingOutputRing(context, ring);
            return false;
        }
    }
    VkDescriptorSetLayout layouts[kMaxFramesInFlight]{};
    std::fill_n(layouts, kMaxFramesInFlight, descriptorLayout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = ring.descriptorPool;
    allocateInfo.descriptorSetCount = kMaxFramesInFlight;
    allocateInfo.pSetLayouts = layouts;
    VkDescriptorSet sets[kMaxFramesInFlight]{};
    if (vkAllocateDescriptorSets(context.device, &allocateInfo, sets) != VK_SUCCESS) {
        DestroyVulkanRayTracingOutputRing(context, ring);
        return false;
    }
    for (u32 index = 0; index < kMaxFramesInFlight; ++index) {
        ring.outputs[index].descriptorSet = sets[index];
        VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, ring.outputs[index].view,
                                        VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[index];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    }
    return ring.IsReady();
}

void DestroyVulkanRayTracingOutputRing(const VulkanContext& context,
                                       VulkanRayTracingOutputRing& ring) noexcept
{
    const VkDevice device = ring.device != VK_NULL_HANDLE ? ring.device : context.device;
    if (device != VK_NULL_HANDLE && ring.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, ring.descriptorPool, nullptr);
    }
    ring.device = VK_NULL_HANDLE;
    ring.descriptorPool = VK_NULL_HANDLE;
    for (VulkanRayTracingOutput& output : ring.outputs) {
        DestroyVulkanRayTracingOutputImage(context, output);
    }
}

void PrepareVulkanRayTracingOutput(VkCommandBuffer commandBuffer,
                                   VulkanRayTracingOutput& output) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !output.IsReady()) {
        return;
    }
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    const bool fromUndefined = output.layout == VK_IMAGE_LAYOUT_UNDEFINED;
    const bool fromTransfer = output.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = fromUndefined ? 0 : fromTransfer ? VK_ACCESS_TRANSFER_READ_BIT
                                                               : VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = output.layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = output.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    const VkPipelineStageFlags srcStage = fromUndefined
                                              ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                              : fromTransfer ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                                              : VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    vkCmdPipelineBarrier(commandBuffer, srcStage, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    output.layout = VK_IMAGE_LAYOUT_GENERAL;
}

} // namespace Concord
