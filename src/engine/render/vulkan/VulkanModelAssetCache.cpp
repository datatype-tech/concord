// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAssetCache.h"

#include <utility>

namespace Concord {

bool VulkanModelAssetCache::Ensure(const VulkanContext& context,
                                   std::shared_ptr<const ModelAsset> source)
{
    if (!source || !source->IsValid()) {
        return false;
    }
    if (Find(source.get()) != nullptr) {
        return true;
    }
    try {
        entries.emplace_back();
    } catch (...) {
        return false;
    }
    VulkanModelAssetEntry& entry = entries.back();
    entry.source = std::move(source);
    if (!CreateVulkanModelAsset(context, *entry.source, entry.gpu)) {
        entries.pop_back();
        return false;
    }
    return true;
}

const VulkanModelAsset* VulkanModelAssetCache::Find(const ModelAsset* source) const noexcept
{
    if (source == nullptr) {
        return nullptr;
    }
    for (const VulkanModelAssetEntry& entry : entries) {
        if (entry.source.get() == source && entry.gpu.IsReady()) {
            return &entry.gpu;
        }
    }
    return nullptr;
}

void VulkanModelAssetCache::Clear(const VulkanContext& context) noexcept
{
    for (VulkanModelAssetEntry& entry : entries) {
        DestroyVulkanModelAsset(context, entry.gpu);
    }
    entries.clear();
}

} // namespace Concord
