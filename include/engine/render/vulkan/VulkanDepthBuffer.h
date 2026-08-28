// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANDEPTHBUFFER_H
#define CONCORD_VULKANDEPTHBUFFER_H

#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/** Depth image owned by one frame slot for one swapchain extent. */
struct VulkanDepthBuffer {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

/** Creates a device-local depth attachment for the requested extent. */
bool CreateVulkanDepthBuffer(const VulkanContext& context, VulkanDepthBuffer& depth,
                             VkExtent2D extent);

/** Releases the depth view, image and memory in dependency order. */
void DestroyVulkanDepthBuffer(const VulkanContext& context, VulkanDepthBuffer& depth);

} // namespace Concord

#endif // CONCORD_VULKANDEPTHBUFFER_H
