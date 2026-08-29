// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANMODELASSETCACHE_H
#define CONCORD_VULKANMODELASSETCACHE_H

#include "engine/render/vulkan/VulkanModelAsset.h"

#include <memory>
#include <vector>

namespace Concord {

/** One immutable CPU asset and its device-side upload. */
struct VulkanModelAssetEntry {
    std::shared_ptr<const ModelAsset> source;
    VulkanModelAsset gpu{};
};

/** Owns model uploads for the lifetime of one Vulkan device. */
struct VulkanModelAssetCache {
    std::vector<VulkanModelAssetEntry> entries;

    /** Finds or uploads an asset; returns false when upload cannot complete. */
    bool Ensure(const VulkanContext& context, std::shared_ptr<const ModelAsset> source);

    /** Resolves a source asset to an already uploaded resource. */
    [[nodiscard]] const VulkanModelAsset* Find(const ModelAsset* source) const noexcept;

    /** Destroys all device resources and releases retained source assets. */
    void Clear(const VulkanContext& context) noexcept;
};

} // namespace Concord

#endif // CONCORD_VULKANMODELASSETCACHE_H
