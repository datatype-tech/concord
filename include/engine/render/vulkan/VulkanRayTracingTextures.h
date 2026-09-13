// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGTEXTURES_H
#define CONCORD_VULKANRAYTRACINGTEXTURES_H

#include "engine/render/RayTracingTextureSlots.h"
#include "engine/render/vulkan/VulkanTextureCache.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Descriptor set the closest-hit shader reads the sampler array from. */
inline constexpr u32 kVulkanRayTracingTextureSet = 3;

/**
 * The fixed sampler array the ray tracing closest-hit stage indexes.
 *
 * One descriptor set holding every slot, rather than one set per material:
 * a ray can land on any primitive, so the shader has no per-draw binding
 * point to switch textures at. Unused slots are filled with the cache's
 * white texture, so every index a shader can compute is bound to something.
 */
struct VulkanRayTracingTextures {
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;

    /** Whether the layout, pool and set are all usable. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return device != VK_NULL_HANDLE && layout != VK_NULL_HANDLE &&
               pool != VK_NULL_HANDLE && set != VK_NULL_HANDLE;
    }
};

/** Creates the sampler-array layout, its pool and the single set. */
bool CreateVulkanRayTracingTextures(const VulkanContext& context,
                                    VulkanRayTracingTextures& textures);

/** Releases every object this binding owns. */
void DestroyVulkanRayTracingTextures(const VulkanContext& context,
                                     VulkanRayTracingTextures& textures) noexcept;

/**
 * Points each slot at its cached texture.
 *
 * A slot whose key is unregistered or no longer resident falls back to the
 * white texture, so the array never holds a hole a ray could index into.
 *
 * @return false when the binding or the cache is unusable.
 */
bool UpdateVulkanRayTracingTextures(const VulkanContext& context,
                                    VulkanRayTracingTextures& textures,
                                    const VulkanTextureCache& cache,
                                    const RayTracingTextureSlots& slots) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGTEXTURES_H
