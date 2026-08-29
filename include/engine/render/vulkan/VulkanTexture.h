// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTEXTURE_H
#define CONCORD_VULKANTEXTURE_H

#include "engine/asset/ImageAsset.h"
#include "engine/render/vulkan/VulkanBuffer.h"

#include <vulkan/vulkan.h>

namespace Concord {

inline constexpr VkFormat kVulkanTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;
inline constexpr u32 kVulkanTextureBinding = 0;

/** Owns one sampled 2D image, its upload staging buffer and descriptor set. */
struct VulkanTexture {
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VulkanBuffer staging{};
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool uploadRecorded = false;
    bool uploadSubmitted = false;
    bool ownsDescriptorObjects = true;

    /** Whether the descriptor-backed image can be sampled. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return device != VK_NULL_HANDLE && image != VK_NULL_HANDLE &&
               memory != VK_NULL_HANDLE && view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE &&
               descriptorSet != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0 &&
               format != VK_FORMAT_UNDEFINED;
    }

    /** Whether an upload barrier and copy have been recorded for this image. */
    [[nodiscard]] bool IsUploaded() const noexcept
    {
        return IsReady() && uploadRecorded && layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    /** Whether the upload command has completed submission and staging may be released. */
    [[nodiscard]] bool IsSubmitted() const noexcept { return uploadSubmitted; }
};

/** Allocates an image and staging storage for decoded RGBA8 pixels. */
bool CreateVulkanTexture(const VulkanContext& context, const ImageAsset& image,
                         VulkanTexture& texture);

/** Creates a texture whose descriptor set comes from a caller-owned shared pool. */
bool CreateVulkanTexture(const VulkanContext& context, const ImageAsset& image,
                         VkDescriptorSetLayout descriptorLayout,
                         VkDescriptorPool descriptorPool, VulkanTexture& texture);

/** Records the one-time staging copy and transitions the image to shader-read layout. */
bool RecordVulkanTextureUpload(VkCommandBuffer commandBuffer, VulkanTexture& texture,
                               VkPipelineStageFlags readStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) noexcept;

/** Releases staging storage after the submission containing the copy has completed. */
void ReleaseVulkanTextureStaging(const VulkanContext& context,
                                 VulkanTexture& texture) noexcept;

/** Releases image, sampler, descriptor and staging resources. */
void DestroyVulkanTexture(const VulkanContext& context, VulkanTexture& texture) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANTEXTURE_H
