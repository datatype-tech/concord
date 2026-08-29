// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ImageAsset.h"

#include <utility>

namespace Concord {
namespace {

std::string Key(std::string_view uri, const std::filesystem::path& baseDirectory)
{
    return baseDirectory.lexically_normal().generic_string() + "\n" + std::string(uri);
}

} // namespace

bool ImageAssetCache::Ensure(std::string_view uri,
                             const std::filesystem::path& baseDirectory)
{
    std::string key;
    try {
        key = Key(uri, baseDirectory);
    } catch (...) {
        return false;
    }
    for (const Entry& entry : entries) {
        if (entry.key == key && entry.image != nullptr) return true;
    }
    ImageLoadResult result = ImageLoader::LoadUri(uri, baseDirectory);
    if (!result.Succeeded()) return false;
    try {
        auto image = std::make_shared<ImageAsset>(std::move(result.image));
        entries.push_back(Entry{key, std::move(image)});
    } catch (...) {
        return false;
    }
    return true;
}

const ImageAsset* ImageAssetCache::Find(std::string_view uri,
                                        const std::filesystem::path& baseDirectory) const noexcept
{
    try {
        const std::string key = Key(uri, baseDirectory);
        for (const Entry& entry : entries) {
            if (entry.key == key && entry.image != nullptr) return entry.image.get();
        }
    } catch (...) {
        return nullptr;
    }
    return nullptr;
}

} // namespace Concord
