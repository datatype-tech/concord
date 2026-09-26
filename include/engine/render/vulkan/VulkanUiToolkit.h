// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_VULKANUITOOLKIT_H
#define CONCORD_VULKANUITOOLKIT_H
#include "engine/render/vulkan/VulkanContext.h"
#include "engine/render/vulkan/VulkanDepthBuffer.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"
#include "engine/render/vulkan/VulkanUiPipeline.h"

namespace Concord {
class UiToolkit;
/** A frame-local render target sampled by the docking UI after the scene pass. */
struct VulkanUiViewport {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet texture = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D extent{};
    VulkanDepthBuffer depth{};
};
/** Optional renderer state; never initialized for ordinary games. */
struct VulkanUiToolkit {
    UiToolkit* ui = nullptr;
    VkSampler sampler = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool initialized = false;
    VulkanUiPipeline pipeline{};
    VulkanUiViewport viewports[kMaxFramesInFlight]{};
};
bool CreateVulkanUiToolkit(const VulkanContext& context, VkFormat format, UiToolkit& ui, VulkanUiToolkit& toolkit);
/** Rebuilds format-dependent resources before the next UI frame creates texture references. */
bool RefreshVulkanUiFormat(const VulkanContext& context, VkFormat format, VulkanUiToolkit& toolkit);
void DestroyVulkanUiToolkit(const VulkanContext& context, VulkanUiToolkit& toolkit);
bool EnsureVulkanUiViewport(const VulkanContext& context, VulkanUiToolkit& toolkit, u32 slot);
void DestroyVulkanUiViewport(const VulkanContext& context, VulkanUiViewport& viewport);
void RecordVulkanUiToolkit(VkCommandBuffer command, VulkanUiToolkit& toolkit, u32 slot,
                           VkImageView target, VkExtent2D extent);
}
#endif
