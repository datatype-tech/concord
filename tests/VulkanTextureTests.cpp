// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTexture.h"

#include <cstddef>

int main()
{
    using namespace Concord;
    VulkanTexture texture{};
    if (texture.IsReady() || texture.IsUploaded() || texture.image != VK_NULL_HANDLE ||
        texture.format != VK_FORMAT_UNDEFINED || texture.layout != VK_IMAGE_LAYOUT_UNDEFINED ||
        kVulkanTextureBinding != 0 || kVulkanTextureFormat != VK_FORMAT_R8G8B8A8_SRGB) {
        return 1;
    }
    ImageAsset invalid{};
    if (CreateVulkanTexture(VulkanContext{}, invalid, texture)) {
        return 1;
    }
    return RecordVulkanTextureUpload(VK_NULL_HANDLE, texture) ? 1 : 0;
}
