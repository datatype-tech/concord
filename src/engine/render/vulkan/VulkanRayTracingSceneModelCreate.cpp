// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/asset/ModelAsset.h"

#include <limits>

namespace Concord {
namespace {
bool AddAddress(VkDeviceAddress base, VkDeviceSize offset,
                VkDeviceAddress& result) noexcept
{
    if (offset > std::numeric_limits<VkDeviceAddress>::max() - base) return false;
    result = base + offset;
    return result != 0;
}
bool AlignSize(VkDeviceSize value, VkDeviceSize alignment,
               VkDeviceSize& result) noexcept
{
    if (alignment == 0) return false;
    const VkDeviceSize remainder = value % alignment;
    const VkDeviceSize padding = remainder == 0 ? 0 : alignment - remainder;
    if (padding > std::numeric_limits<VkDeviceSize>::max() - value) return false;
    result = value + padding;
    return result != 0;
}

bool CreateStorage(const VulkanContext& context, VkDeviceSize size,
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
} // namespace
bool CreateVulkanRayTracingModelPrimitive(
    const VulkanContext& context, VulkanRayTracingScene& scene,
    const ModelAsset* source, u32 primitiveIndex, const VulkanModelAsset& gpu,
    const VulkanModelPrimitiveRange& range, VulkanRayTracingModelPrimitive& output)
{
    if (source == nullptr || !gpu.HasRayTracingGeometry() || range.indexCount < 3 ||
        range.indexCount % 3 != 0 || range.vertexCount == 0 || gpu.vertexCount == 0 ||
        gpu.indexCount == 0 || range.vertexCount > gpu.vertexCount ||
        range.indexCount > gpu.indexCount ||
        range.firstVertex > gpu.vertexCount - range.vertexCount ||
        range.firstIndex > gpu.indexCount - range.indexCount ||
        range.materialIndex >= kVulkanRayTracingModelInstanceBit ||
        scene.scratchAlignment == 0) {
        return false;
    }
    const VkDeviceAddress vertexAddress = gpu.vertexBuffer.GetDeviceAddress();
    const VkDeviceAddress indexBase = gpu.indexBuffer.GetDeviceAddress();
    if (vertexAddress % kVulkanRayTracingModelAddressAlignment != 0 ||
        indexBase % kVulkanRayTracingModelAddressAlignment != 0) {
        return false;
    }
    VkDeviceAddress indexAddress = 0;
    if (!AddAddress(indexBase, static_cast<VkDeviceSize>(range.firstIndex) * sizeof(u32),
                    indexAddress)) {
        return false;
    }
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(ModelVertex);
    triangles.maxVertex = range.firstVertex + range.vertexCount - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
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
    const u32 primitiveCount = range.indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    scene.dispatch.getBuildSizes(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                  &build, &primitiveCount, &sizes);
    VkDeviceSize scratchSize = 0;
    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0 ||
        !AlignSize(sizes.buildScratchSize, scene.scratchAlignment, scratchSize) ||
        !CreateStorage(context, sizes.accelerationStructureSize,
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, output.storage) ||
        !CreateStorage(context, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, output.scratch)) {
        DestroyVulkanBuffer(context, output.storage);
        DestroyVulkanBuffer(context, output.scratch);
        return false;
    }
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = output.storage.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (scene.dispatch.createAccelerationStructure(context.device, &createInfo, nullptr,
                                                   &output.accelerationStructure) != VK_SUCCESS) {
        DestroyVulkanBuffer(context, output.storage);
        DestroyVulkanBuffer(context, output.scratch);
        return false;
    }
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = output.accelerationStructure;
    output.address = scene.dispatch.getDeviceAddress(context.device, &addressInfo);
    if (output.address == 0) {
        scene.dispatch.destroyAccelerationStructure(context.device,
                                                    output.accelerationStructure, nullptr);
        output.accelerationStructure = VK_NULL_HANDLE;
        DestroyVulkanBuffer(context, output.storage);
        DestroyVulkanBuffer(context, output.scratch);
        return false;
    }
    output.source = source;
    output.primitiveIndex = primitiveIndex;
    output.meshIndex = range.meshIndex;
    output.materialIndex = range.materialIndex;
    output.firstVertex = range.firstVertex;
    output.firstIndex = range.firstIndex;
    output.indexCount = range.indexCount;
    output.vertexCount = range.vertexCount;
    output.vertexBuffer = gpu.vertexBuffer.buffer;
    output.indexBuffer = gpu.indexBuffer.buffer;
    output.vertexAddress = vertexAddress;
    output.indexAddress = indexAddress;
    output.scratchSize = scratchSize;
    if (output.IsReady()) return true;
    scene.dispatch.destroyAccelerationStructure(context.device,
                                                output.accelerationStructure, nullptr);
    output.accelerationStructure = VK_NULL_HANDLE;
    DestroyVulkanBuffer(context, output.storage);
    DestroyVulkanBuffer(context, output.scratch);
    output = {};
    return false;
}

} // namespace Concord
