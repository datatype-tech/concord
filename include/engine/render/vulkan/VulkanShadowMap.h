// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWMAP_H
#define CONCORD_VULKANSHADOWMAP_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Default square resolution used by the optional directional shadow map. */
inline constexpr u32 kDirectionalShadowMapSize = 2048;

/** Binding used inside the shadow map's standalone sampled-image set. */
inline constexpr u32 kDirectionalShadowMapBinding = 0;

/** Descriptor-set slot reserved for shadow lookups in a forward pipeline. */
inline constexpr u32 kDirectionalShadowMapSet = 1;

/** One frame slot's depth image and sampling objects for a directional light.
 *
 * Backends with multiple frames in flight own one instance per frame slot so
 * a new shadow pass never overwrites an image still sampled by older work.
 */
struct VulkanShadowMap {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    /** Whether the image can be rendered to and sampled by a future pass. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return image != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && device != VK_NULL_HANDLE &&
               view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE &&
               descriptorLayout != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE &&
               format != VK_FORMAT_UNDEFINED && extent.width != 0 && extent.height != 0;
    }
};

/** Creates an optional sampled depth image and its descriptor set. */
bool CreateVulkanShadowMap(const VulkanContext& context, VkExtent2D extent,
                           VulkanShadowMap& shadowMap);

/** Releases all image, sampler, and descriptor objects owned by the map. */
void DestroyVulkanShadowMap(const VulkanContext& context,
                            VulkanShadowMap& shadowMap) noexcept;

/** Transitions the map into the layout required by a depth-only pass. */
void TransitionVulkanShadowMapToDepth(VkCommandBuffer commandBuffer,
                                      VulkanShadowMap& shadowMap) noexcept;

/** Transitions the map into the layout required by a sampled shadow lookup. */
void TransitionVulkanShadowMapToRead(VkCommandBuffer commandBuffer,
                                     VulkanShadowMap& shadowMap) noexcept;

/** Invalidates CPU layout tracking after a command buffer is abandoned. */
void InvalidateVulkanShadowMapLayouts(VulkanShadowMap* shadowMaps, u32 count) noexcept;

/** Binds the optional shadow set at the reserved forward-pipeline slot. */
bool BindVulkanShadowMap(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                         const VulkanShadowMap& shadowMap) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSHADOWMAP_H
