// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipeline.h"

namespace Concord {

/** Releases all resources owned by the directional shadow pipelines. */
void DestroyVulkanShadowPipeline(const VulkanContext& context,
                                 VulkanShadowPipeline& pipeline) noexcept
{
    if (context.device != VK_NULL_HANDLE) {
        if (pipeline.skinnedDepth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.skinnedDepth, nullptr);
        }
        if (pipeline.skinnedLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.skinnedLayout, nullptr);
        }
        if (pipeline.depth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.depth, nullptr);
        }
        if (pipeline.modelDepth != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, pipeline.modelDepth, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.layout, nullptr);
        }
        if (pipeline.modelLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, pipeline.modelLayout, nullptr);
        }
    }
    pipeline = {};
}

} // namespace Concord
