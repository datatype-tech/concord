// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSkinningResources.h"

#include <array>

namespace Concord {

bool CreateVulkanSkinningResources(const VulkanContext& context,
                                   VulkanSkinningResources& resources)
{
    DestroyVulkanSkinningResources(context, resources);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE) return false;
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &resources.layout) !=
        VK_SUCCESS) return false;
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxFramesInFlight};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxFramesInFlight;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &resources.pool) != VK_SUCCESS) {
        DestroyVulkanSkinningResources(context, resources);
        return false;
    }
    for (VulkanBuffer& buffer : resources.buffers) {
        if (!CreateVulkanHostBuffer(context, sizeof(Mat4) * kMaxSkinningPaletteJoints,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, buffer)) {
            DestroyVulkanSkinningResources(context, resources);
            return false;
        }
    }
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(resources.layout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = resources.pool;
    allocateInfo.descriptorSetCount = kMaxFramesInFlight;
    allocateInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(context.device, &allocateInfo, resources.sets) != VK_SUCCESS) {
        DestroyVulkanSkinningResources(context, resources);
        return false;
    }
    std::array<VkDescriptorBufferInfo, kMaxFramesInFlight> infos{};
    std::array<VkWriteDescriptorSet, kMaxFramesInFlight> writes{};
    for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
        infos[i] = {resources.buffers[i].buffer, 0, resources.buffers[i].size};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = resources.sets[i];
        writes[i].dstBinding = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(context.device, kMaxFramesInFlight, writes.data(), 0, nullptr);
    return resources.IsReady();
}

bool UploadVulkanSkinningPalette(VulkanSkinningResources& resources, u32 frameIndex,
                                 const SkinningPaletteUpload& palette) noexcept
{
    if (!resources.IsReady() || frameIndex >= kMaxFramesInFlight ||
        palette.jointMatrices.size() > kMaxSkinningPaletteJoints) return false;
    return UploadVulkanBuffer(resources.buffers[frameIndex], SkinningPaletteBytes(palette));
}

bool BindVulkanSkinningPalette(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                               const VulkanSkinningResources& resources,
                               u32 frameIndex) noexcept
{
    if (!resources.IsReady() || commandBuffer == VK_NULL_HANDLE ||
        pipelineLayout == VK_NULL_HANDLE || frameIndex >= kMaxFramesInFlight) return false;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1,
                            1, &resources.sets[frameIndex], 0, nullptr);
    return true;
}

void InsertVulkanSkinningPaletteBarrier(VkCommandBuffer commandBuffer,
                                         const VulkanSkinningResources& resources,
                                         u32 frameIndex) noexcept
{
    if (!resources.IsReady() || commandBuffer == VK_NULL_HANDLE ||
        frameIndex >= kMaxFramesInFlight) return;
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = resources.buffers[frameIndex].buffer;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &barrier,
                         0, nullptr);
}

void DestroyVulkanSkinningResources(const VulkanContext& context,
                                    VulkanSkinningResources& resources) noexcept
{
    if (context.device != VK_NULL_HANDLE && resources.pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(context.device, resources.pool, nullptr);
    if (context.device != VK_NULL_HANDLE && resources.layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(context.device, resources.layout, nullptr);
    resources.pool = VK_NULL_HANDLE;
    resources.layout = VK_NULL_HANDLE;
    for (VkDescriptorSet& set : resources.sets) set = VK_NULL_HANDLE;
    for (VulkanBuffer& buffer : resources.buffers) DestroyVulkanBuffer(context, buffer);
}

} // namespace Concord
