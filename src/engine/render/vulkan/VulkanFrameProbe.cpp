// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameProbe.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace Concord {
namespace {

/** Bytes one pixel of the ray tracing output format occupies. */
constexpr VkDeviceSize kProbeBytesPerPixel = 8;

constexpr VkDeviceSize kProbeAlignment = 256;

/** Frames between recording a copy and reading it, so its fence has settled. */
constexpr u64 kProbeResolveDelay = 2;

u32 ProbeIntervalFromEnvironment() noexcept
{
    const char* value = std::getenv("CONCORD_FRAME_PROBE");
    if (value == nullptr || *value == '\0') {
        return 0;
    }
    const long parsed = std::strtol(value, nullptr, 10);
    // A bare name with no number means "on": a caller asking for the probe
    // should not have to guess a frame count to get one.
    if (parsed <= 0) {
        return 240;
    }
    return static_cast<u32>(std::min<long>(parsed, 1000000));
}

} // namespace

namespace {

/** Allocates the readback buffer for one frame of `extent`, if it can. */
bool AllocateProbeStaging(const VulkanContext& context, VkExtent2D extent,
                          VulkanFrameProbe& probe)
{
    if (context.device == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
        return false;
    }
    const VkDeviceSize pixels = static_cast<VkDeviceSize>(extent.width) * extent.height;
    if (pixels > (std::numeric_limits<VkDeviceSize>::max() / kProbeBytesPerPixel)) {
        return false;
    }
    VulkanBufferCreateInfo info{};
    info.size = pixels * kProbeBytesPerPixel;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    info.persistentMap = true;
    return CreateVulkanBuffer(context, info, probe.staging);
}

} // namespace

bool CreateVulkanFrameProbe(const VulkanContext& context, VkExtent2D extent,
                            VulkanFrameProbe& probe)
{
    // Idempotent: an earlier allocation must not be orphaned by a second call,
    // which is what makes re-arming on resize a single step.
    DestroyVulkanFrameProbe(context, probe);
    probe.interval = ProbeIntervalFromEnvironment();
    probe.extent = extent;
    if (probe.interval == 0) {
        // Left unallocated rather than merely unused. The periodic probe is off
        // in every frame an application ships, and a readback buffer for a 4K
        // still is a hundred megabytes nobody asked for; a still allocates it
        // on the frame it is requested and this stays free until then.
        return true;
    }
    if (!AllocateProbeStaging(context, extent, probe)) {
        probe.interval = 0;
        return true;
    }
    std::fprintf(stderr,
                 "[Concord] frame probe armed: %ux%u every %u frames\n",
                 extent.width, extent.height, probe.interval);
    return true;
}

bool RequestVulkanFrameProbeStill(const VulkanContext& context, VkExtent2D extent,
                                  VulkanFrameProbe& probe, const char* path)
{
    if (path == nullptr || *path == '\0') {
        return false;
    }
    // The extent is the caller's rather than the probe's own, because a still
    // may be asked for before the probe has ever been armed and because a
    // resize re-arms it: matching here is what keeps the copy below from
    // reading a frame into a buffer sized for a different one.
    if (!probe.staging.IsReady() || probe.extent.width != extent.width ||
        probe.extent.height != extent.height) {
        DestroyVulkanBuffer(context, probe.staging);
        probe.extent = extent;
        if (!AllocateProbeStaging(context, extent, probe)) {
            return false;
        }
    }
    probe.stillPath = path;
    probe.stillRecorded = false;
    return true;
}

void DestroyVulkanFrameProbe(const VulkanContext& context, VulkanFrameProbe& probe) noexcept
{
    DestroyVulkanBuffer(context, probe.staging);
    probe = {};
}

bool RecordVulkanFrameProbe(VkCommandBuffer commandBuffer, VulkanFrameProbe& probe,
                            VkImage image, VkExtent2D extent) noexcept
{
    ++probe.frameIndex;
    const bool still = !probe.stillPath.empty() && !probe.stillRecorded;
    if (!probe.staging.IsReady() || image == VK_NULL_HANDLE ||
        extent.width != probe.extent.width || extent.height != probe.extent.height) {
        return false;
    }
    if (still) {
        // Takes the buffer ahead of the periodic probe. Both cannot have it,
        // and the schedule can wait a frame where a caller blocked on a file
        // cannot: dropping the still instead would hang that caller on an
        // image that is never written.
        probe.awaitingResolve = false;
        probe.framesSinceCapture = 0;
    } else if (probe.interval == 0) {
        return false;
    } else if (probe.awaitingResolve || ++probe.framesSinceCapture < probe.interval) {
        // A capture already in flight is resolved first, so a second one cannot
        // overwrite the buffer before the first has been read.
        return false;
    } else {
        probe.framesSinceCapture = 0;
    }
    // The image is already a blit source by the time this runs, so the barrier
    // is an access ordering rather than a layout change.
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           probe.staging.buffer, 1, &region);
    probe.captureFrame = probe.frameIndex;
    probe.awaitingResolve = !still;
    probe.stillRecorded = still;
    return true;
}

void ResolveVulkanFrameProbe(VulkanFrameProbe& probe) noexcept
{
    if (!probe.awaitingResolve || probe.frameIndex < probe.captureFrame + kProbeResolveDelay) {
        return;
    }
    probe.awaitingResolve = false;
    ReportVulkanFrameProbe(probe);
}

bool ResolveVulkanFrameProbeStill(VulkanFrameProbe& probe) noexcept
{
    if (!probe.stillRecorded) {
        return false;
    }
    const std::string path = std::move(probe.stillPath);
    probe.stillPath.clear();
    probe.stillRecorded = false;
    return WriteVulkanFrameProbeImage(probe, path.c_str());
}

} // namespace Concord
