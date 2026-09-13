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

bool CreateVulkanFrameProbe(const VulkanContext& context, VkExtent2D extent,
                            VulkanFrameProbe& probe)
{
    // Idempotent: an earlier allocation must not be orphaned by a second call,
    // which is what makes re-arming on resize a single step.
    DestroyVulkanFrameProbe(context, probe);
    probe.interval = ProbeIntervalFromEnvironment();
    probe.extent = extent;
    if (probe.interval == 0 || context.device == VK_NULL_HANDLE || extent.width == 0 ||
        extent.height == 0) {
        return true;
    }
    const VkDeviceSize pixels = static_cast<VkDeviceSize>(extent.width) * extent.height;
    if (pixels > (std::numeric_limits<VkDeviceSize>::max() / kProbeBytesPerPixel)) {
        probe.interval = 0;
        return true;
    }
    VulkanBufferCreateInfo info{};
    info.size = pixels * kProbeBytesPerPixel;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    info.persistentMap = true;
    if (!CreateVulkanBuffer(context, info, probe.staging)) {
        probe.interval = 0;
        return true;
    }
    std::fprintf(stderr,
                 "[Concord] frame probe armed: %ux%u every %u frames\n",
                 extent.width, extent.height, probe.interval);
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
    if (probe.interval == 0 || !probe.staging.IsReady() || image == VK_NULL_HANDLE ||
        extent.width != probe.extent.width || extent.height != probe.extent.height) {
        return false;
    }
    // A capture already in flight is resolved first, so a second one cannot
    // overwrite the buffer before the first has been read.
    if (probe.awaitingResolve || ++probe.framesSinceCapture < probe.interval) {
        return false;
    }
    probe.framesSinceCapture = 0;
    // The image is already a blit source by the time this runs, so the barrier
    // is an access ordering rather than a layout change.
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           probe.staging.buffer, 1, &region);
    probe.captureFrame = probe.frameIndex;
    probe.awaitingResolve = true;
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

} // namespace Concord
