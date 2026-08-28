// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"

namespace Concord {

bool CreateVulkanRayTracingSceneRing(const VulkanContext& context,
                                     VulkanRayTracingSceneRing& ring)
{
    DestroyVulkanRayTracingSceneRing(context, ring);
    for (VulkanRayTracingScene& scene : ring.scenes) {
        if (!CreateVulkanRayTracingScene(context, scene)) {
            DestroyVulkanRayTracingSceneRing(context, ring);
            return false;
        }
    }
    return true;
}

void DestroyVulkanRayTracingSceneRing(const VulkanContext& context,
                                      VulkanRayTracingSceneRing& ring) noexcept
{
    for (VulkanRayTracingScene& scene : ring.scenes) {
        DestroyVulkanRayTracingScene(context, scene);
    }
}

} // namespace Concord
