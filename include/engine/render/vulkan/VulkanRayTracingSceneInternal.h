// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H
#define CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H

#include "engine/render/vulkan/VulkanRayTracingScene.h"
#include "engine/render/vulkan/VulkanModelAssetCache.h"
#include "engine/render/vulkan/VulkanTextureKey.h"

namespace Concord {

/**
 * Registers a material's base-colour texture and returns its sampler slot.
 *
 * The table arms itself on first use so a scene that never had textures still
 * resolves everything to the fallback slot. An untextured material, or one
 * whose key does not fit the fixed array, yields slot 0 -- the white texture
 * the renderer always binds -- rather than an index the shader cannot use.
 */
[[nodiscard]] inline u32 AcquireRayTracingTextureSlot(RayTracingTextureSlots& slots,
                                                      const ModelAsset& asset,
                                                      const VulkanModelAsset& gpu,
                                                      u32 materialIndex)
{
    if (!slots.IsReady()) {
        ResetRayTracingTextureSlots(slots);
    }
    if (materialIndex >= gpu.baseColorTextures.size()) {
        return 0;
    }
    return slots.Acquire(MakeVulkanTextureCacheKey(gpu.baseColorTextures[materialIndex],
                                                   VulkanModelAssetDirectory(asset)));
}

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

/** Writes this frame's live water disturbances into the ripple SSBO. */
bool UploadVulkanRayTracingRipples(VulkanRayTracingScene& scene,
                                   const RenderSceneSnapshot* snapshot) noexcept;

/** Recreates packed model SSBOs and refreshes the scene descriptor set. */
bool RebuildVulkanRayTracingModelBuffers(const VulkanContext& context,
                                         VulkanRayTracingScene& scene);

/** Records all imported-model BLAS builds before the TLAS build. */
bool RecordVulkanRayTracingModelBuilds(VkCommandBuffer commandBuffer,
                                       const VulkanRayTracingScene& scene) noexcept;

/** Appends one CPU-side skinned source per animated model instance. */
bool AppendVulkanRayTracingSkinnedSources(
    VulkanRayTracingScene& scene, const RenderSceneSnapshot& snapshot,
    const VulkanModelAssetCache& modelAssets);

/** Creates the BLAS objects skinned sources need; call after model buffers exist. */
bool CreateVulkanRayTracingSkinnedPrimitives(const VulkanContext& context,
                                             VulkanRayTracingScene& scene);

/** Re-points skinned BLAS input at the current packed model buffers. */
void RefreshVulkanRayTracingSkinnedAddresses(VulkanRayTracingScene& scene) noexcept;

/** Deforms every skinned source from its entity pose and re-uploads the SSBO. */
bool UpdateVulkanRayTracingSkinnedGeometry(VulkanRayTracingScene& scene,
                                           const RenderSceneSnapshot& snapshot);

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
