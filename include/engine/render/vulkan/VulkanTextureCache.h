// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTEXTURECACHE_H
#define CONCORD_VULKANTEXTURECACHE_H

#include "engine/render/vulkan/VulkanTexture.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Concord {

/** One decoded URI and its device-side sampled texture. */
struct VulkanTextureCacheEntry {
    std::string key;
    std::shared_ptr<const ImageAsset> source;
    VulkanTexture texture{};
};

/** Caches imported base-color textures for one Vulkan device lifetime. */
struct VulkanTextureCache {
    std::vector<VulkanTextureCacheEntry> entries;
    std::shared_ptr<const ImageAsset> fallbackSource{};
    VulkanTexture fallbackTexture{};
    VkDevice device = VK_NULL_HANDLE;

    /** Creates the cache's descriptor-compatible 1x1 white fallback. */
    bool Initialize(const VulkanContext& context);
    /** Whether the fallback descriptor layout and image are available. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return fallbackTexture.IsReady() && device == fallbackTexture.device;
    }
    /** Returns the descriptor layout shared by all sampled texture sets. */
    [[nodiscard]] VkDescriptorSetLayout DescriptorLayout() const noexcept
    {
        return IsReady() ? fallbackTexture.descriptorLayout : VK_NULL_HANDLE;
    }
    /** Returns the uploaded fallback texture, or nullptr before initialization. */
    [[nodiscard]] const VulkanTexture* Fallback() const noexcept
    {
        return IsReady() ? &fallbackTexture : nullptr;
    }

    /** Decodes and uploads a URI unless it is already cached. */
    bool Ensure(const VulkanContext& context, std::string_view uri,
                const std::filesystem::path& baseDirectory = {});
    /** Records pending image copies for all cached textures. */
    bool RecordUploads(VkCommandBuffer commandBuffer) noexcept;
    /** Commits uploads after the command buffer has been submitted. */
    void CommitUploads() noexcept;
    /** Rolls back uploads recorded into an abandoned command buffer. */
    void InvalidateUploads() noexcept;
    /** Finds a sampled texture by URI and base directory. */
    [[nodiscard]] const VulkanTexture* Find(
        std::string_view uri, const std::filesystem::path& baseDirectory = {}) const noexcept;
    /** Destroys device resources and releases decoded images. */
    void Clear(const VulkanContext& context) noexcept;
};

} // namespace Concord

#endif // CONCORD_VULKANTEXTURECACHE_H
