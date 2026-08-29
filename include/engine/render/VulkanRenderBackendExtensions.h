// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKENDEXTENSIONS_H
#define CONCORD_VULKANRENDERBACKENDEXTENSIONS_H

#include "engine/render/VulkanPass.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanFrameSync.h"
#include "engine/render/vulkan/VulkanSwapchain.h"
#include "engine/render/vulkan/VulkanRayTracingScene.h"

namespace Concord {

/**
 * Runs registered callbacks with the current frame's opaque native handles.
 * BeforeScene and AfterScene run while the command buffer is recording, but
 * outside Concord's internal dynamic-rendering scope. Initialize and Shutdown
 * are lifecycle notifications: their commandBufferRecording flag is zero,
 * imageIndex is VulkanPassInvalidImageIndex, and per-frame attachment,
 * descriptor, and acceleration-structure handles are zero. Callbacks must
 * not issue commands on the engine-owned command buffer. No callback may
 * submit/end that command buffer or change the reported image layouts behind
 * the backend's state tracking.
 */
bool RunVulkanRenderExtensions(const VulkanContext& context,
                               const VulkanSwapchain& swapchain,
                               const VulkanDepthBuffer& depth,
                               const VulkanFrame& frame, u32 frameIndex,
                               u32 imageIndex, VkDescriptorSet frameDescriptorSet,
                               VkDescriptorSet shadowDescriptorSet,
                               const VulkanRayTracingScene* rayTracingScene,
                               u64 swapchainGeneration,
                               VulkanPassPhase phase) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRENDERBACKENDEXTENSIONS_H
