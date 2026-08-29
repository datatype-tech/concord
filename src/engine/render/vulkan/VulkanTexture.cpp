// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTexture.h"

#include "engine/render/vulkan/VulkanTextureInternal.h"

#include <span>

namespace Concord {

bool CreateVulkanTexture(const VulkanContext& context, const ImageAsset& image,
                         VulkanTexture& texture)
{
    return CreateVulkanTexture(context, image, VK_NULL_HANDLE, VK_NULL_HANDLE, texture);
}

bool CreateVulkanTexture(const VulkanContext& context, const ImageAsset& image,
                         VkDescriptorSetLayout descriptorLayout,
                         VkDescriptorPool descriptorPool, VulkanTexture& texture)
{
    DestroyVulkanTexture(context, texture);
    if (!image.IsValid() || image.pixels.empty()) return false;
    const auto bytes = std::as_bytes(std::span<const u8>(image.pixels));
    if (!CreateVulkanHostBuffer(context, bytes.size_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                texture.staging) ||
        !UploadVulkanBuffer(texture.staging, bytes) ||
        !CreateVulkanTextureImage(context, image, texture) ||
        !CreateVulkanTextureDescriptors(context, texture, descriptorLayout, descriptorPool,
                                        descriptorLayout == VK_NULL_HANDLE)) {
        DestroyVulkanTexture(context, texture);
        return false;
    }
    return texture.IsReady();
}

void ReleaseVulkanTextureStaging(const VulkanContext& context,
                                 VulkanTexture& texture) noexcept
{
    DestroyVulkanBuffer(context, texture.staging);
}

void DestroyVulkanTexture(const VulkanContext& context, VulkanTexture& texture) noexcept
{
    const VkDevice device = texture.device != VK_NULL_HANDLE ? texture.device : context.device;
    if (device != VK_NULL_HANDLE && texture.ownsDescriptorObjects &&
        texture.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, texture.descriptorPool, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.ownsDescriptorObjects &&
        texture.descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, texture.descriptorLayout, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, texture.sampler, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texture.view, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, texture.image, nullptr);
    }
    if (device != VK_NULL_HANDLE && texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, texture.memory, nullptr);
    }
    DestroyVulkanBuffer(context, texture.staging);
    texture = {};
}

} // namespace Concord
