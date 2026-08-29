// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTEXTUREINTERNAL_H
#define CONCORD_VULKANTEXTUREINTERNAL_H

#include "engine/render/vulkan/VulkanTexture.h"

namespace Concord {

/** Creates and binds the device-local optimal-tiled image. */
bool CreateVulkanTextureImage(const VulkanContext& context, const ImageAsset& source,
                              VulkanTexture& texture);

/** Creates the sampled-image view, sampler and descriptor set. */
bool CreateVulkanTextureDescriptors(const VulkanContext& context, VulkanTexture& texture,
                                    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE,
                                    VkDescriptorPool descriptorPool = VK_NULL_HANDLE,
                                    bool ownsDescriptorObjects = true);

/** Inserts a transfer-destination image barrier. */
void TransitionVulkanTextureToTransfer(VkCommandBuffer commandBuffer,
                                       VulkanTexture& texture) noexcept;

/** Inserts a shader-read image barrier. */
void TransitionVulkanTextureToShaderRead(VkCommandBuffer commandBuffer,
                                         VulkanTexture& texture,
                                         VkPipelineStageFlags readStages) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANTEXTUREINTERNAL_H
