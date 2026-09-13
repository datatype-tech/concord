// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanPostProcess.h"

#include "engine/render/vulkan/VulkanPostProcessInternal.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <array>
#include <vector>

namespace Concord {
namespace {

/** Declares the frame to read and the graded image to write. */
bool CreateDescriptorLayout(const VulkanContext& context, VulkanPostProcessRing& ring,
                            std::array<VkDescriptorSetLayoutBinding, 2>& bindings)
{
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    const VkResult result =
        vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &ring.setLayout);
    return result == VK_SUCCESS ? true
                                : VulkanFailed("vkCreateDescriptorSetLayout(post)", result);
}

bool CreatePipeline(const VulkanContext& context, const std::vector<u32>& code,
                    VulkanPostProcessRing& ring)
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.size = sizeof(VulkanPostProcessConstants);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &ring.setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &ring.pipelineLayout) !=
        VK_SUCCESS) {
        return false;
    }
    const VkShaderModule shader = CreateVulkanShaderModule(context, code);
    if (shader == VK_NULL_HANDLE) {
        return false;
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName = "main";
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stage;
    pipelineInfo.layout = ring.pipelineLayout;
    const VkResult result = vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1,
                                                     &pipelineInfo, nullptr, &ring.pipeline);
    vkDestroyShaderModule(context.device, shader, nullptr);
    return result == VK_SUCCESS ? true : VulkanFailed("vkCreateComputePipelines(post)", result);
}

/** Point samples and clamps: the pass never wants a tap from outside the frame. */
VkSamplerCreateInfo SamplerInfo() noexcept
{
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.maxLod = 0.0f;
    return info;
}

bool CreateDescriptors(const VulkanContext& context, VulkanPostProcessRing& ring)
{
    const VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxFramesInFlight},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxFramesInFlight},
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxFramesInFlight;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &ring.descriptorPool) !=
        VK_SUCCESS) {
        return false;
    }
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(ring.setLayout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = ring.descriptorPool;
    allocateInfo.descriptorSetCount = kMaxFramesInFlight;
    allocateInfo.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, kMaxFramesInFlight> sets{};
    if (vkAllocateDescriptorSets(context.device, &allocateInfo, sets.data()) != VK_SUCCESS) {
        return false;
    }
    // Only the destination is written here. The source is written per frame,
    // because it is whichever frame slot is being traced.
    for (u32 index = 0; index < kMaxFramesInFlight; ++index) {
        ring.items[index].descriptorSet = sets[index];
        VkDescriptorImageInfo targetInfo{VK_NULL_HANDLE, ring.items[index].view,
                                         VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[index];
        write.dstBinding = 1;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &targetInfo;
        vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    }
    return true;
}

} // namespace

bool CreateVulkanPostProcessRing(const VulkanContext& context, VkExtent2D extent,
                                 VulkanPostProcessRing& ring)
{
    DestroyVulkanPostProcessRing(context, ring);
    if (context.device == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
        return false;
    }
    ring.device = context.device;
    const VkSamplerCreateInfo samplerInfo = SamplerInfo();
    if (vkCreateSampler(context.device, &samplerInfo, nullptr, &ring.sampler) != VK_SUCCESS) {
        DestroyVulkanPostProcessRing(context, ring);
        return false;
    }
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    if (!CreateDescriptorLayout(context, ring, bindings)) {
        DestroyVulkanPostProcessRing(context, ring);
        return false;
    }
    for (VulkanPostProcess& slot : ring.items) {
        if (!CreateVulkanPostProcessImage(context, extent, slot)) {
            DestroyVulkanPostProcessRing(context, ring);
            return false;
        }
    }
    if (!CreateDescriptors(context, ring)) {
        DestroyVulkanPostProcessRing(context, ring);
        return false;
    }
    const std::vector<u32> code = ReadVulkanShaderCode("post.comp.spv");
    if (code.empty() || !CreatePipeline(context, code, ring)) {
        DestroyVulkanPostProcessRing(context, ring);
        return false;
    }
    return ring.IsReady();
}

void DestroyVulkanPostProcessRing(const VulkanContext& context,
                                  VulkanPostProcessRing& ring) noexcept
{
    const VkDevice device = ring.device != VK_NULL_HANDLE ? ring.device : context.device;
    if (device != VK_NULL_HANDLE) {
        if (ring.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, ring.descriptorPool, nullptr);
        }
        if (ring.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, ring.pipeline, nullptr);
        }
        if (ring.pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, ring.pipelineLayout, nullptr);
        }
        if (ring.setLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, ring.setLayout, nullptr);
        }
        if (ring.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, ring.sampler, nullptr);
        }
    }
    for (VulkanPostProcess& slot : ring.items) {
        DestroyVulkanPostProcessImage(context, slot);
    }
    ring = {};
}

} // namespace Concord
