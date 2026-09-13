// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPOSTPROCESS_H
#define CONCORD_VULKANPOSTPROCESS_H

#include "engine/core/Vec4.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"

namespace Concord {

/**
 * Push-constant block mirroring `post.comp`'s declaration.
 *
 * The values ride in push constants rather than in the frame block because
 * this is the only stage that reads them, and because they are authored
 * appearance rather than scene state. Every one of them is a control rather
 * than a constant baked into the shader, so the look can be changed without
 * recompiling anything.
 */
struct VulkanPostProcessConstants {
    /** x exposure, y contrast, z saturation, w vignette. */
    Vec4 grade{};
    /** x bloom threshold, y bloom intensity, z bloom radius in pixels, w chromatic aberration. */
    Vec4 bloom{};
    /** xy extent, zw inverse extent. */
    Vec4 resolution{};
};

static_assert(sizeof(VulkanPostProcessConstants) == 48);

/** The graded image one frame slot writes, and the set that binds it. */
struct VulkanPostProcess {
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkExtent2D extent{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    /** Whether the image, view and descriptor are ready for a dispatch. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && view != VK_NULL_HANDLE &&
               descriptorSet != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0;
    }
};

/**
 * The whole post-processing stage: one graded image per frame in flight.
 *
 * This is the mount point the ray tracing path was missing. The trace leaves
 * raw radiance, which is what a bloom gather and a tone curve both need; doing
 * either inside the ray-generation shader would either see only one pixel or
 * compress the value before it was finished being added to.
 */
struct VulkanPostProcessRing {
    VulkanPostProcess items[kMaxFramesInFlight]{};
    VkDevice device = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    /** Whether the pipeline, the pool and every frame slot are complete. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        if (device == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE ||
            setLayout == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE ||
            pipeline == VK_NULL_HANDLE || descriptorPool == VK_NULL_HANDLE) {
            return false;
        }
        for (const VulkanPostProcess& item : items) {
            if (!item.IsReady()) {
                return false;
            }
        }
        return true;
    }

    /** Returns the graded image belonging to the selected frame fence. */
    [[nodiscard]] VulkanPostProcess& At(u32 frameIndex) noexcept { return items[frameIndex]; }
};

/** Loads the compute shader, builds the pipeline and allocates one image per slot. */
bool CreateVulkanPostProcessRing(const VulkanContext& context, VkExtent2D extent,
                                 VulkanPostProcessRing& ring);

/** Releases the pipeline, the images and every descriptor object. */
void DestroyVulkanPostProcessRing(const VulkanContext& context,
                                  VulkanPostProcessRing& ring) noexcept;

/**
 * Grades one traced frame into this slot's image.
 *
 * The source is the frame's raw radiance. The graded image is left in
 * `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, ready for a blit.
 *
 * @return Whether the pass was recorded and the graded image is usable.
 */
[[nodiscard]] bool RecordVulkanPostProcess(VkCommandBuffer commandBuffer,
                                           VulkanPostProcessRing& ring, u32 frameIndex,
                                           const VulkanRayTracingOutput& source,
                                           const VulkanPostProcessConstants& constants) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANPOSTPROCESS_H
