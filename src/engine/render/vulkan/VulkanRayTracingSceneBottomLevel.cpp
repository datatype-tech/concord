// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/core/Vec3.h"
#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {
namespace {

/** Describes the indexed unit Box triangles used by the BLAS. */
VkAccelerationStructureGeometryKHR MakeTriangles(const VulkanRayTracingScene& scene) noexcept
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = scene.vertexBuffer.GetDeviceAddress();
    triangles.vertexStride = sizeof(Vec3);
    triangles.maxVertex = kVulkanRayTracingBoxVertexCount - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = scene.indexBuffer.GetDeviceAddress();

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    return geometry;
}

/** Allocates device-local storage backing one acceleration structure. */
bool CreateStorage(const VulkanContext& context, VkDeviceSize size, VulkanBuffer& storage)
{
    VulkanBufferCreateInfo info{};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.deviceAddress = true;
    return CreateVulkanBuffer(context, info, storage);
}

} // namespace

bool CreateVulkanRayTracingSceneBottomLevel(const VulkanContext& context,
                                           VulkanRayTracingScene& scene)
{
    if (!scene.dispatch.IsReady() || !scene.vertexBuffer.HasDeviceAddress() ||
        !scene.indexBuffer.HasDeviceAddress() ||
        scene.vertexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment != 0 ||
        scene.indexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment != 0) {
        return false;
    }
    const VkAccelerationStructureGeometryKHR geometry = MakeTriangles(scene);
    VkAccelerationStructureBuildGeometryInfoKHR build{};
    build.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    const u32 primitiveCount = kVulkanRayTracingBoxPrimitiveCount;
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    scene.dispatch.getBuildSizes(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                  &build, &primitiveCount, &sizes);
    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0 ||
        !CreateStorage(context, sizes.accelerationStructureSize, scene.bottomLevelBuffer)) {
        return false;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = scene.bottomLevelBuffer.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    const VkResult result = scene.dispatch.createAccelerationStructure(
        context.device, &createInfo, nullptr, &scene.bottomLevel);
    if (result != VK_SUCCESS) {
        DestroyVulkanBuffer(context, scene.bottomLevelBuffer);
        return VulkanFailed("vkCreateAccelerationStructureKHR(BLAS)", result);
    }

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = scene.bottomLevel;
    scene.bottomLevelAddress = scene.dispatch.getDeviceAddress(context.device, &addressInfo);
    if (scene.bottomLevelAddress == 0) {
        scene.dispatch.destroyAccelerationStructure(context.device, scene.bottomLevel, nullptr);
        scene.bottomLevel = VK_NULL_HANDLE;
        DestroyVulkanBuffer(context, scene.bottomLevelBuffer);
        return false;
    }
    scene.bottomLevelScratchSize = sizes.buildScratchSize;
    return true;
}

} // namespace Concord
