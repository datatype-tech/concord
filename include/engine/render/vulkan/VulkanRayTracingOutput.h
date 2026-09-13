// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGOUTPUT_H
#define CONCORD_VULKANRAYTRACINGOUTPUT_H

#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"

namespace Concord {

inline constexpr VkFormat kVulkanRayTracingOutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

/** One storage image written by the optional ray-generation pass. */
struct VulkanRayTracingOutput {
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
        return device != VK_NULL_HANDLE && image != VK_NULL_HANDLE &&
               memory != VK_NULL_HANDLE && view != VK_NULL_HANDLE &&
               descriptorSet != VK_NULL_HANDLE && extent.width != 0 && extent.height != 0;
    }
};

/** Keeps one RT output image isolated for each frame-in-flight slot. */
struct VulkanRayTracingOutputRing {
    VulkanRayTracingOutput outputs[kMaxFramesInFlight]{};
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    /** Whether every frame slot owns a complete output image. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        if (device == VK_NULL_HANDLE || descriptorPool == VK_NULL_HANDLE) {
            return false;
        }
        for (const VulkanRayTracingOutput& output : outputs) {
            if (!output.IsReady()) {
                return false;
            }
        }
        return true;
    }

    /** Returns the output image protected by the selected frame fence. */
    [[nodiscard]] VulkanRayTracingOutput& At(u32 frameIndex) noexcept
    {
        return outputs[frameIndex];
    }
};

/**
 * Fraction of the swapchain the traced and graded images are allocated at.
 *
 * Every pixel of the traced image costs at least one traversal of the scene
 * and most of them cost several, so native resolution prices the whole frame
 * at the heaviest pixel.
 *
 * Native, after the cloud layer's march bound was cut. The 0.8 this used to be
 * bought back a third of the trace, and it charged for it twice over in edge
 * quality: anti-aliasing runs on the traced image, so every edge was resolved
 * at four fifths of display resolution and *then* stretched back up by a
 * quarter. A blit cannot put back a sample that was never taken, so what
 * reached the screen was a staircase with a soft edge on it -- which is worse
 * than either a sharp staircase or an honestly soft image.
 *
 * `CONCORD_RENDER_SCALE` still trades pixels for frame time when a title wants
 * a different bargain; it is the right lever for a machine that needs one, and
 * the wrong default for every machine that does not.
 */
inline constexpr f32 kVulkanDefaultRenderScale = 1.0f;

/**
 * Extent the traced and graded images are allocated at, for a given output.
 *
 * Never zero. A swapchain a few pixels across would otherwise ask for a traced
 * image with no pixels in it, and an image with no pixels is not a frame -- it
 * is a validation error followed by a black window.
 */
[[nodiscard]] VkExtent2D ResolveVulkanRenderExtent(VkExtent2D output) noexcept;

/**
 * The scale ResolveVulkanRenderExtent is applying.
 *
 * Read from CONCORD_RENDER_SCALE once, on first use, and cached: a scale that
 * can change between the allocation of a target and the dispatch into it is a
 * scale that produces mismatched images, and the failure mode of a mismatched
 * image is not a visible error but a wrong picture.
 */
[[nodiscard]] f32 VulkanRenderScale() noexcept;

/** Creates one storage image and descriptor set per frame slot. */
bool CreateVulkanRayTracingOutputRing(const VulkanContext& context,
                                      VkDescriptorSetLayout descriptorLayout,
                                      VkExtent2D extent,
                                      VulkanRayTracingOutputRing& ring);

/** Releases all output images, views and descriptor objects. */
void DestroyVulkanRayTracingOutputRing(const VulkanContext& context,
                                       VulkanRayTracingOutputRing& ring) noexcept;

/** Makes one output image writable by a ray-generation shader. */
void PrepareVulkanRayTracingOutput(VkCommandBuffer commandBuffer,
                                   VulkanRayTracingOutput& output) noexcept;

/**
 * Blits a completed colour image into the acquired swapchain image.
 *
 * Separate from the ray tracing output so the post-processing stage can hand
 * over its own result instead; the two differ only in which image is the
 * source.
 */
bool CompositeVulkanColorImage(const VulkanContext& context, VkCommandBuffer commandBuffer,
                               VkImage source, VkExtent2D sourceExtent,
                               VkImageLayout sourceLayout, VkImage swapchainImage,
                               VkFormat swapchainFormat, VkImageLayout swapchainLayout,
                               VkExtent2D extent) noexcept;

/** Blits one completed RT output into the acquired swapchain image. */
bool CompositeVulkanRayTracingOutput(const VulkanContext& context,
                                     VkCommandBuffer commandBuffer,
                                     VulkanRayTracingOutput& output,
                                     VkImage swapchainImage,
                                     VkFormat swapchainFormat,
                                     VkImageLayout swapchainLayout,
                                     VkExtent2D extent) noexcept;

/** Returns whether the RT output and swapchain formats support blitting. */
[[nodiscard]] bool SupportsVulkanRayTracingComposite(const VulkanContext& context,
                                                     VkFormat swapchainFormat) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGOUTPUT_H
