// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_VULKANUIPIPELINE_H
#define CONCORD_VULKANUIPIPELINE_H

#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/** Native UI pipeline with full-precision conversion of authored sRGB colors. */
struct VulkanUiPipeline {
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

/** Uses descriptor and push-constant layouts compatible with ImGui's Vulkan backend. */
bool CreateVulkanUiPipeline(const VulkanContext& context, VkFormat format, VulkanUiPipeline& pipeline);
/** Releases pipeline resources after the owning renderer has become idle. */
void DestroyVulkanUiPipeline(const VulkanContext& context, VulkanUiPipeline& pipeline);

} // namespace Concord

#endif // CONCORD_VULKANUIPIPELINE_H
