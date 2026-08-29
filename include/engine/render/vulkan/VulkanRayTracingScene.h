// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENE_H
#define CONCORD_VULKANRAYTRACINGSCENE_H

#include "engine/core/Types.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanBuffer.h"

#include <vulkan/vulkan.h>

namespace Concord {

/** Device entry points required by the optional KHR acceleration-structure path. */
struct VulkanRayTracingDispatch {
    PFN_vkCreateAccelerationStructureKHR createAccelerationStructure = nullptr;
    PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR getBuildSizes = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR getDeviceAddress = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR cmdBuildAccelerationStructures = nullptr;

    /** Whether every command used by the scene resource is available. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return createAccelerationStructure != nullptr &&
               destroyAccelerationStructure != nullptr && getBuildSizes != nullptr &&
               getDeviceAddress != nullptr && cmdBuildAccelerationStructures != nullptr;
    }
};

inline constexpr u32 kVulkanRayTracingBoxVertexCount = 8;
inline constexpr u32 kVulkanRayTracingBoxIndexCount = 36;
inline constexpr u32 kVulkanRayTracingBoxPrimitiveCount = 12;
inline constexpr u32 kVulkanRayTracingMaxInstances = 256;
inline constexpr u32 kVulkanRayTracingDescriptorSet = 2;
inline constexpr VkDeviceSize kVulkanRayTracingVertexAddressAlignment = 4;
inline constexpr VkDeviceSize kVulkanRayTracingInstanceAddressAlignment = 16;

/** Optional hardware ray-tracing acceleration structures for one static unit Box. */
struct VulkanRayTracingScene {
    VkDevice device = VK_NULL_HANDLE;
    VulkanRayTracingDispatch dispatch{};
    VulkanBuffer vertexBuffer{};
    VulkanBuffer indexBuffer{};
    VulkanBuffer instanceBuffer{};
    VulkanBuffer bottomLevelBuffer{};
    VulkanBuffer topLevelBuffer{};
    VulkanBuffer scratchBuffer{};
    VkAccelerationStructureKHR bottomLevel = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevel = VK_NULL_HANDLE;
    VkDeviceAddress bottomLevelAddress = 0;
    VkDeviceAddress topLevelAddress = 0;
    VkDeviceSize bottomLevelScratchSize = 0;
    VkDeviceSize topLevelScratchSize = 0;
    VkDeviceSize scratchAlignment = 0;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    /** Includes non-shadow-casting meshes when this scene feeds primary RT rays. */
    bool includeNonShadowCasters = false;

    /** Whether all buffers, acceleration structures, and dispatch commands exist. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return device != VK_NULL_HANDLE && dispatch.IsReady() &&
               vertexBuffer.HasDeviceAddress() && indexBuffer.HasDeviceAddress() &&
               instanceBuffer.HasDeviceAddress() &&
               vertexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment == 0 &&
               indexBuffer.GetDeviceAddress() % kVulkanRayTracingVertexAddressAlignment == 0 &&
               instanceBuffer.GetDeviceAddress() % kVulkanRayTracingInstanceAddressAlignment == 0 &&
               bottomLevelBuffer.HasDeviceAddress() &&
               topLevelBuffer.HasDeviceAddress() && scratchBuffer.HasDeviceAddress() &&
               bottomLevel != VK_NULL_HANDLE && topLevel != VK_NULL_HANDLE &&
               bottomLevelAddress != 0 && topLevelAddress != 0 && scratchAlignment != 0 &&
               descriptorLayout != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE &&
               descriptorSet != VK_NULL_HANDLE;
    }
};

/** Allocates static Box geometry, BLAS/TLAS storage, and one TLAS instance. */
bool CreateVulkanRayTracingScene(const VulkanContext& context,
                                 VulkanRayTracingScene& scene);

/** Releases scene resources; callers must ensure no submitted work is using them. */
void DestroyVulkanRayTracingScene(const VulkanContext& context,
                                  VulkanRayTracingScene& scene) noexcept;

/**
 * Records the BLAS and TLAS builds into the caller's current command buffer.
 *
 * The scene's acceleration structures and scratch storage are mutable build
 * destinations. Do not record a second submission until the first has
 * completed; VulkanRayTracingSceneRing supplies one isolated scene per frame
 * slot for that lifetime rule.
 */
bool RecordVulkanRayTracingSceneBuild(VkCommandBuffer commandBuffer,
                                      VulkanRayTracingScene& scene,
                                      const RenderSceneSnapshot* snapshot = nullptr) noexcept;

/** Binds the scene's top-level AS descriptor for a graphics or RT pass. */
bool BindVulkanRayTracingScene(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                               const VulkanRayTracingScene& scene,
                               VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) noexcept;

/** Makes a completed TLAS visible to the requested ray-consuming shader stage.
 * The default targets fragment ray queries; a full RT pipeline should pass
 * `VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR` explicitly.
 */
void InsertVulkanRayTracingSceneReadBarrier(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGSCENE_H
