// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSKYPIPELINE_H
#define CONCORD_VULKANSKYPIPELINE_H

#include "engine/render/vulkan/VulkanContext.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Fullscreen equirectangular sky drawn behind raster geometry. */
struct VulkanSkyPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    /** Whether a sky pass can be recorded. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return layout != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE;
    }
};

/**
 * Creates the sky pipeline for one color format.
 *
 * @param frameDataLayout Per-frame camera block, bound as set 0.
 * @param textureLayout Single combined sampler, bound as set 1.
 */
bool CreateVulkanSkyPipeline(const VulkanContext& context, VkFormat colorFormat,
                             VkDescriptorSetLayout frameDataLayout,
                             VkDescriptorSetLayout textureLayout, VulkanSkyPipeline& pipeline);

/** Releases the pipeline and its layout. */
void DestroyVulkanSkyPipeline(const VulkanContext& context, VulkanSkyPipeline& pipeline) noexcept;

/**
 * Clears the color target with the sky image.
 *
 * Depth is left untouched: the pre-pass already owns it, and a scene with no
 * geometry clears depth before this pass.
 */
bool RecordVulkanSkyPass(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImageView colorView,
                         const VulkanSkyPipeline& pipeline, VkDescriptorSet frameDataSet,
                         VkDescriptorSet skyboxSet) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSKYPIPELINE_H
