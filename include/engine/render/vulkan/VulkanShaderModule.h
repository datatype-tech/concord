// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADERMODULE_H
#define CONCORD_VULKANSHADERMODULE_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanContext.h"

#include <vector>

namespace Concord {

/** Reads a staged SPIR-V module using the engine's standard search paths. */
std::vector<u32> ReadVulkanShaderCode(const char* fileName);

/** Creates a shader module, or returns null for invalid/empty code. */
VkShaderModule CreateVulkanShaderModule(const VulkanContext& context,
                                        const std::vector<u32>& code);

} // namespace Concord

#endif // CONCORD_VULKANSHADERMODULE_H
