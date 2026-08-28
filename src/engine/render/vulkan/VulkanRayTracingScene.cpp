// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingScene.h"

#include "engine/render/vulkan/VulkanPhysicalDevice.h"
#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <algorithm>
#include <limits>

namespace Concord {
namespace {

template <typename Function>
Function LoadFunction(VkDevice device, const char* name) noexcept
{
    return reinterpret_cast<Function>(vkGetDeviceProcAddr(device, name));
}

/** Returns the physical-device scratch alignment required by KHR AS builds. */
VkDeviceSize QueryScratchAlignment(VkPhysicalDevice device) noexcept
{
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{};
    asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &asProperties;
    vkGetPhysicalDeviceProperties2(device, &properties);
    return asProperties.minAccelerationStructureScratchOffsetAlignment;
}

/** Rounds a scratch allocation up without wrapping the device-size type. */
bool AlignScratchSize(VkDeviceSize value, VkDeviceSize alignment,
                      VkDeviceSize& result) noexcept
{
    if (alignment == 0) {
        return false;
    }
    const VkDeviceSize remainder = value % alignment;
    if (remainder == 0) {
        result = value;
        return true;
    }
    const VkDeviceSize padding = alignment - remainder;
    if (padding > std::numeric_limits<VkDeviceSize>::max() - value) {
        return false;
    }
    result = value + padding;
    return true;
}

} // namespace
bool LoadVulkanRayTracingDispatch(VkDevice device,
                                  VulkanRayTracingDispatch& dispatch) noexcept
{
    dispatch = {};
    if (device == VK_NULL_HANDLE) {
        return false;
    }
    dispatch.createAccelerationStructure =
        LoadFunction<PFN_vkCreateAccelerationStructureKHR>(
            device, "vkCreateAccelerationStructureKHR");
    dispatch.destroyAccelerationStructure =
        LoadFunction<PFN_vkDestroyAccelerationStructureKHR>(
            device, "vkDestroyAccelerationStructureKHR");
    dispatch.getBuildSizes = LoadFunction<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        device, "vkGetAccelerationStructureBuildSizesKHR");
    dispatch.getDeviceAddress = LoadFunction<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        device, "vkGetAccelerationStructureDeviceAddressKHR");
    dispatch.cmdBuildAccelerationStructures =
        LoadFunction<PFN_vkCmdBuildAccelerationStructuresKHR>(
            device, "vkCmdBuildAccelerationStructuresKHR");
    return dispatch.IsReady();
}

bool CreateVulkanRayTracingScene(const VulkanContext& context,
                                 VulkanRayTracingScene& scene)
{
    DestroyVulkanRayTracingScene(context, scene);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE ||
        !QueueFamilySupportsCompute(context.physicalDevice, context.queueFamily) ||
        (!context.rayTracing.IsUsable() && !context.rayTracing.IsRayQueryUsable())) {
        return false;
    }
    scene.device = context.device;
    scene.scratchAlignment = QueryScratchAlignment(context.physicalDevice);
    if (scene.scratchAlignment == 0 || !LoadVulkanRayTracingDispatch(scene.device, scene.dispatch) ||
        !CreateVulkanRayTracingSceneGeometry(context, scene) ||
        scene.vertexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment != 0 ||
        scene.indexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment != 0 ||
        !CreateVulkanRayTracingSceneBottomLevel(context, scene) ||
        !CreateVulkanRayTracingSceneTopLevel(context, scene) ||
        !CreateVulkanRayTracingSceneDescriptor(context, scene)) {
        DestroyVulkanRayTracingScene(context, scene);
        return false;
    }
    const VkDeviceSize scratchSize =
        std::max(scene.bottomLevelScratchSize, scene.topLevelScratchSize);
    VkDeviceSize alignedSize = 0;
    if (scratchSize == 0 || !AlignScratchSize(scratchSize, scene.scratchAlignment, alignedSize)) {
        DestroyVulkanRayTracingScene(context, scene);
        return false;
    }
    VulkanBufferCreateInfo scratchInfo{};
    scratchInfo.size = alignedSize;
    scratchInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    scratchInfo.requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    scratchInfo.preferredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    scratchInfo.deviceAddress = true;
    if (!CreateVulkanBuffer(context, scratchInfo, scene.scratchBuffer) ||
        scene.scratchBuffer.GetDeviceAddress() % scene.scratchAlignment != 0 ||
        !scene.IsReady()) {
        DestroyVulkanRayTracingScene(context, scene);
        return false;
    }
    return true;
}

void DestroyVulkanRayTracingScene(const VulkanContext& context,
                                  VulkanRayTracingScene& scene) noexcept
{
    const VkDevice device = scene.device != VK_NULL_HANDLE ? scene.device : context.device;
    if (device != VK_NULL_HANDLE && scene.dispatch.destroyAccelerationStructure != nullptr) {
        if (scene.topLevel != VK_NULL_HANDLE) {
            scene.dispatch.destroyAccelerationStructure(device, scene.topLevel, nullptr);
        }
        if (scene.bottomLevel != VK_NULL_HANDLE) {
            scene.dispatch.destroyAccelerationStructure(device, scene.bottomLevel, nullptr);
        }
    }
    DestroyVulkanRayTracingSceneDescriptor(context, scene);
    DestroyVulkanBuffer(context, scene.scratchBuffer);
    DestroyVulkanBuffer(context, scene.topLevelBuffer);
    DestroyVulkanBuffer(context, scene.bottomLevelBuffer);
    DestroyVulkanBuffer(context, scene.instanceBuffer);
    DestroyVulkanBuffer(context, scene.indexBuffer);
    DestroyVulkanBuffer(context, scene.vertexBuffer);
    scene = {};
}

bool RecordVulkanRayTracingSceneBuild(VkCommandBuffer commandBuffer,
                                      VulkanRayTracingScene& scene,
                                      const RenderSceneSnapshot* snapshot) noexcept
{
    return RecordVulkanRayTracingSceneBuildInternal(commandBuffer, scene, snapshot);
}

} // namespace Concord
