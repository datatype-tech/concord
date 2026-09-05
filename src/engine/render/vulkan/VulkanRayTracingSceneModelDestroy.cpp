// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

namespace Concord {

void DestroyVulkanRayTracingModelPrimitives(const VulkanContext& context,
                                            VulkanRayTracingScene& scene) noexcept
{
    const VkDevice device = scene.device != VK_NULL_HANDLE ? scene.device : context.device;
    for (VulkanRayTracingModelPrimitive& primitive : scene.modelPrimitives) {
        if (device != VK_NULL_HANDLE && primitive.accelerationStructure != VK_NULL_HANDLE &&
            scene.dispatch.destroyAccelerationStructure != nullptr) {
            scene.dispatch.destroyAccelerationStructure(device, primitive.accelerationStructure,
                                                        nullptr);
        }
        DestroyVulkanBuffer(context, primitive.scratch);
        DestroyVulkanBuffer(context, primitive.storage);
        primitive = {};
    }
    scene.modelPrimitives.clear();
    DestroyVulkanBuffer(context, scene.modelVertexBuffer);
    DestroyVulkanBuffer(context, scene.modelIndexBuffer);
    scene.modelPrimitiveInfos.clear();
    scene.modelVertices.clear();
    scene.modelIndices.clear();
    if (scene.descriptorSet != VK_NULL_HANDLE) {
        UpdateVulkanRayTracingSceneModelDescriptors(context, scene);
    }
}

} // namespace Concord
