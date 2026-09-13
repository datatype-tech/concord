// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMEPROBE_H
#define CONCORD_VULKANFRAMEPROBE_H

#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"

#include <chrono>

namespace Concord {

/**
 * Copies a finished frame back to the host and reports what is in it.
 *
 * Every claim about how a render looks is otherwise unverifiable from inside
 * the engine: an image either reads as intended or it does not, and without a
 * number there is no way to tell a change that fixed the exposure from one
 * that only moved it. The probe answers the questions that actually come up --
 * is the frame crushed or clipped, where does mid grey land, and does the
 * region the water occupies carry structure or is it a flat fill.
 *
 * Off unless `CONCORD_FRAME_PROBE` is set, so a shipping frame never pays for
 * the copy. The readback is resolved two frames after it is recorded, by which
 * point the fence for the slot that recorded it has certainly been waited on.
 */
struct VulkanFrameProbe {
    VulkanBuffer staging{};
    VkExtent2D extent{};
    /** Frames between captures, or zero when the probe is disabled. */
    u32 interval = 0;
    u32 framesSinceCapture = 0;
    /** Frame counter at the last capture, and whether one is still in flight. */
    u64 captureFrame = 0;
    u64 frameIndex = 0;
    bool awaitingResolve = false;
    /**
     * Wall clock at the previous *report*, so the interval it measures is the
     * one between two captures. Stamping it when a copy is recorded instead
     * measures the two frames between recording and reading, which is not the
     * frame rate at all.
     */
    std::chrono::steady_clock::time_point lastReport{};
    /**
     * Whether the staged frame is already display referred.
     *
     * The probe reads the graded frame, so the values it sees are what reaches
     * the monitor; applying the display transform a second time here would
     * report a picture nobody is looking at.
     */
    bool displayReferred = true;
};

/** Reads the capture interval from the environment and allocates staging. */
bool CreateVulkanFrameProbe(const VulkanContext& context, VkExtent2D extent,
                            VulkanFrameProbe& probe);

void DestroyVulkanFrameProbe(const VulkanContext& context, VulkanFrameProbe& probe) noexcept;

/**
 * Records this frame's readback and advances the capture schedule.
 *
 * The image is left in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` and the output
 * is updated to match, so a caller that composites afterwards transitions from
 * the layout the image is actually in.
 *
 * @return Whether a copy was recorded, i.e. whether the output was modified.
 */
bool RecordVulkanFrameProbe(VkCommandBuffer commandBuffer, VulkanFrameProbe& probe,
                            VkImage image, VkExtent2D extent) noexcept;

/** Reports the captured frame once its fence has certainly been waited on. */
void ResolveVulkanFrameProbe(VulkanFrameProbe& probe) noexcept;

/** Decodes the staged readback and prints its statistics. */
void ReportVulkanFrameProbe(VulkanFrameProbe& probe) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANFRAMEPROBE_H
