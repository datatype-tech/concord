// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBuffer.h"

int main()
{
    Concord::VulkanBuffer buffer{};
    if (buffer.IsBound() || buffer.IsMapped() || buffer.HasDeviceAddress() || buffer.IsReady()) {
        return 1;
    }
    buffer.buffer = reinterpret_cast<VkBuffer>(1);
    buffer.memory = reinterpret_cast<VkDeviceMemory>(2);
    buffer.device = reinterpret_cast<VkDevice>(3);
    if (!buffer.IsBound() || buffer.IsMapped() || buffer.HasDeviceAddress() || buffer.IsReady()) {
        return 1;
    }
    buffer.mapped = reinterpret_cast<void*>(4);
    buffer.deviceAddress = 0x1000;
    return buffer.IsMapped() && buffer.HasDeviceAddress() && buffer.IsReady() ? 0 : 1;
}
