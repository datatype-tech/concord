// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGPIPELINE_H
#define CONCORD_VULKANRAYTRACINGPIPELINE_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanRayTracingSupport.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** One aligned region in a ray-tracing shader binding table allocation. */
struct VulkanRayTracingSbtRegion {
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
    VkDeviceSize stride = 0;

    /** Whether the region contains at least one shader-group record. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return size != 0 && stride != 0 && size >= stride;
    }
};

/** A validated, allocation-relative SBT layout for one ray-tracing pipeline. */
struct VulkanRayTracingSbtLayout {
    VulkanRayTracingSbtRegion raygen{};
    VulkanRayTracingSbtRegion miss{};
    VulkanRayTracingSbtRegion hit{};
    VkDeviceSize totalSize = 0;
    VkDeviceSize baseAlignment = 0;

    /** Whether all required regions and the complete allocation are valid. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return raygen.IsReady() && miss.IsReady() && hit.IsReady() && totalSize != 0 &&
               baseAlignment != 0;
    }
};

/** Computes overflow-safe offsets for raygen, miss, and hit SBT records. */
[[nodiscard]] VulkanRayTracingSbtLayout BuildVulkanRayTracingSbtLayout(
    const VulkanRayTracingSupport& support, u32 raygenRecordCount = 1,
    u32 missRecordCount = 1, u32 hitRecordCount = 1) noexcept;

/** Converts an allocation base address and layout into Vulkan trace regions. */
bool BuildVulkanRayTracingSbtRegions(
    VkDeviceAddress baseAddress, const VulkanRayTracingSbtLayout& layout,
    VkStridedDeviceAddressRegionKHR& raygen, VkStridedDeviceAddressRegionKHR& miss,
    VkStridedDeviceAddressRegionKHR& hit) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGPIPELINE_H
