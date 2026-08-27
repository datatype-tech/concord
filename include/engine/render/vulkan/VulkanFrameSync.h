// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMESYNC_H
#define CONCORD_VULKANFRAMESYNC_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/** How many frames the CPU may record ahead of the GPU. */
inline constexpr u32 kMaxFramesInFlight = 2;

/** The command buffer and synchronization objects belonging to one frame. */
struct VulkanFrame {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

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

/** Destroys everything the ring owns. */
void DestroyVulkanFrameRing(const VulkanContext& context, VulkanFrameRing& ring);

} // namespace Concord

#endif // CONCORD_VULKANFRAMESYNC_H
