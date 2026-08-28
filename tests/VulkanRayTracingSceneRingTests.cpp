// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneRing.h"

int main()
{
    Concord::VulkanRayTracingSceneRing ring{};
    if (ring.IsReady()) {
        return 1;
    }
    for (Concord::u32 i = 0; i < Concord::kMaxFramesInFlight; ++i) {
        if (&ring.At(i) != &ring.scenes[i]) {
            return 1;
        }
    }
    Concord::VulkanContext context{};
    if (Concord::CreateVulkanRayTracingSceneRing(context, ring) || ring.IsReady()) {
        return 1;
    }
    return 0;
}
