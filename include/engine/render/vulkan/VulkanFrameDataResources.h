// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMEDATARESOURCES_H
#define CONCORD_VULKANFRAMEDATARESOURCES_H

#include "engine/render/RenderFrameData.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"
#include "engine/render/vulkan/VulkanTileLightLimits.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Per-frame uniform buffers and descriptor objects for scene data.
 *
 * The descriptor set always has a valid binding 1. When full tile storage
 * cannot be allocated, creation installs a tiny fallback buffer and the
 * renderer disables the tile flag, so the fragment shader takes its all-light
 * path while the frame UBO remains usable.
 */
struct VulkanFrameDataResources {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VulkanBuffer buffers[kMaxFramesInFlight]{};
    VulkanBuffer tileBuffers[kMaxFramesInFlight]{};
    VkDescriptorSet sets[kMaxFramesInFlight]{};

    /** Whether the frame UBOs and descriptor sets are usable. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        if (layout == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
            return false;
        }
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            if (!buffers[i].IsReady() || sets[i] == VK_NULL_HANDLE) {
                return false;
            }
        }
        return true;
    }

    /** Whether every frame slot owns the full fixed-size tile list buffer. */
    [[nodiscard]] bool IsTileReady() const noexcept
    {
        if (!IsReady()) {
            return false;
        }
        for (const VulkanBuffer& buffer : tileBuffers) {
            if (!buffer.IsReady() || buffer.size < TileLightBufferBytes()) {
                return false;
            }
        }
        return true;
    }
};

/** Creates host-visible frame buffers, a descriptor layout, and descriptor sets. */
bool CreateVulkanFrameDataResources(const VulkanContext& context,
                                    VulkanFrameDataResources& resources);

/** Uploads the current frame packet into one frame slot. */
bool UploadVulkanFrameData(VulkanFrameDataResources& resources, u32 frameIndex,
                           const RenderFrameData& data) noexcept;

/** Binds one frame slot's descriptor set to a graphics command buffer. */
bool BindVulkanFrameData(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                         const VulkanFrameDataResources& resources,
                         u32 frameIndex) noexcept;

/** Releases all frame buffers and descriptor objects. */
void DestroyVulkanFrameDataResources(const VulkanContext& context,
                                     VulkanFrameDataResources& resources) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANFRAMEDATARESOURCES_H
