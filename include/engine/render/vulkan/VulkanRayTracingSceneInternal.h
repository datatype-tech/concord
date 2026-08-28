// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H
#define CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H

#include "engine/render/vulkan/VulkanRayTracingScene.h"

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

/** Inserts the BLAS-to-TLAS build ordering barrier. */
void InsertVulkanRayTracingBuildBarrier(VkCommandBuffer commandBuffer) noexcept;

/** Creates and updates the single acceleration-structure descriptor set. */
bool CreateVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                          VulkanRayTracingScene& scene);

/** Releases the acceleration-structure descriptor objects. */
void DestroyVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                            VulkanRayTracingScene& scene) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGSCENEINTERNAL_H
