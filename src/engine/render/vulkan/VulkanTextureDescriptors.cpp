// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTextureInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {

bool CreateVulkanTextureDescriptors(const VulkanContext& context, VulkanTexture& texture,
                                    VkDescriptorSetLayout descriptorLayout,
                                    VkDescriptorPool descriptorPool,
                                    bool ownsDescriptorObjects)
{
    if (context.device == VK_NULL_HANDLE || texture.image == VK_NULL_HANDLE ||
        texture.format == VK_FORMAT_UNDEFINED ||
        ((descriptorLayout == VK_NULL_HANDLE) != (descriptorPool == VK_NULL_HANDLE))) {
        return false;
    }
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = texture.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VkResult result = vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view);
    if (result != VK_SUCCESS) return VulkanFailed("vkCreateImageView(texture)", result);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxLod = 1.0f;
    result = vkCreateSampler(context.device, &samplerInfo, nullptr, &texture.sampler);
    if (result != VK_SUCCESS) return VulkanFailed("vkCreateSampler(texture)", result);

    const bool createLayout = descriptorLayout == VK_NULL_HANDLE;
    const bool createPool = descriptorPool == VK_NULL_HANDLE;
    if (createLayout) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = kVulkanTextureBinding;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                             &descriptorLayout);
        if (result != VK_SUCCESS) return VulkanFailed("vkCreateDescriptorSetLayout(texture)", result);
    }
    if (createPool) {
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool);
        if (result != VK_SUCCESS) {
            if (createLayout) vkDestroyDescriptorSetLayout(context.device, descriptorLayout, nullptr);
            return VulkanFailed("vkCreateDescriptorPool(texture)", result);
        }
    }
    texture.descriptorLayout = descriptorLayout;
    texture.descriptorPool = descriptorPool;
    texture.ownsDescriptorObjects = ownsDescriptorObjects && createLayout && createPool;
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = descriptorPool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &descriptorLayout;
    result = vkAllocateDescriptorSets(context.device, &allocate, &texture.descriptorSet);
    if (result != VK_SUCCESS) {
        if (createPool) vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
        if (createLayout) vkDestroyDescriptorSetLayout(context.device, descriptorLayout, nullptr);
        texture.descriptorPool = VK_NULL_HANDLE;
        texture.descriptorLayout = VK_NULL_HANDLE;
        texture.ownsDescriptorObjects = true;
        return VulkanFailed("vkAllocateDescriptorSets(texture)", result);
    }
    VkDescriptorImageInfo imageInfo{texture.sampler, texture.view,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = texture.descriptorSet;
    write.dstBinding = kVulkanTextureBinding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    return true;
}

} // namespace Concord
