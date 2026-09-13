// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENE_H
#define CONCORD_VULKANRAYTRACINGSCENE_H

#include "engine/asset/ModelAsset.h"
#include "engine/asset/SkinnedGeometry.h"
#include "engine/core/Types.h"
#include "engine/render/RayTracingTextureSlots.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanBoxMaterial.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanRippleSource.h"
#include "engine/render/vulkan/VulkanRayTracingModel.h"

#include <vulkan/vulkan.h>

#include <vector>

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
/**
 * Bits of a TLAS instance mask, which decides what a ray is allowed to hit.
 *
 * One acceleration structure serves both the camera and the sun, so the two
 * are separated by mask rather than by building two trees: an object authored
 * with `castShadow = false` stays visible to primary rays while it stops
 * blocking the shadow rays. Without this the promise was empty on the ray
 * traced path -- a water surface spanning a basin occluded every point under
 * it from a low sun, which left the whole floor on ambient light alone.
 */
inline constexpr u32 kVulkanRayTracingMaskVisible = 0x02u;
inline constexpr u32 kVulkanRayTracingMaskLightBlocker = 0x01u;
inline constexpr u32 kVulkanRayTracingDescriptorSet = 2;
inline constexpr VkDeviceSize kVulkanRayTracingVertexAddressAlignment = 4;
inline constexpr VkDeviceSize kVulkanRayTracingInstanceAddressAlignment = 16;

/**
 * Bind-pose geometry and palette binding for one per-frame skinned BLAS.
 *
 * A skinned primitive cannot share one BLAS across instances the way a static
 * primitive does, because every animated entity deforms the same rest mesh
 * differently. Each source therefore owns its own metadata range and is
 * refit every frame from CPU-solved vertices.
 */
struct VulkanRayTracingSkinnedSource {
    /** Rest-pose geometry, carrying the joint indices and weights to deform by. */
    ModelPrimitive rest{};
    /** Entity whose animated pose drives this geometry. */
    Entity entity{};
    /** Index of this source's BLAS entry inside scene.modelPrimitives. */
    u32 modelPrimitiveIndex = 0;
    /** Index into the hit-shader primitive metadata SSBO. */
    u32 metadataIndex = 0;
    /** First vertex of this primitive inside the packed model vertex SSBO. */
    u32 metadataFirstVertex = 0;
    u32 vertexCount = 0;
    /** Reused deformation scratch, so a frame allocates nothing here. */
    std::vector<SkinnedVertex> scratch;
};

/** Optional hardware ray-tracing structures for Box and imported-model geometry. */
struct VulkanRayTracingScene {
    VkDevice device = VK_NULL_HANDLE;
    VulkanRayTracingDispatch dispatch{};
    VulkanBuffer vertexBuffer{};
    VulkanBuffer indexBuffer{};
    VulkanBuffer instanceBuffer{};
    VulkanBuffer bottomLevelBuffer{};
    VulkanBuffer topLevelBuffer{};
    VulkanBuffer scratchBuffer{};
    VulkanBuffer modelVertexBuffer{};
    VulkanBuffer modelIndexBuffer{};
    VulkanBuffer modelPrimitiveBuffer{};
    /** Authored material of every Box instance, indexed by its custom index. */
    VulkanBuffer boxMaterialBuffer{};
    /** Live water disturbances, rewritten every frame. */
    VulkanBuffer rippleSourceBuffer{};
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
    /** BLAS resources lazily created for static imported-model primitives. */
    std::vector<VulkanRayTracingModelPrimitive> modelPrimitives;
    /** CPU mirror used to populate the model hit-shader metadata SSBO. */
    std::vector<VulkanRayTracingModelPrimitiveInfo> modelPrimitiveInfos;
    /** Packed CPU geometry consumed by the model hit-shader SSBOs. */
    std::vector<VulkanRayTracingModelVertex> modelVertices;
    std::vector<u32> modelIndices;
    /** Per-frame skinned primitives, each refit from its entity's pose. */
    std::vector<VulkanRayTracingSkinnedSource> skinnedSources;
    /** Sampler slot per material's base-colour texture, read by the hit shader. */
    RayTracingTextureSlots textureSlots;
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
               modelPrimitiveBuffer.IsReady() && boxMaterialBuffer.IsReady() &&
               rippleSourceBuffer.IsReady() &&
               bottomLevel != VK_NULL_HANDLE && topLevel != VK_NULL_HANDLE &&
               bottomLevelAddress != 0 && topLevelAddress != 0 && scratchAlignment != 0 &&
               descriptorLayout != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE &&
               descriptorSet != VK_NULL_HANDLE;
    }
};

/** Allocates static Box geometry and BLAS/TLAS storage; model BLAS are lazy. */
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
