// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKENDDEBUG_H
#define CONCORD_VULKANRENDERBACKENDDEBUG_H

#include "engine/core/Vec3.h"
#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/** Begins a validation-only command-buffer label when the extension is present. */
void BeginVulkanDebugLabel(const VulkanContext& context, VkCommandBuffer commandBuffer,
                           const char* name, Vec3 color) noexcept;

/** Ends the current validation-only command-buffer label. */
void EndVulkanDebugLabel(const VulkanContext& context, VkCommandBuffer commandBuffer) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRENDERBACKENDDEBUG_H
