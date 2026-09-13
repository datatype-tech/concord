// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPOSTPROCESSINTERNAL_H
#define CONCORD_VULKANPOSTPROCESSINTERNAL_H

#include "engine/render/vulkan/VulkanPostProcess.h"

namespace Concord {

/** Creates and binds one device-local graded colour target. */
bool CreateVulkanPostProcessImage(const VulkanContext& context, VkExtent2D extent,
                                  VulkanPostProcess& slot);

/** Releases one graded colour target, including partially-created resources. */
void DestroyVulkanPostProcessImage(const VulkanContext& context,
                                   VulkanPostProcess& slot) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANPOSTPROCESSINTERNAL_H
