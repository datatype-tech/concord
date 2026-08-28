// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameDataResources.h"

namespace Concord {

bool UploadVulkanFrameData(VulkanFrameDataResources& resources, u32 frameIndex,
                           const RenderFrameData& data) noexcept
{
    if (frameIndex >= kMaxFramesInFlight) {
        return false;
    }
    return UploadVulkanBuffer(resources.buffers[frameIndex], RenderFrameDataBytes(data));
}

} // namespace Concord
