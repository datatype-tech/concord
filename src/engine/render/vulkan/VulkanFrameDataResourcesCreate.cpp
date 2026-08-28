// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameDataResources.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <algorithm>
#include <cstdio>

namespace Concord {
namespace {

/** Keeps a valid storage descriptor when full tile memory is unavailable. */
bool CreateTileBuffers(const VulkanContext& context, VulkanFrameDataResources& resources)
{
    for (VulkanBuffer& buffer : resources.tileBuffers) {
        if (!CreateVulkanHostBuffer(context, TileLightBufferBytes(),
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, buffer)) {
            for (VulkanBuffer& replacement : resources.tileBuffers) {
                DestroyVulkanBuffer(context, replacement);
            }
            std::fprintf(stderr,
                         "[Concord] tile light-list allocation unavailable; "
                         "using all-light fallback\n");
            for (VulkanBuffer& fallback : resources.tileBuffers) {
                if (!CreateVulkanHostBuffer(context, kTileFallbackBufferBytes,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, fallback)) {
                    return false;
                }
            }
            return true;
        }
    }
    return true;
}

} // namespace

bool CreateVulkanFrameDataResources(const VulkanContext& context,
                                    VulkanFrameDataResources& resources)
{
    DestroyVulkanFrameDataResources(context, resources);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorSetLayoutBinding frameBinding{};
    frameBinding.binding = 0;
    frameBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameBinding.descriptorCount = 1;
    frameBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding tileBinding{};
    tileBinding.binding = 1;
    tileBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tileBinding.descriptorCount = 1;
    tileBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding bindings[] = {frameBinding, tileBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    VkResult result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                                   &resources.layout);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDescriptorSetLayout", result);
    }

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxFramesInFlight};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxFramesInFlight;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &resources.pool);
    if (result != VK_SUCCESS) {
        DestroyVulkanFrameDataResources(context, resources);
        return VulkanFailed("vkCreateDescriptorPool", result);
    }

    for (VulkanBuffer& buffer : resources.buffers) {
        if (!CreateVulkanHostBuffer(context, sizeof(RenderFrameData),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, buffer)) {
            DestroyVulkanFrameDataResources(context, resources);
            return false;
        }
    }
    if (!CreateTileBuffers(context, resources)) {
        DestroyVulkanFrameDataResources(context, resources);
        return false;
    }

    VkDescriptorSetLayout layouts[kMaxFramesInFlight]{};
    std::fill_n(layouts, kMaxFramesInFlight, resources.layout);
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = resources.pool;
    allocateInfo.descriptorSetCount = kMaxFramesInFlight;
    allocateInfo.pSetLayouts = layouts;
    result = vkAllocateDescriptorSets(context.device, &allocateInfo, resources.sets);
    if (result != VK_SUCCESS) {
        DestroyVulkanFrameDataResources(context, resources);
        return VulkanFailed("vkAllocateDescriptorSets", result);
    }

    VkDescriptorBufferInfo frameInfos[kMaxFramesInFlight]{};
    VkDescriptorBufferInfo tileInfos[kMaxFramesInFlight]{};
    VkWriteDescriptorSet writes[kMaxFramesInFlight * 2]{};
    for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
        frameInfos[i] = {resources.buffers[i].buffer, 0, sizeof(RenderFrameData)};
        tileInfos[i] = {resources.tileBuffers[i].buffer, 0, resources.tileBuffers[i].size};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = resources.sets[i];
        writes[i].dstBinding = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[i].pBufferInfo = &frameInfos[i];
        writes[kMaxFramesInFlight + i] = writes[i];
        writes[kMaxFramesInFlight + i].dstBinding = 1;
        writes[kMaxFramesInFlight + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[kMaxFramesInFlight + i].pBufferInfo = &tileInfos[i];
    }
    vkUpdateDescriptorSets(context.device, kMaxFramesInFlight * 2, writes, 0, nullptr);
    return resources.IsReady();
}

} // namespace Concord
