// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <array>
#include <span>

namespace Concord {

/** Converts a model list into bounded TLAS instances and uploads them. */
u32 UploadVulkanRayTracingInstances(VulkanRayTracingScene& scene,
                                    const RenderSceneSnapshot* snapshot) noexcept
{
    std::array<VkAccelerationStructureInstanceKHR, kVulkanRayTracingMaxInstances> instances{};
    u32 count = 0;
    if (!snapshot) {
        count = 1;
        instances[0].transform.matrix[0][0] = 1.0f;
        instances[0].transform.matrix[1][1] = 1.0f;
        instances[0].transform.matrix[2][2] = 1.0f;
        instances[0].mask = 0xff;
        instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                             VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
        instances[0].accelerationStructureReference = scene.bottomLevelAddress;
    } else {
        for (const RenderObjectSnapshot& object : snapshot->objects) {
            if (object.shape != PrimitiveShape::Box ||
                (!scene.includeNonShadowCasters && !object.castShadow) ||
                count >= kVulkanRayTracingMaxInstances) {
                continue;
            }
            VkAccelerationStructureInstanceKHR& instance = instances[count];
            for (u32 row = 0; row < 3; ++row) {
                for (u32 column = 0; column < 4; ++column) {
                    instance.transform.matrix[row][column] = object.model.col[column][row];
                }
            }
            instance.instanceCustomIndex = count++;
            instance.mask = 0xff;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                             VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
            instance.accelerationStructureReference = scene.bottomLevelAddress;
        }
    }
    if (count == 0) {
        return 0;
    }
    const auto bytes = std::as_bytes(std::span<const VkAccelerationStructureInstanceKHR>(
        instances.data(), count));
    return UploadVulkanBuffer(scene.instanceBuffer, bytes) ? count : 0;
}

/** Makes host writes visible to acceleration-structure input reads. */
void InsertVulkanRayTracingInputBarrier(VkCommandBuffer commandBuffer,
                                         const VulkanRayTracingScene& scene) noexcept
{
    VkBufferMemoryBarrier barriers[3]{};
    const VulkanBuffer* buffers[] = {&scene.vertexBuffer, &scene.indexBuffer,
                                     &scene.instanceBuffer};
    for (u32 i = 0; i < 3; ++i) {
        barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[i].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].buffer = buffers[i]->buffer;
        barriers[i].size = VK_WHOLE_SIZE;
    }
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0,
                         nullptr, 3, barriers, 0, nullptr);
}

/** Makes BLAS writes available to the following TLAS build. */
void InsertVulkanRayTracingBuildBarrier(VkCommandBuffer commandBuffer) noexcept
{
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                            VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier,
                         0, nullptr, 0, nullptr);
}

} // namespace Concord
