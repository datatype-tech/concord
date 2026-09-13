// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <array>
#include <span>

namespace Concord {

namespace {

bool AppendInstance(std::array<VkAccelerationStructureInstanceKHR,
                                kVulkanRayTracingMaxInstances>& instances,
                    u32& count, const Mat4& model, VkDeviceAddress address,
                    u32 customIndex, bool blocksLight = true) noexcept
{
    if (address == 0 || count >= kVulkanRayTracingMaxInstances) return false;
    VkAccelerationStructureInstanceKHR& instance = instances[count];
    for (u32 row = 0; row < 3; ++row) {
        for (u32 column = 0; column < 4; ++column) {
            instance.transform.matrix[row][column] = model.col[column][row];
        }
    }
    instance.instanceCustomIndex = customIndex;
    instance.mask = kVulkanRayTracingMaskVisible |
                    (blocksLight ? kVulkanRayTracingMaskLightBlocker : 0u);
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                     VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    instance.accelerationStructureReference = address;
    ++count;
    return true;
}

/** Whether this skinned BLAS entry belongs to the entity being placed. */
bool SkinnedPrimitiveBelongsTo(const VulkanRayTracingScene& scene, u32 primitiveIndex,
                               Entity entity) noexcept
{
    for (const VulkanRayTracingSkinnedSource& source : scene.skinnedSources) {
        if (source.modelPrimitiveIndex == primitiveIndex && source.entity == entity) {
            return true;
        }
    }
    return false;
}

void AppendModelInstances(
    const VulkanRayTracingScene& scene, const RenderObjectSnapshot& object,
    std::array<VkAccelerationStructureInstanceKHR, kVulkanRayTracingMaxInstances>& instances,
    u32& count) noexcept
{
    if (object.shape != PrimitiveShape::Model || !object.modelAsset) return;
    // A skinned draw shares its rest mesh with every other instance of the
    // same asset, so its BLAS is keyed by entity instead of by asset.
    const bool skinned = object.modelSkin >= 0 || object.skinningRange.jointCount != 0;
    for (u32 index = 0; index < scene.modelPrimitives.size(); ++index) {
        const VulkanRayTracingModelPrimitive& primitive = scene.modelPrimitives[index];
        if (!primitive.IsReady() || primitive.skinned != skinned) continue;
        if (skinned) {
            if (!SkinnedPrimitiveBelongsTo(scene, index, object.entity)) continue;
        } else if (primitive.source != object.modelAsset.get()) {
            continue;
        }
        if (object.modelMesh != kAllModelMeshes && primitive.meshIndex != object.modelMesh) {
            continue;
        }
        if (!AppendInstance(instances, count, object.model, primitive.address,
                            kVulkanRayTracingModelInstanceBit | primitive.metadataIndex,
                            object.castShadow)) {
            return;
        }
    }
}

} // namespace

/** Converts a model list into bounded TLAS instances and uploads them. */
u32 UploadVulkanRayTracingInstances(VulkanRayTracingScene& scene,
                                    const RenderSceneSnapshot* snapshot) noexcept
{
    std::array<VkAccelerationStructureInstanceKHR, kVulkanRayTracingMaxInstances> instances{};
    // One authored material per instance slot. A Box carries its appearance in
    // the hit shader through this array rather than through a baked-in palette:
    // the palette made every authored colour in the scene render as one of
    // eight fixed hues, which is what turned a grey floor into near-black and
    // left the water with nothing to show through it.
    std::array<VulkanBoxMaterial, kVulkanRayTracingMaxInstances> boxMaterials{};
    u32 count = 0;
    u32 boxCount = 0;
    if (!snapshot) {
        count = 1;
        instances[0].transform.matrix[0][0] = 1.0f;
        instances[0].transform.matrix[1][1] = 1.0f;
        instances[0].transform.matrix[2][2] = 1.0f;
        instances[0].mask = kVulkanRayTracingMaskVisible | kVulkanRayTracingMaskLightBlocker;
        instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                             VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
        instances[0].accelerationStructureReference = scene.bottomLevelAddress;
    } else {
        for (const RenderObjectSnapshot& object : snapshot->objects) {
            if (!scene.includeNonShadowCasters && !object.castShadow) continue;
            if (object.shape == PrimitiveShape::Box) {
                // The slot is the instance index, so the custom index needs no
                // second lookup table and both stay valid together.
                if (count < kVulkanRayTracingMaxInstances) {
                    boxMaterials[count] = MakeVulkanBoxMaterial(object.material);
                }
                AppendInstance(instances, count, object.model, scene.bottomLevelAddress,
                               kVulkanRayTracingBoxMaterialBit | count, object.castShadow);
                ++boxCount;
            } else if (object.shape == PrimitiveShape::Model) {
                AppendModelInstances(scene, object, instances, count);
            }
        }
    }
    if (count == 0) {
        return 0;
    }
    if (boxCount != 0 &&
        !UploadVulkanBuffer(scene.boxMaterialBuffer,
                            std::as_bytes(std::span(boxMaterials.data(), count)))) {
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
