// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/asset/ModelAsset.h"

namespace Concord {
namespace {

VkAccelerationStructureGeometryKHR MakeGeometry(
    const VulkanRayTracingModelPrimitive& primitive) noexcept
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = primitive.vertexAddress;
    triangles.vertexStride = sizeof(ModelVertex);
    triangles.maxVertex = primitive.firstVertex + primitive.vertexCount - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = primitive.indexAddress;
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    return geometry;
}

} // namespace

bool RecordVulkanRayTracingModelBuilds(VkCommandBuffer commandBuffer,
                                       const VulkanRayTracingScene& scene) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !scene.dispatch.IsReady()) return false;
    for (const VulkanRayTracingModelPrimitive& primitive : scene.modelPrimitives) {
        if (!primitive.IsReady() || scene.scratchAlignment == 0 ||
            primitive.vertexAddress % kVulkanRayTracingModelAddressAlignment != 0 ||
            primitive.indexAddress % kVulkanRayTracingModelAddressAlignment != 0 ||
            primitive.scratch.GetDeviceAddress() % scene.scratchAlignment != 0) {
            return false;
        }
        const VkAccelerationStructureGeometryKHR geometry = MakeGeometry(primitive);
        VkAccelerationStructureBuildGeometryInfoKHR build{};
        build.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.dstAccelerationStructure = primitive.accelerationStructure;
        build.geometryCount = 1;
        build.pGeometries = &geometry;
        build.scratchData.deviceAddress = primitive.scratch.GetDeviceAddress();
        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = primitive.indexCount / 3;
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = {&range};
        scene.dispatch.cmdBuildAccelerationStructures(commandBuffer, 1, &build, ranges);
    }
    return true;
}

} // namespace Concord
