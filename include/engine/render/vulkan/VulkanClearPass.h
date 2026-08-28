// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANCLEARPASS_H
#define CONCORD_VULKANCLEARPASS_H

#include "engine/core/Vec3.h"

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * Records a dynamic-rendering pass that only clears the target.
 *
 * Uses `vkCmdBeginRendering` rather than a VkRenderPass so no framebuffer or
 * render-pass object has to be built or invalidated when the swapchain is
 * rebuilt.
 *
 * @param color Clear colour in linear space.
 */
void RecordClearPass(VkCommandBuffer commandBuffer, VkImageView target, VkExtent2D extent,
                     Vec3 color, VkImageView depthTarget = VK_NULL_HANDLE);

} // namespace Concord

#endif // CONCORD_VULKANCLEARPASS_H
