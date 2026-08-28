// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTILELIGHTCULLING_H
#define CONCORD_VULKANTILELIGHTCULLING_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanTileLightLimits.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Optional compute pipeline that builds one bounded light list per tile. */
struct VulkanTileLightCulling {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    /** Whether the compute shader and its layout are usable. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return layout != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE;
    }
};

/** Creates the compute pipeline against the frame-data descriptor layout. */
bool CreateVulkanTileLightCulling(const VulkanContext& context,
                                  VkDescriptorSetLayout frameDataLayout,
                                  VulkanTileLightCulling& culling);

/** Releases the optional compute pipeline. */
void DestroyVulkanTileLightCulling(const VulkanContext& context,
                                   VulkanTileLightCulling& culling) noexcept;

/** Records one dispatch for the current viewport. */
void RecordVulkanTileLightCulling(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                  const VulkanTileLightCulling& culling,
                                  VkDescriptorSet frameDataSet);

/** Makes compute writes visible to the forward fragment shader. */
void InsertVulkanTileLightBarrier(VkCommandBuffer commandBuffer, VkBuffer tileBuffer);

} // namespace Concord

#endif // CONCORD_VULKANTILELIGHTCULLING_H
