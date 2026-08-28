// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

int main()
{
    Concord::VulkanRayTracingScene scene{};
    if (scene.IsReady() || scene.dispatch.IsReady()) {
        return 1;
    }
    Concord::VulkanRayTracingDispatch dispatch{};
    if (Concord::LoadVulkanRayTracingDispatch(VK_NULL_HANDLE, dispatch) || dispatch.IsReady()) {
        return 1;
    }
    Concord::VulkanContext context{};
    if (Concord::CreateVulkanRayTracingScene(context, scene) || scene.IsReady()) {
        return 1;
    }
    if (Concord::RecordVulkanRayTracingSceneBuild(VK_NULL_HANDLE, scene)) {
        return 1;
    }
    Concord::InsertVulkanRayTracingSceneReadBarrier(VK_NULL_HANDLE);
    Concord::DestroyVulkanRayTracingScene(context, scene);
    return scene.IsReady() ? 1 : 0;
}
