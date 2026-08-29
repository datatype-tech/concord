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
