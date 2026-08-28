// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSUPPORT_H
#define CONCORD_VULKANRAYTRACINGSUPPORT_H

#include "engine/core/Types.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Optional Vulkan ray-tracing capabilities exposed by one physical device. */
struct VulkanRayTracingSupport {
    bool supported = false;
    bool bufferDeviceAddress = false;
    bool accelerationStructure = false;
    bool rayTracingPipeline = false;
    bool deferredHostOperations = false;
    bool rayQuery = false;
    u32 shaderGroupHandleSize = 0;
    u32 shaderGroupHandleAlignment = 0;
    u32 shaderGroupBaseAlignment = 0;
    u32 maxShaderGroupStride = 0;
    u32 maxRayRecursionDepth = 0;
    u32 maxRayHitAttributeSize = 0;

    /** Whether the required hardware pipeline path is ready to enable. */
    [[nodiscard]] bool IsUsable() const noexcept
    {
        return supported && bufferDeviceAddress && accelerationStructure &&
               rayTracingPipeline && deferredHostOperations && shaderGroupHandleSize != 0 &&
               shaderGroupHandleAlignment != 0 && shaderGroupBaseAlignment != 0 &&
               maxShaderGroupStride != 0 && maxRayRecursionDepth != 0;
    }

    /** Whether the independent ray-query path has its required features. */
    [[nodiscard]] bool IsRayQueryUsable() const noexcept
    {
        return bufferDeviceAddress && accelerationStructure && deferredHostOperations && rayQuery;
    }
};

/** Queries extension, feature, and shader-binding-table limits safely. */
[[nodiscard]] VulkanRayTracingSupport QueryVulkanRayTracingSupport(
    VkPhysicalDevice device) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGSUPPORT_H
