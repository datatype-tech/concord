// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/core/Vec3.h"

#include <array>
#include <span>

namespace Concord {
namespace {

constexpr std::array<Vec3, kVulkanRayTracingBoxVertexCount> kBoxVertices = {
    Vec3{-0.5f, -0.5f, -0.5f}, Vec3{0.5f, -0.5f, -0.5f},
    Vec3{0.5f, 0.5f, -0.5f},   Vec3{-0.5f, 0.5f, -0.5f},
    Vec3{-0.5f, -0.5f, 0.5f},  Vec3{0.5f, -0.5f, 0.5f},
    Vec3{0.5f, 0.5f, 0.5f},    Vec3{-0.5f, 0.5f, 0.5f}};

constexpr std::array<u32, kVulkanRayTracingBoxIndexCount> kBoxIndices = {
    0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5,
    4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4};

template <typename T, std::size_t N>
bool UploadArray(const VulkanContext& context, const std::array<T, N>& data,
                 VulkanBuffer& buffer)
{
    VulkanBufferCreateInfo info{};
    info.size = sizeof(data);
    info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    info.persistentMap = true;
    info.deviceAddress = true;
    if (!CreateVulkanBuffer(context, info, buffer)) {
        return false;
    }
    return UploadVulkanBuffer(buffer, std::as_bytes(std::span(data)));
}

} // namespace

bool CreateVulkanRayTracingSceneGeometry(const VulkanContext& context,
                                         VulkanRayTracingScene& scene)
{
    if (!UploadArray(context, kBoxVertices, scene.vertexBuffer)) {
        return false;
    }
    if (!UploadArray(context, kBoxIndices, scene.indexBuffer)) {
        DestroyVulkanBuffer(context, scene.vertexBuffer);
        return false;
    }
    return true;
}

} // namespace Concord
