// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMEPROBE_H
#define CONCORD_VULKANFRAMEPROBE_H

#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"

#include <chrono>
#include <string>

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
 *
 * Setting `CONCORD_FRAME_PROBE_PNG` to a path additionally writes each capture
 * out as an image, with the frame index spliced in ahead of the extension. The
 * statistics say whether a frame is crushed or flat but never what it looks
 * like, and a screen grab answers that only as long as nothing on the desktop
 * is in front of the window.
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
    /**
     * Where a requested still is to be written, empty when none was asked for.
     *
     * A still is not the periodic probe with a different destination: it is
     * asked for by name, it must land on the very next frame rather than
     * whenever the schedule next comes round, and the caller is waiting on the
     * file rather than reading a log. It therefore bypasses `interval`
     * entirely -- including the zero that means the periodic probe is off,
     * which is the normal state of an application that only ever wants stills.
     */
    std::string stillPath{};
    /** Whether the frame now in flight is carrying the requested still. */
    bool stillRecorded = false;
};

/** Reads the capture interval from the environment and allocates staging. */
bool CreateVulkanFrameProbe(const VulkanContext& context, VkExtent2D extent,
                            VulkanFrameProbe& probe);

/**
 * Asks for the next recorded frame to be written to `path` as a PNG.
 *
 * Allocates the staging buffer when the periodic probe left it unallocated, so
 * that an application which never sets `CONCORD_FRAME_PROBE` still pays for the
 * readback only on the frames it actually asks to keep.
 *
 * @return Whether the request was armed; false leaves no file to wait for.
 */
bool RequestVulkanFrameProbeStill(const VulkanContext& context, VkExtent2D extent,
                                  VulkanFrameProbe& probe, const char* path);

/**
 * Writes a requested still, which the caller must already have waited out.
 *
 * Separate from ResolveVulkanFrameProbe because the two answer to different
 * clocks: the periodic probe reads two frames later so that it never stalls a
 * live loop, while a still is the whole point of the frame that produced it and
 * its caller has nothing to do until the file exists.
 *
 * @return Whether a still was pending and has now been written.
 */
bool ResolveVulkanFrameProbeStill(VulkanFrameProbe& probe) noexcept;

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

/** Decodes the staged readback and writes it to `path`; no statistics. */
bool WriteVulkanFrameProbeImage(const VulkanFrameProbe& probe, const char* path) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANFRAMEPROBE_H
