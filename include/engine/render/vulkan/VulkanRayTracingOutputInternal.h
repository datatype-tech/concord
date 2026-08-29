// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGOUTPUTINTERNAL_H
#define CONCORD_VULKANRAYTRACINGOUTPUTINTERNAL_H

#include "engine/render/vulkan/VulkanRayTracingOutput.h"

namespace Concord {

/** Creates and binds one device-local RT storage image. */
bool CreateVulkanRayTracingOutputImage(const VulkanContext& context, VkExtent2D extent,
                                       VulkanRayTracingOutput& output);

/** Releases one RT storage image, including partially-created resources. */
void DestroyVulkanRayTracingOutputImage(const VulkanContext& context,
                                        VulkanRayTracingOutput& output) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGOUTPUTINTERNAL_H
