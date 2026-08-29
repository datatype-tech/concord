// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKENDRAYTRACING_H
#define CONCORD_VULKANRENDERBACKENDRAYTRACING_H

#include "engine/render/RenderSceneSnapshot.h"
#include "engine/render/vulkan/VulkanBoxPipeline.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanRayTracingPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingScene.h"

namespace Concord {

/** Records AS builds and an optional primary-ray dispatch for one frame. */
bool RecordVulkanRayTracingFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                 VulkanRayTracingScene& scene,
                                 const RenderSceneSnapshot& snapshot,
                                 const VulkanRayTracingPipeline& pipeline,
                                 VulkanRayTracingOutputRing& outputRing,
                                 const VulkanBoxPipeline& boxPipeline,
                                 VkDescriptorSet frameDataSet, u32 frameIndex,
                                 bool& sceneBuilt) noexcept;

/** Blits a completed RT output into the acquired swapchain image. */
bool CompositeVulkanRayTracingFrame(const VulkanContext& context, VkCommandBuffer commandBuffer,
                                    VulkanRayTracingOutputRing& outputRing, u32 frameIndex,
                                    VkImage swapchainImage, VkFormat swapchainFormat,
                                    VkImageLayout swapchainLayout, VkExtent2D extent) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRENDERBACKENDRAYTRACING_H
