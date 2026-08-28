// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGSCENEGPUSUPPORT_H
#define CONCORD_VULKANRAYTRACINGSCENEGPUSUPPORT_H

#include "engine/core/Types.h"

#include <vulkan/vulkan.h>

namespace ConcordTest {

/** Creates a Vulkan 1.3 instance for the optional scene smoke test. */
bool CreateInstance(VkInstance& instance);

/** Finds a graphics-capable queue family, or returns 0xffffffff. */
Concord::u32 FindGraphicsFamily(VkPhysicalDevice device);

/** Creates a logical device with the required KHR acceleration features. */
bool CreateRayTracingDevice(VkPhysicalDevice physicalDevice, Concord::u32 queueFamily,
                            VkDevice& device);

} // namespace ConcordTest

#endif // CONCORD_VULKANRAYTRACINGSCENEGPUSUPPORT_H
