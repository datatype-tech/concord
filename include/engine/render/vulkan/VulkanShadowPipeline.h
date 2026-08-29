// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWPIPELINE_H
#define CONCORD_VULKANSHADOWPIPELINE_H

#include "engine/core/Mat4.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <vulkan/vulkan.h>

namespace Concord {

struct VulkanModelAssetCache;

/** Optional depth-only pipeline for a single directional shadow map. */
struct VulkanShadowPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline depth = VK_NULL_HANDLE;
    VkPipeline modelDepth = VK_NULL_HANDLE;
    VkPipelineLayout modelLayout = VK_NULL_HANDLE;

    /** Whether the shadow pass can be recorded. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return layout != VK_NULL_HANDLE && depth != VK_NULL_HANDLE;
    }

    /** Whether the optional indexed model shadow pipeline is available. */
    [[nodiscard]] bool HasModel() const noexcept
    {
        return modelLayout != VK_NULL_HANDLE && modelDepth != VK_NULL_HANDLE;
    }
};

/** Loads the bundled directional-shadow vertex shader and creates its pipeline. */
bool CreateVulkanShadowPipeline(const VulkanContext& context, VkFormat depthFormat,
                               VulkanShadowPipeline& pipeline);

/** Releases the optional directional-shadow pipeline. */
void DestroyVulkanShadowPipeline(const VulkanContext& context,
                                 VulkanShadowPipeline& pipeline) noexcept;

/** Records a depth-only pass for shadow-casting built-in boxes. */
void RecordVulkanShadowPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                            VkImageView depthView, const VulkanShadowPipeline& pipeline,
                            const RenderSceneSnapshot& snapshot,
                            const Mat4& lightViewProjection,
                            const VulkanModelAssetCache* modelAssets = nullptr) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSHADOWPIPELINE_H
