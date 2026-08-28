// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <span>

namespace Concord {
namespace {

/** Creates the identity instance that references the static Box BLAS. */
VkAccelerationStructureInstanceKHR MakeInstance(VkDeviceAddress bottomLevel) noexcept
{
    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0f;
    instance.transform.matrix[1][1] = 1.0f;
    instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xff;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                     VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    instance.accelerationStructureReference = bottomLevel;
    return instance;
}

/** Allocates device-local storage backing the TLAS. */
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

bool CreateVulkanRayTracingSceneTopLevel(const VulkanContext& context,
                                        VulkanRayTracingScene& scene)
{
    if (!scene.dispatch.IsReady() || scene.bottomLevelAddress == 0) {
        return false;
    }
    const VkAccelerationStructureInstanceKHR instance = MakeInstance(scene.bottomLevelAddress);
    VulkanBufferCreateInfo instanceInfo{};
    instanceInfo.size = sizeof(VkAccelerationStructureInstanceKHR) *
                        kVulkanRayTracingMaxInstances;
    instanceInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    instanceInfo.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    instanceInfo.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    instanceInfo.persistentMap = true;
    instanceInfo.deviceAddress = true;
    if (!CreateVulkanBuffer(context, instanceInfo, scene.instanceBuffer) ||
        scene.instanceBuffer.GetDeviceAddress() % kVulkanRayTracingInstanceAddressAlignment != 0 ||
        !UploadVulkanBuffer(scene.instanceBuffer,
                            std::as_bytes(std::span<const VkAccelerationStructureInstanceKHR>(
                                &instance, 1)))) {
        DestroyVulkanBuffer(context, scene.instanceBuffer);
        return false;
    }

    VkAccelerationStructureGeometryInstancesDataKHR instances{};
    instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instances.arrayOfPointers = VK_FALSE;
    instances.data.deviceAddress = scene.instanceBuffer.GetDeviceAddress();
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instances;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR build{};
    build.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    const u32 instanceCount = kVulkanRayTracingMaxInstances;
    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    scene.dispatch.getBuildSizes(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                  &build, &instanceCount, &sizes);
    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0 ||
        !CreateStorage(context, sizes.accelerationStructureSize, scene.topLevelBuffer)) {
        DestroyVulkanBuffer(context, scene.instanceBuffer);
        return false;
    }

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = scene.topLevelBuffer.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    const VkResult result = scene.dispatch.createAccelerationStructure(
        context.device, &createInfo, nullptr, &scene.topLevel);
    if (result != VK_SUCCESS) {
        DestroyVulkanBuffer(context, scene.topLevelBuffer);
        DestroyVulkanBuffer(context, scene.instanceBuffer);
        return VulkanFailed("vkCreateAccelerationStructureKHR(TLAS)", result);
    }

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = scene.topLevel;
    scene.topLevelAddress = scene.dispatch.getDeviceAddress(context.device, &addressInfo);
    if (scene.topLevelAddress == 0) {
        scene.dispatch.destroyAccelerationStructure(context.device, scene.topLevel, nullptr);
        scene.topLevel = VK_NULL_HANDLE;
        DestroyVulkanBuffer(context, scene.topLevelBuffer);
        DestroyVulkanBuffer(context, scene.instanceBuffer);
        return false;
    }
    scene.topLevelScratchSize = sizes.buildScratchSize;
    return true;
}

} // namespace Concord
