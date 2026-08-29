// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ImageAsset.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stb_image.h>
#include <utility>

namespace Concord {
namespace {

ImageLoadResult Failure(ImageLoadError error, std::string message)
{
    ImageLoadResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

bool PixelCount(u32 width, u32 height, usize& bytes) noexcept
{
    if (width == 0 || height == 0 || width > kImageMaxDimension ||
        height > kImageMaxDimension || height > std::numeric_limits<usize>::max() /
                                            (static_cast<usize>(width) * kImageChannelsRgba)) {
        return false;
    }
    bytes = static_cast<usize>(width) * height * kImageChannelsRgba;
    return bytes <= kImageMaxBytes;
}

ImageLoadError DimensionError(int width, int height) noexcept
{
    if (width <= 0 || height <= 0) return ImageLoadError::InvalidDimensions;
    usize bytes = 0;
    return PixelCount(static_cast<u32>(width), static_cast<u32>(height), bytes)
               ? ImageLoadError::None
               : ImageLoadError::TooLarge;
}

} // namespace

bool ImageAsset::IsValid() const noexcept
{
    usize expected = 0;
    return channels == kImageChannelsRgba && PixelCount(width, height, expected) &&
           pixels.size() == expected;
}

const char* ImageLoadErrorMessage(ImageLoadError error) noexcept
{
    switch (error) {
    case ImageLoadError::None: return "none";
    case ImageLoadError::EmptyInput: return "image input is empty";
    case ImageLoadError::FileUnavailable: return "image file is unavailable";
    case ImageLoadError::InvalidUri: return "image URI is invalid";
    case ImageLoadError::UnsupportedFormat: return "image format is unsupported";
    case ImageLoadError::DecodeFailed: return "image decoding failed";
    case ImageLoadError::InvalidDimensions: return "image dimensions are invalid";
    case ImageLoadError::TooLarge: return "image exceeds the configured size limit";
    case ImageLoadError::AllocationFailure: return "image allocation failed";
    }
    return "unknown image error";
}

ImageLoadResult ImageLoader::Decode(std::span<const std::byte> encoded,
                                    std::string_view hint)
{
    if (encoded.empty() || encoded.size_bytes() > std::numeric_limits<int>::max()) {
        return Failure(encoded.empty() ? ImageLoadError::EmptyInput
                                       : ImageLoadError::TooLarge,
                       ImageLoadErrorMessage(encoded.empty() ? ImageLoadError::EmptyInput
                                                              : ImageLoadError::TooLarge));
    }
    int width = 0, height = 0, sourceChannels = 0;
    if (stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()),
                              static_cast<int>(encoded.size_bytes()), &width, &height,
                              &sourceChannels) == 0) {
        return Failure(ImageLoadError::DecodeFailed,
                       ImageLoadErrorMessage(ImageLoadError::DecodeFailed));
    }
    const ImageLoadError dimensionError = DimensionError(width, height);
    if (dimensionError != ImageLoadError::None) {
        return Failure(dimensionError, ImageLoadErrorMessage(dimensionError));
    }
    stbi_uc* decoded = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size_bytes()),
        &width, &height, &sourceChannels, static_cast<int>(kImageChannelsRgba));
    if (decoded == nullptr) {
        std::string message = ImageLoadErrorMessage(ImageLoadError::DecodeFailed);
        if (!hint.empty()) message += " (" + std::string(hint) + ")";
        return Failure(ImageLoadError::DecodeFailed, std::move(message));
    }
    usize bytes = 0;
    if (!PixelCount(static_cast<u32>(width), static_cast<u32>(height), bytes)) {
        stbi_image_free(decoded);
        return Failure(ImageLoadError::TooLarge,
                       ImageLoadErrorMessage(ImageLoadError::TooLarge));
    }
    ImageLoadResult result;
    try {
        result.image.width = static_cast<u32>(width);
        result.image.height = static_cast<u32>(height);
        result.image.pixels.assign(decoded, decoded + bytes);
    } catch (...) {
        stbi_image_free(decoded);
        return Failure(ImageLoadError::AllocationFailure,
                       ImageLoadErrorMessage(ImageLoadError::AllocationFailure));
    }
    stbi_image_free(decoded);
    return result;
}

ImageLoadResult ImageLoader::Load(const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0) {
        return Failure(ImageLoadError::FileUnavailable,
                       ImageLoadErrorMessage(ImageLoadError::FileUnavailable));
    }
    if (size > kImageMaxBytes || size > std::numeric_limits<usize>::max()) {
        return Failure(ImageLoadError::TooLarge,
                       ImageLoadErrorMessage(ImageLoadError::TooLarge));
    }
    try {
        std::vector<std::byte> bytes(static_cast<usize>(size));
        std::ifstream file(path, std::ios::binary);
        if (!file || !file.read(reinterpret_cast<char*>(bytes.data()),
                                static_cast<std::streamsize>(bytes.size()))) {
            return Failure(ImageLoadError::FileUnavailable,
                           ImageLoadErrorMessage(ImageLoadError::FileUnavailable));
        }
        return Decode(bytes, path.extension().string());
    } catch (...) {
        return Failure(ImageLoadError::AllocationFailure,
                       ImageLoadErrorMessage(ImageLoadError::AllocationFailure));
    }
}

} // namespace Concord
