// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <limits>

namespace Concord {
namespace {

bool AlignSize(VkDeviceSize value, VkDeviceSize alignment, VkDeviceSize& result) noexcept
{
    if (alignment == 0) return false;
    const VkDeviceSize remainder = value % alignment;
    const VkDeviceSize padding = remainder == 0 ? 0 : alignment - remainder;
    if (padding > std::numeric_limits<VkDeviceSize>::max() - value) return false;
    result = value + padding;
    return result != 0;
}

bool CreateDeviceStorage(const VulkanContext& context, VkDeviceSize size,
                         VkBufferUsageFlags usage, VulkanBuffer& buffer)
{
    VulkanBufferCreateInfo info{};
    info.size = size;
    info.usage = usage;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.deviceAddress = true;
    return CreateVulkanBuffer(context, info, buffer);
}

/** Points a skinned primitive's BLAS input at its slice of the packed SSBOs. */
void PointAtPackedBuffers(VulkanRayTracingScene& scene,
                          VulkanRayTracingModelPrimitive& primitive) noexcept
{
    primitive.vertexBuffer = scene.modelVertexBuffer.buffer;
    primitive.indexBuffer = scene.modelIndexBuffer.buffer;
    primitive.vertexAddress =
        scene.modelVertexBuffer.GetDeviceAddress() +
        static_cast<VkDeviceAddress>(primitive.metadataFirstVertex) *
            sizeof(VulkanRayTracingModelVertex);
    primitive.indexAddress = scene.modelIndexBuffer.GetDeviceAddress() +
                             static_cast<VkDeviceAddress>(primitive.metadataFirstIndex) *
                                 sizeof(u32);
}

/** Allocates the AS storage, scratch and acceleration structure for one source. */
bool CreateSkinnedBlas(const VulkanContext& context, VulkanRayTracingScene& scene,
                       VulkanRayTracingModelPrimitive& primitive)
{
    if (scene.scratchAlignment == 0 || primitive.vertexCount == 0 || primitive.indexCount < 3) {
        return false;
    }
    PointAtPackedBuffers(scene, primitive);
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = primitive.vertexAddress;
    triangles.vertexStride = primitive.vertexStride;
    triangles.maxVertex = primitive.vertexCount - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = primitive.indexAddress;
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    VkAccelerationStructureBuildGeometryInfoKHR build{};
    build.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    const u32 primitiveCount = primitive.indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    scene.dispatch.getBuildSizes(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                  &build, &primitiveCount, &sizes);
    VkDeviceSize scratchSize = 0;
    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0 ||
        !AlignSize(sizes.buildScratchSize, scene.scratchAlignment, scratchSize) ||
        !CreateDeviceStorage(context, sizes.accelerationStructureSize,
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                             primitive.storage) ||
        !CreateDeviceStorage(context, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             primitive.scratch)) {
        return false;
    }
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = primitive.storage.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (scene.dispatch.createAccelerationStructure(context.device, &createInfo, nullptr,
                                                   &primitive.accelerationStructure) != VK_SUCCESS) {
        return false;
    }
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = primitive.accelerationStructure;
    primitive.address = scene.dispatch.getDeviceAddress(context.device, &addressInfo);
    primitive.scratchSize = scratchSize;
    return primitive.IsReady();
}

} // namespace

bool CreateVulkanRayTracingSkinnedPrimitives(const VulkanContext& context,
                                             VulkanRayTracingScene& scene)
{
    if (!scene.modelVertexBuffer.HasDeviceAddress() || !scene.modelIndexBuffer.HasDeviceAddress()) {
        return false;
    }
    for (const VulkanRayTracingSkinnedSource& source : scene.skinnedSources) {
        if (source.modelPrimitiveIndex >= scene.modelPrimitives.size()) {
            return false;
        }
        VulkanRayTracingModelPrimitive& primitive =
            scene.modelPrimitives[source.modelPrimitiveIndex];
        if (primitive.accelerationStructure != VK_NULL_HANDLE) {
            continue;
        }
        if (!CreateSkinnedBlas(context, scene, primitive)) {
            return false;
        }
    }
    return true;
}

void RefreshVulkanRayTracingSkinnedAddresses(VulkanRayTracingScene& scene) noexcept
{
    for (VulkanRayTracingModelPrimitive& primitive : scene.modelPrimitives) {
        if (primitive.skinned) {
            PointAtPackedBuffers(scene, primitive);
        }
    }
}

} // namespace Concord
