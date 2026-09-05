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
                       VulkanBuffer& output)
{
    if (values.empty() || values.size() > std::numeric_limits<VkDeviceSize>::max() / sizeof(T)) {
        return false;
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(values.size() * sizeof(T));
    return CreateVulkanHostBuffer(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, output) &&
           UploadVulkanBuffer(output, std::as_bytes(std::span<const T>(values)));
}

} // namespace

bool RebuildVulkanRayTracingModelBuffers(const VulkanContext& context,
                                         VulkanRayTracingScene& scene)
{
    VulkanBuffer vertex{};
    VulkanBuffer index{};
    if (!scene.modelVertices.empty() &&
        !CreateModelBuffer(context, scene.modelVertices, vertex)) {
        DestroyVulkanBuffer(context, vertex);
        return false;
    }
    if (!scene.modelIndices.empty() && !CreateModelBuffer(context, scene.modelIndices, index)) {
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
    VulkanBuffer staleVertex = oldVertex;
    VulkanBuffer staleIndex = oldIndex;
    DestroyVulkanBuffer(context, staleVertex);
    DestroyVulkanBuffer(context, staleIndex);
    return true;
}

} // namespace Concord
