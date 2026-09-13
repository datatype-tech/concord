// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTEXTUREKEY_H
#define CONCORD_VULKANTEXTUREKEY_H

#include <filesystem>
#include <string>
#include <string_view>

namespace Concord {

/**
 * Builds the cache key a texture URI and base directory resolve to.
 *
 * Inline and dependency-free on purpose. The raster, skinned and ray tracing
 * paths all construct keys, and a mismatch between them would make one path
 * silently miss textures another loaded. Keeping a single definition in a
 * light header lets the ray-tracing units share it without dragging the whole
 * texture cache into their translation units.
 *
 * @return The key, or an empty string when the base directory cannot be
 *         normalized or the concatenation cannot be allocated.
 */
[[nodiscard]] inline std::string MakeVulkanTextureCacheKey(
    std::string_view uri, const std::filesystem::path& baseDirectory) noexcept
{
    try {
        std::string base = baseDirectory.lexically_normal().generic_string();
        // Folding "assets/sub/.." leaves a trailing separator, which would key
        // the same folder under two spellings and decode its textures twice.
        // A lone root separator is kept, since it is the whole path.
        while (base.size() > 1 && (base.back() == '/' || base.back() == '\\')) {
            base.pop_back();
        }
        return base + "\n" + std::string(uri);
    } catch (...) {
        return {};
    }
}

} // namespace Concord

#endif // CONCORD_VULKANTEXTUREKEY_H
