// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipeline.h"

namespace Concord {

void DestroyVulkanRayTracingPipeline(const VulkanContext& context,
                                     VulkanRayTracingPipeline& pipeline) noexcept
{
    const VkDevice device = pipeline.device != VK_NULL_HANDLE ? pipeline.device : context.device;
    if (device != VK_NULL_HANDLE && pipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    }
    if (device != VK_NULL_HANDLE && pipeline.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    }
    if (device != VK_NULL_HANDLE && pipeline.outputLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, pipeline.outputLayout, nullptr);
    }
    DestroyVulkanBuffer(context, pipeline.sbt);
    pipeline = {};
}

} // namespace Concord
