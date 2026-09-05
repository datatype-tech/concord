// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H
#define CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H

#include "engine/render/vulkan/VulkanRayTracingScene.h"
#include "engine/render/vulkan/VulkanModelAssetCache.h"

namespace Concord {

/** Loads KHR acceleration-structure commands from one logical device. */
bool LoadVulkanRayTracingDispatch(VkDevice device,
                                  VulkanRayTracingDispatch& dispatch) noexcept;

/** Uploads the static unit Box vertex and index arrays. */
bool CreateVulkanRayTracingSceneGeometry(const VulkanContext& context,
                                         VulkanRayTracingScene& scene);

/** Creates the triangle BLAS and caches its build requirements/address. */
bool CreateVulkanRayTracingSceneBottomLevel(const VulkanContext& context,
                                           VulkanRayTracingScene& scene);

/** Creates the one-instance TLAS and caches its build requirements/address. */
bool CreateVulkanRayTracingSceneTopLevel(const VulkanContext& context,
                                        VulkanRayTracingScene& scene);

/** Lazily creates BLAS resources for static imported-model primitives. */
bool EnsureVulkanRayTracingModelPrimitives(
    const VulkanContext& context, VulkanRayTracingScene& scene,
    const RenderSceneSnapshot& snapshot, const VulkanModelAssetCache& modelAssets);

/** Creates storage and metadata for one imported primitive BLAS. */
bool CreateVulkanRayTracingModelPrimitive(
    const VulkanContext& context, VulkanRayTracingScene& scene,
    const ModelAsset* source, u32 primitiveIndex, const VulkanModelAsset& gpu,
    const VulkanModelPrimitiveRange& range, VulkanRayTracingModelPrimitive& output);

/** Appends one imported primitive to the model hit-shader CPU payload. */
bool AppendVulkanRayTracingModelData(
    VulkanRayTracingScene& scene, const ModelAsset* source, u32 primitiveIndex,
    const VulkanModelAsset& gpu, const VulkanModelPrimitiveRange& range,
    VulkanRayTracingModelPrimitive& output);

/** Recreates packed model SSBOs and refreshes the scene descriptor set. */
bool RebuildVulkanRayTracingModelBuffers(const VulkanContext& context,
                                         VulkanRayTracingScene& scene);

/** Records all imported-model BLAS builds before the TLAS build. */
bool RecordVulkanRayTracingModelBuilds(VkCommandBuffer commandBuffer,
                                       const VulkanRayTracingScene& scene) noexcept;

/** Releases imported-model BLAS resources owned by one frame slot. */
void DestroyVulkanRayTracingModelPrimitives(const VulkanContext& context,
                                            VulkanRayTracingScene& scene) noexcept;

/** Records both acceleration-structure builds into a command buffer. */
bool RecordVulkanRayTracingSceneBuildInternal(
    VkCommandBuffer commandBuffer, VulkanRayTracingScene& scene,
    const RenderSceneSnapshot* snapshot) noexcept;

/** Uploads bounded TLAS instances for the current render snapshot. */
u32 UploadVulkanRayTracingInstances(VulkanRayTracingScene& scene,
                                    const RenderSceneSnapshot* snapshot) noexcept;

/** Inserts host-to-acceleration-structure input visibility barriers. */
void InsertVulkanRayTracingInputBarrier(VkCommandBuffer commandBuffer,
                                         const VulkanRayTracingScene& scene) noexcept;

/** Inserts host-write visibility barriers for imported model geometry buffers. */
bool InsertVulkanRayTracingModelInputBarrier(
    VkCommandBuffer commandBuffer, const VulkanRayTracingScene& scene) noexcept;

/** Makes packed model SSBO host writes visible to RT and fragment shaders. */
bool InsertVulkanRayTracingModelShaderBarrier(
    VkCommandBuffer commandBuffer, const VulkanRayTracingScene& scene) noexcept;

/** Inserts the BLAS-to-TLAS build ordering barrier. */
void InsertVulkanRayTracingBuildBarrier(VkCommandBuffer commandBuffer) noexcept;

/** Creates and updates the single acceleration-structure descriptor set. */
bool CreateVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                          VulkanRayTracingScene& scene);

/** Refreshes only the model storage-buffer bindings in an existing descriptor set. */
bool UpdateVulkanRayTracingSceneModelDescriptors(const VulkanContext& context,
                                                 VulkanRayTracingScene& scene);

/** Releases the acceleration-structure descriptor objects. */
void DestroyVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                            VulkanRayTracingScene& scene) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H
