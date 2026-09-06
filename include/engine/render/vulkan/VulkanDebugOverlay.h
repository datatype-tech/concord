// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANDEBUGOVERLAY_H
#define CONCORD_VULKANDEBUGOVERLAY_H

#include "engine/core/Types.h"
#include "engine/debug/DebugOverlayFrame.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanDebugFont.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * GPU state for drawing the debug text overlay on top of the scene.
 *
 * The glyphs live in a one-time-uploaded 8x8 font atlas; each visible line
 * becomes six vertices per character in a per-frame-slot host buffer, drawn
 * twice through one dedicated alpha-blended pipeline (a black offset pass
 * for readability, then the text itself). Creation fails silently to an
 * unusable state whenever the shader artifacts or the format are missing, so
 * the overlay degrades to "not drawn" instead of failing the backend.
 */
struct VulkanDebugOverlay {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImage atlasImage = VK_NULL_HANDLE;
    VkDeviceMemory atlasMemory = VK_NULL_HANDLE;
    VkImageView atlasView = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    /** Baked glyph metrics and pen layout driving the vertex generation. */
    DebugFontBake font{};
    /** One host-visible quad buffer per frame in flight. */
    VulkanBuffer vertices[kMaxFramesInFlight]{};

    /** Whether every resource the overlay draws with is alive. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return device != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE &&
               layout != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE &&
               sampler != VK_NULL_HANDLE && atlasView != VK_NULL_HANDLE &&
               vertices[0].IsReady();
    }
};

/**
 * Creates the atlas, pipeline and quad buffers.
 *
 * @return True when the overlay is usable; a false result leaves `overlay`
 *         in a safe-to-destroy state and simply disables the feature.
 */
bool CreateVulkanDebugOverlay(const VulkanContext& context, VkFormat colorFormat,
                              VulkanDebugOverlay& overlay);

/** Releases every overlay resource; safe on a partially created overlay. */
void DestroyVulkanDebugOverlay(const VulkanContext& context, VulkanDebugOverlay& overlay);

/**
 * Records the overlay draw into `commandBuffer` over the current scene.
 *
 * The image must be in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL and the
 * scene passes must already be recorded; the overlay loads and stores the
 * existing contents. Vertices beyond the per-slot buffer capacity are
 * skipped rather than wrapped.
 */
void RecordVulkanDebugOverlay(VkCommandBuffer commandBuffer, VulkanDebugOverlay& overlay,
                              u32 frameSlot, VkExtent2D extent, VkImageView colorView,
                              const DebugOverlayFrame& frame);

} // namespace Concord

#endif // CONCORD_VULKANDEBUGOVERLAY_H
