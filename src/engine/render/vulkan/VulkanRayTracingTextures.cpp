// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingTextures.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {
namespace {

/** Resolves one slot's key against the cache, or null when it is not resident. */
const VulkanTexture* ResolveSlot(const VulkanTextureCache& cache,
                                 const RayTracingTextureSlots& slots, u32 slot) noexcept
{
    const std::string_view key = RayTracingTextureSlotKey(slots, slot);
    if (key.empty()) {
        return nullptr;
    }
    for (const VulkanTextureCacheEntry& entry : cache.entries) {
        if (entry.key == key && entry.texture.IsUploaded()) {
            return &entry.texture;
        }
    }
    return nullptr;
}

} // namespace

bool CreateVulkanRayTracingTextures(const VulkanContext& context,
                                    VulkanRayTracingTextures& textures)
{
    DestroyVulkanRayTracingTextures(context, textures);
    // Without dynamic sampler-array indexing the closest-hit stage could not
    // read this array, so building it would only cost descriptors.
    if (context.device == VK_NULL_HANDLE || !context.rayTracing.IsUsable() ||
        !context.samplerArrayIndexing) {
        return false;
    }
    textures.device = context.device;

    VkDescriptorSetLayoutBinding bindings[1]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = kMaxRayTracingTextureSlots;
    bindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &textures.layout) !=
        VK_SUCCESS) {
        DestroyVulkanRayTracingTextures(context, textures);
        return false;
    }

    VkDescriptorPoolSize poolSizes[1]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = kMaxRayTracingTextureSlots;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &textures.pool) != VK_SUCCESS) {
        DestroyVulkanRayTracingTextures(context, textures);
        return false;
    }

    VkDescriptorSetAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = textures.pool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &textures.layout;
    if (vkAllocateDescriptorSets(context.device, &allocation, &textures.set) != VK_SUCCESS) {
        textures.set = VK_NULL_HANDLE;
        DestroyVulkanRayTracingTextures(context, textures);
        return false;
    }
    return textures.IsReady();
}

void DestroyVulkanRayTracingTextures(const VulkanContext& context,
                                     VulkanRayTracingTextures& textures) noexcept
{
    if (context.device != VK_NULL_HANDLE) {
        if (textures.pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(context.device, textures.pool, nullptr);
        }
        if (textures.layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, textures.layout, nullptr);
        }
    }
    textures = {};
}

bool UpdateVulkanRayTracingTextures(const VulkanContext& context,
                                    VulkanRayTracingTextures& textures,
                                    const VulkanTextureCache& cache,
                                    const RayTracingTextureSlots& slots) noexcept
{
    if (!textures.IsReady() || !cache.IsReady()) {
        return false;
    }
    const VulkanTexture* fallback = cache.Fallback();
    if (fallback == nullptr) {
        return false;
    }
    VkDescriptorImageInfo images[kMaxRayTracingTextureSlots]{};
    for (u32 slot = 0; slot < kMaxRayTracingTextureSlots; ++slot) {
        const VulkanTexture* texture = ResolveSlot(cache, slots, slot);
        if (texture == nullptr) {
            texture = fallback;
        }
        images[slot].sampler = texture->sampler;
        images[slot].imageView = texture->view;
        images[slot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = textures.set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = kMaxRayTracingTextureSlots;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = images;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    return true;
}

} // namespace Concord
