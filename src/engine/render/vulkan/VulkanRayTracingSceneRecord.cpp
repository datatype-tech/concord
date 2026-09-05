// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/core/Vec3.h"

namespace Concord {
namespace {

/** Builds the indexed triangle descriptor consumed by the BLAS command. */
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

/** Builds the one-instance descriptor consumed by the TLAS command. */
VkAccelerationStructureGeometryKHR MakeInstances(const VulkanRayTracingScene& scene) noexcept
{
    VkAccelerationStructureGeometryInstancesDataKHR instances{};
    instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instances.arrayOfPointers = VK_FALSE;
    instances.data.deviceAddress = scene.instanceBuffer.GetDeviceAddress();
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instances;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    return geometry;
}

} // namespace

bool RecordVulkanRayTracingSceneBuildInternal(
    VkCommandBuffer commandBuffer, VulkanRayTracingScene& scene,
    const RenderSceneSnapshot* snapshot) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !scene.IsReady() || scene.scratchAlignment == 0 ||
        scene.scratchBuffer.GetDeviceAddress() % scene.scratchAlignment != 0) {
        return false;
    }
    const u32 instanceCount = UploadVulkanRayTracingInstances(scene, snapshot);
    if (instanceCount == 0) {
        return false;
    }
    const VkAccelerationStructureGeometryKHR triangles = MakeTriangles(scene);
    VkAccelerationStructureBuildGeometryInfoKHR bottom{};
    bottom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    bottom.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bottom.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bottom.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bottom.dstAccelerationStructure = scene.bottomLevel;
    bottom.geometryCount = 1;
    bottom.pGeometries = &triangles;
    bottom.scratchData.deviceAddress = scene.scratchBuffer.GetDeviceAddress();
    VkAccelerationStructureBuildRangeInfoKHR bottomRange{};
    bottomRange.primitiveCount = kVulkanRayTracingBoxPrimitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* bottomRanges[] = {&bottomRange};
    InsertVulkanRayTracingInputBarrier(commandBuffer, scene);
    if (!InsertVulkanRayTracingModelInputBarrier(commandBuffer, scene)) {
        return false;
    }
    scene.dispatch.cmdBuildAccelerationStructures(commandBuffer, 1, &bottom, bottomRanges);
    if (!RecordVulkanRayTracingModelBuilds(commandBuffer, scene)) {
        return false;
    }
    InsertVulkanRayTracingBuildBarrier(commandBuffer);

    const VkAccelerationStructureGeometryKHR instances = MakeInstances(scene);
    VkAccelerationStructureBuildGeometryInfoKHR top{};
    top.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    top.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    top.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    top.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    top.dstAccelerationStructure = scene.topLevel;
    top.geometryCount = 1;
    top.pGeometries = &instances;
    top.scratchData.deviceAddress = scene.scratchBuffer.GetDeviceAddress();
    VkAccelerationStructureBuildRangeInfoKHR topRange{};
    topRange.primitiveCount = instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* topRanges[] = {&topRange};
    scene.dispatch.cmdBuildAccelerationStructures(commandBuffer, 1, &top, topRanges);
    return InsertVulkanRayTracingModelShaderBarrier(commandBuffer, scene);
}

void InsertVulkanRayTracingSceneReadBarrier(VkCommandBuffer commandBuffer,
                                            VkPipelineStageFlags dstStages) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || dstStages == 0) {
        return;
    }
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         dstStages,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // namespace Concord
