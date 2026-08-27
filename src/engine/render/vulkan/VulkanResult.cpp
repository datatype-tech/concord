// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanResult.h"

#include <cstdio>

namespace Concord {

bool VulkanFailed(const char* what, VkResult result)
{
    std::fprintf(stderr, "[Concord] %s failed (VkResult %d)\n", what, static_cast<int>(result));
    return false;
}

} // namespace Concord
