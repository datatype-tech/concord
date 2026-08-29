// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTextureCache.h"

#include <utility>

namespace Concord {
namespace {

std::string MakeKey(std::string_view uri,
                    const std::filesystem::path& baseDirectory)
{
    return baseDirectory.lexically_normal().generic_string() + "\n" + std::string(uri);
}

bool CreateFallback(const VulkanContext& context, VulkanTexture& texture,
                    std::shared_ptr<const ImageAsset>& source)
{
    auto image = std::make_shared<ImageAsset>();
    image->width = 1;
    image->height = 1;
    image->pixels = {255, 255, 255, 255};
    if (!CreateVulkanTexture(context, *image, texture)) return false;
    source = std::move(image);
    return true;
}

void InvalidateTextureUpload(VulkanTexture& texture) noexcept
{
    if (texture.uploadRecorded && !texture.uploadSubmitted) {
        texture.uploadRecorded = false;
        texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

} // namespace

bool VulkanTextureCache::Initialize(const VulkanContext& context)
{
    if (IsReady()) return true;
    Clear(context);
    if (context.device == VK_NULL_HANDLE || context.physicalDevice == VK_NULL_HANDLE) {
        return false;
    }
    if (!CreateFallback(context, fallbackTexture, fallbackSource)) {
        Clear(context);
        return false;
    }
    device = context.device;
    return true;
}

bool VulkanTextureCache::Ensure(const VulkanContext& context, std::string_view uri,
                                const std::filesystem::path& baseDirectory)
{
    if (uri.empty()) return IsReady();
    if (!IsReady() && !Initialize(context)) return false;
    std::string key;
    try {
        key = MakeKey(uri, baseDirectory);
    } catch (...) {
        return false;
    }
    for (const VulkanTextureCacheEntry& entry : entries) {
        if (entry.key == key && entry.texture.IsReady()) return true;
    }
    ImageLoadResult decoded = ImageLoader::LoadUri(uri, baseDirectory);
    if (!decoded.Succeeded()) return false;
    VulkanTextureCacheEntry entry;
    try {
        entry.source = std::make_shared<ImageAsset>(std::move(decoded.image));
    } catch (...) {
        return false;
    }
    if (!CreateVulkanTexture(context, *entry.source, entry.texture)) return false;
    entry.key = key;
    try {
        entries.emplace_back(std::move(entry));
    } catch (...) {
        DestroyVulkanTexture(context, entry.texture);
        return false;
    }
    return true;
}

bool VulkanTextureCache::RecordUploads(VkCommandBuffer commandBuffer) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !IsReady()) return false;
    bool success = true;
    if (fallbackTexture.IsReady() && !fallbackTexture.IsUploaded()) {
        success = RecordVulkanTextureUpload(commandBuffer, fallbackTexture) && success;
    }
    for (VulkanTextureCacheEntry& entry : entries) {
        if (entry.texture.IsReady() && !entry.texture.IsUploaded()) {
            success = RecordVulkanTextureUpload(commandBuffer, entry.texture) && success;
        }
    }
    return success;
}

const VulkanTexture* VulkanTextureCache::Find(
    std::string_view uri, const std::filesystem::path& baseDirectory) const noexcept
{
    const VulkanTexture* fallback = Fallback();
    if (uri.empty()) return fallback;
    try {
        const std::string key = MakeKey(uri, baseDirectory);
        for (const VulkanTextureCacheEntry& entry : entries) {
            if (entry.key == key && entry.texture.IsUploaded()) return &entry.texture;
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

void VulkanTextureCache::CommitUploads() noexcept
{
    if (fallbackTexture.uploadRecorded) fallbackTexture.uploadSubmitted = true;
    for (VulkanTextureCacheEntry& entry : entries) {
        if (entry.texture.uploadRecorded) entry.texture.uploadSubmitted = true;
    }
}

void VulkanTextureCache::InvalidateUploads() noexcept
{
    InvalidateTextureUpload(fallbackTexture);
    for (VulkanTextureCacheEntry& entry : entries) InvalidateTextureUpload(entry.texture);
}

void VulkanTextureCache::Clear(const VulkanContext& context) noexcept
{
    DestroyVulkanTexture(context, fallbackTexture);
    fallbackSource.reset();
    for (VulkanTextureCacheEntry& entry : entries) {
        DestroyVulkanTexture(context, entry.texture);
    }
    entries.clear();
    device = VK_NULL_HANDLE;
}

} // namespace Concord
