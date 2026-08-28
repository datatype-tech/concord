// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMESYNC_H
#define CONCORD_VULKANFRAMESYNC_H

#include "engine/core/Types.h"
#include "engine/render/RenderFrameData.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"

namespace Concord {

/** The command buffer and synchronization objects belonging to one frame. */
struct VulkanFrame {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    /** Whether the command buffer is currently between Begin and End. */
    bool commandBufferRecording = false;

    /** CPU-side frame packet reserved for the upcoming GPU buffer upload. */
    RenderFrameData renderData{};

    /** Signalled by the presentation engine once its image may be drawn to. */
    VkSemaphore imageAvailable = VK_NULL_HANDLE;

    /** Signalled on submit so the CPU knows this frame's slot is reusable. */
    VkFence inFlight = VK_NULL_HANDLE;
};

/** The command pool and every frame in flight. */
struct VulkanFrameRing {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VulkanFrame frames[kMaxFramesInFlight]{};
    u32 currentFrame = 0;

    /** The frame currently being recorded. */
    [[nodiscard]] VulkanFrame& Current() noexcept { return frames[currentFrame]; }

    /** Advances to the next frame slot, wrapping around the ring. */
    void Advance() noexcept { currentFrame = (currentFrame + 1) % kMaxFramesInFlight; }
};

/**
 * Allocates the command pool, command buffers, semaphores and fences.
 *
 * Fences start signalled so the first frame does not wait on work that was
 * never submitted.
 *
 * @return False when any object could not be created.
 */
bool CreateVulkanFrameRing(const VulkanContext& context, VulkanFrameRing& ring);

/**
 * Recreates a frame's semaphore and signalled fence after an aborted frame.
 *
 * The helper waits for submitted work, closes a recording command buffer,
 * resets it, and only then replaces the synchronization objects.
 */
bool RecoverVulkanFrame(const VulkanContext& context, VulkanFrame& frame);

/** Destroys everything the ring owns. */
void DestroyVulkanFrameRing(const VulkanContext& context, VulkanFrameRing& ring);

} // namespace Concord

#endif // CONCORD_VULKANFRAMESYNC_H
