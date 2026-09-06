// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDebugOverlay.h"

namespace Concord {

void DestroyVulkanDebugOverlay(const VulkanContext& context, VulkanDebugOverlay& overlay)
{
    if (context.device == VK_NULL_HANDLE) {
        overlay = {};
        return;
    }
    for (VulkanBuffer& buffer : overlay.vertices) {
        DestroyVulkanBuffer(context, buffer);
    }
    if (overlay.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, overlay.pipeline, nullptr);
    }
    if (overlay.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, overlay.layout, nullptr);
    }
    if (overlay.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, overlay.descriptorPool, nullptr);
    }
    if (overlay.descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, overlay.descriptorLayout, nullptr);
    }
    if (overlay.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, overlay.sampler, nullptr);
    }
    if (overlay.atlasView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, overlay.atlasView, nullptr);
    }
    if (overlay.atlasImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, overlay.atlasImage, nullptr);
    }
    if (overlay.atlasMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, overlay.atlasMemory, nullptr);
    }
    overlay = {};
}

} // namespace Concord
