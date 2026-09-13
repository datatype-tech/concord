// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <limits>
#include <span>

namespace Concord {
namespace {

template <typename T>
bool CreateModelBuffer(const VulkanContext& context, const std::vector<T>& values,
                       VkBufferUsageFlags usage, bool deviceAddress, VulkanBuffer& output)
{
    if (values.empty() || values.size() > std::numeric_limits<VkDeviceSize>::max() / sizeof(T)) {
        return false;
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(values.size() * sizeof(T));
    return CreateVulkanHostBuffer(context, size, usage, output, deviceAddress) &&
           UploadVulkanBuffer(output, std::as_bytes(std::span<const T>(values)));
}

/**
 * Usage the skinned geometry path needs on top of plain storage.
 *
 * A skinned primitive builds its BLAS straight out of the vertex SSBO the
 * hit shader reads, so the same allocation has to be both shader-visible and
 * a legal acceleration-structure build input with a device address.
 */
constexpr VkBufferUsageFlags kModelGeometryUsage =
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

} // namespace

bool RebuildVulkanRayTracingModelBuffers(const VulkanContext& context,
                                         VulkanRayTracingScene& scene)
{
    VulkanBuffer vertex{};
    VulkanBuffer index{};
    if (!scene.modelVertices.empty() &&
        !CreateModelBuffer(context, scene.modelVertices, kModelGeometryUsage, true, vertex)) {
        DestroyVulkanBuffer(context, vertex);
        return false;
    }
    if (!scene.modelIndices.empty() &&
        !CreateModelBuffer(context, scene.modelIndices, kModelGeometryUsage, true, index)) {
        DestroyVulkanBuffer(context, vertex);
        DestroyVulkanBuffer(context, index);
        return false;
    }
    if (!scene.modelPrimitiveBuffer.IsReady() ||
        scene.modelPrimitiveInfos.size() > kVulkanRayTracingModelMetadataCapacity ||
        !UploadVulkanBuffer(scene.modelPrimitiveBuffer,
                            std::as_bytes(std::span<const VulkanRayTracingModelPrimitiveInfo>(
                                scene.modelPrimitiveInfos)))) {
        DestroyVulkanBuffer(context, vertex);
        DestroyVulkanBuffer(context, index);
        return false;
    }
    const VulkanBuffer oldVertex = scene.modelVertexBuffer;
    const VulkanBuffer oldIndex = scene.modelIndexBuffer;
    scene.modelVertexBuffer = vertex;
    scene.modelIndexBuffer = index;
    vertex = {};
    index = {};
    if (scene.descriptorSet != VK_NULL_HANDLE &&
        !UpdateVulkanRayTracingSceneModelDescriptors(context, scene)) {
        VulkanBuffer failedVertex = scene.modelVertexBuffer;
        VulkanBuffer failedIndex = scene.modelIndexBuffer;
        scene.modelVertexBuffer = oldVertex;
        scene.modelIndexBuffer = oldIndex;
        DestroyVulkanBuffer(context, failedVertex);
        DestroyVulkanBuffer(context, failedIndex);
        return false;
    }
    // Skinned BLAS input points into these buffers, so their addresses have
    // to follow the swap before anything builds against them.
    RefreshVulkanRayTracingSkinnedAddresses(scene);
    VulkanBuffer staleVertex = oldVertex;
    VulkanBuffer staleIndex = oldIndex;
    DestroyVulkanBuffer(context, staleVertex);
    DestroyVulkanBuffer(context, staleIndex);
    return true;
}

} // namespace Concord
