// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowMap.h"

#include "engine/render/vulkan/VulkanShadowMapInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {

/** Creates the comparison sampler and one future-facing sampled-image set. */
bool CreateVulkanShadowMapDescriptors(const VulkanContext& context,
                                      VulkanShadowMap& shadowMap)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.maxLod = 1.0f;
    VkResult result = vkCreateSampler(context.device, &samplerInfo, nullptr, &shadowMap.sampler);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateSampler(shadow)", result);
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = kDirectionalShadowMapBinding;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                         &shadowMap.descriptorLayout);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDescriptorSetLayout(shadow)", result);
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &shadowMap.descriptorPool);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDescriptorPool(shadow)", result);
    }
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = shadowMap.descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &shadowMap.descriptorLayout;
    result = vkAllocateDescriptorSets(context.device, &allocateInfo, &shadowMap.descriptorSet);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkAllocateDescriptorSets(shadow)", result);
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = shadowMap.sampler;
    imageInfo.imageView = shadowMap.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = shadowMap.descriptorSet;
    write.dstBinding = kDirectionalShadowMapBinding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    return shadowMap.IsReady();
}

bool BindVulkanShadowMap(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                         const VulkanShadowMap& shadowMap) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE ||
        !shadowMap.IsReady()) {
        return false;
    }
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            kDirectionalShadowMapSet, 1, &shadowMap.descriptorSet, 0, nullptr);
    return true;
}

void DestroyVulkanShadowMap(const VulkanContext& context,
                            VulkanShadowMap& shadowMap) noexcept
{
    const VkDevice device = shadowMap.device != VK_NULL_HANDLE ? shadowMap.device : context.device;
    if (device != VK_NULL_HANDLE && shadowMap.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, shadowMap.descriptorPool, nullptr);
    }
    if (device != VK_NULL_HANDLE && shadowMap.descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, shadowMap.descriptorLayout, nullptr);
    }
    if (device != VK_NULL_HANDLE && shadowMap.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, shadowMap.sampler, nullptr);
    }
    if (device != VK_NULL_HANDLE && shadowMap.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, shadowMap.view, nullptr);
    }
    if (device != VK_NULL_HANDLE && shadowMap.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, shadowMap.image, nullptr);
    }
    if (device != VK_NULL_HANDLE && shadowMap.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, shadowMap.memory, nullptr);
    }
    shadowMap = {};
}

} // namespace Concord
