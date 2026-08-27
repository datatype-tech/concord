// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANINSTANCE_H
#define CONCORD_VULKANINSTANCE_H

#include "engine/render/vulkan/VulkanContext.h"

namespace Concord {

/**
 * Creates the Vulkan instance, requesting the surface extensions SDL needs.
 *
 * @param enableValidation Requests `VK_LAYER_KHRONOS_validation`; silently
 *        skipped when the layer is not installed, with the outcome recorded
 *        in `context.validationEnabled`.
 * @return False when instance creation failed.
 */
bool CreateVulkanInstance(VulkanContext& context, bool enableValidation);

/** Destroys the instance and clears the handle. */
void DestroyVulkanInstance(VulkanContext& context);

} // namespace Concord

#endif // CONCORD_VULKANINSTANCE_H
