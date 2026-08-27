// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRESULT_H
#define CONCORD_VULKANRESULT_H

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * Reports a failed Vulkan call and returns false.
 *
 * Bring-up code unwinds by returning false rather than throwing, so this
 * folds the diagnostic and the return value into one expression:
 * `return VulkanFailed("vkCreateDevice", result);`
 */
bool VulkanFailed(const char* what, VkResult result);

} // namespace Concord

#endif // CONCORD_VULKANRESULT_H
