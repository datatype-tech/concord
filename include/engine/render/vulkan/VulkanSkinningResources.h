// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSKINNINGRESOURCES_H
#define CONCORD_VULKANSKINNINGRESOURCES_H

#include "engine/asset/SkinningPalette.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Per-frame storage buffers and descriptors for GPU skinning matrices. */
struct VulkanSkinningResources {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VulkanBuffer buffers[kMaxFramesInFlight]{};
    VkDescriptorSet sets[kMaxFramesInFlight]{};

    /** Whether every frame slot has a mapped palette buffer and descriptor. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        if (layout == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) return false;
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            if (!buffers[i].IsReady() || sets[i] == VK_NULL_HANDLE) return false;
        }
        return true;
    }
};

/** Allocates the per-frame skinning palette buffers and descriptor sets. */
bool CreateVulkanSkinningResources(const VulkanContext& context,
                                   VulkanSkinningResources& resources);

/** Uploads one frame's concatenated joint matrices. */
bool UploadVulkanSkinningPalette(VulkanSkinningResources& resources, u32 frameIndex,
                                 const SkinningPaletteUpload& palette) noexcept;

/** Binds one frame's palette descriptor at set one of a graphics pipeline. */
bool BindVulkanSkinningPalette(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                               const VulkanSkinningResources& resources,
                               u32 frameIndex) noexcept;

/** Makes the current frame palette visible to vertex shader reads. */
void InsertVulkanSkinningPaletteBarrier(VkCommandBuffer commandBuffer,
                                         const VulkanSkinningResources& resources,
                                         u32 frameIndex) noexcept;

/** Releases all palette buffers and descriptor objects. */
void DestroyVulkanSkinningResources(const VulkanContext& context,
                                    VulkanSkinningResources& resources) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSKINNINGRESOURCES_H
