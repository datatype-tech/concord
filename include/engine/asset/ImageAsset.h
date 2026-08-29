// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_IMAGEASSET_H
#define CONCORD_ASSET_IMAGEASSET_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Concord {

inline constexpr u32 kImageChannelsRgba = 4;
inline constexpr u32 kImageMaxDimension = 16384;
inline constexpr usize kImageMaxBytes = 256u * 1024u * 1024u;

/** Decoding failures returned by the image loader. */
enum class ImageLoadError {
    None,
    EmptyInput,
    FileUnavailable,
    InvalidUri,
    UnsupportedFormat,
    DecodeFailed,
    InvalidDimensions,
    TooLarge,
    AllocationFailure,
};

/** Immutable RGBA8 pixels decoded from a PNG, JPEG, or stb-supported image. */
struct CENGINE_API ImageAsset {
    u32 width = 0;
    u32 height = 0;
    u32 channels = kImageChannelsRgba;
    std::vector<u8> pixels;

    /** Whether dimensions and packed pixel storage agree. */
    [[nodiscard]] bool IsValid() const noexcept;
    /** Returns the packed RGBA8 bytes without transferring ownership. */
    [[nodiscard]] std::span<const u8> PixelBytes() const noexcept { return pixels; }
};

/** Value result carrying either a decoded image or a stable diagnostic. */
struct CENGINE_API ImageLoadResult {
    ImageAsset image{};
    ImageLoadError error = ImageLoadError::None;
    std::string message;

    /** Whether decoding produced a complete image. */
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == ImageLoadError::None && image.IsValid();
    }
};

/** Loads encoded image bytes and resolves external or data URIs. */
class CENGINE_API ImageLoader {
public:
    /** Decodes a memory block, forcing the result to RGBA8. */
    [[nodiscard]] static ImageLoadResult Decode(std::span<const std::byte> encoded,
                                                std::string_view hint = {});
    /** Reads and decodes one image file. */
    [[nodiscard]] static ImageLoadResult Load(const std::filesystem::path& path);
    /** Resolves a glTF-style URI relative to a model directory. */
    [[nodiscard]] static ImageLoadResult LoadUri(std::string_view uri,
                                                 const std::filesystem::path& baseDirectory = {});
};

/** Returns a stable human-readable message for an image error. */
[[nodiscard]] CENGINE_API const char* ImageLoadErrorMessage(ImageLoadError error) noexcept;

/** Small immutable CPU cache keyed by a URI and its base directory. */
struct CENGINE_API ImageAssetCache {
    struct Entry {
        std::string key;
        std::shared_ptr<const ImageAsset> image;
    };
    std::vector<Entry> entries;

    /** Loads and stores an image unless the key is already cached. */
    bool Ensure(std::string_view uri, const std::filesystem::path& baseDirectory = {});
    /** Returns a cached image, or nullptr when the URI is absent. */
    [[nodiscard]] const ImageAsset* Find(
        std::string_view uri, const std::filesystem::path& baseDirectory = {}) const noexcept;
    /** Releases all decoded images. */
    void Clear() noexcept { entries.clear(); }
};

} // namespace Concord

#endif // CONCORD_ASSET_IMAGEASSET_H
