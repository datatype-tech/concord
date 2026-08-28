// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENERING_H
#define CONCORD_VULKANRAYTRACINGSCENERING_H

#include "engine/render/vulkan/VulkanFrameLimits.h"
#include "engine/render/vulkan/VulkanRayTracingScene.h"

namespace Concord {

/** Owns one independent acceleration-structure scene for every frame slot. */
struct VulkanRayTracingSceneRing {
    VulkanRayTracingScene scenes[kMaxFramesInFlight]{};

    /** Whether every frame slot has a complete descriptor-backed scene. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        for (const VulkanRayTracingScene& scene : scenes) {
            if (!scene.IsReady()) {
                return false;
            }
        }
        return true;
    }

    /** Returns the scene whose resources are protected by one frame fence. */
    [[nodiscard]] VulkanRayTracingScene& At(u32 frameIndex) noexcept
    {
        return scenes[frameIndex];
    }

    /** Returns the const scene whose resources belong to one frame slot. */
    [[nodiscard]] const VulkanRayTracingScene& At(u32 frameIndex) const noexcept
    {
        return scenes[frameIndex];
    }
};

/** Creates independent static geometry and AS resources for each frame slot. */
bool CreateVulkanRayTracingSceneRing(const VulkanContext& context,
                                     VulkanRayTracingSceneRing& ring);

/** Releases every per-frame scene after submitted work has stopped. */
void DestroyVulkanRayTracingSceneRing(const VulkanContext& context,
                                      VulkanRayTracingSceneRing& ring) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGSCENERING_H
