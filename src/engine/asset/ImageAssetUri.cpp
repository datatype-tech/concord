// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ImageAsset.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Concord {
namespace {

int Base64Value(char value) noexcept
{
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}
int HexValue(char value) noexcept
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
bool DecodeBase64(std::string_view input, std::vector<std::byte>& output)
{
    std::string compact;
    constexpr usize maxEncoded = (kImageMaxBytes / 3u) * 4u + 4u;
    try {
        compact.reserve(std::min(input.size(), maxEncoded));
        for (const char value : input) {
            if (std::isspace(static_cast<unsigned char>(value)) != 0) continue;
            if (compact.size() >= maxEncoded) return false;
            compact.push_back(value);
        }
    } catch (...) {
        return false;
    }
    if (compact.empty()) return false;
    usize padding = 0;
    while (padding < compact.size() && compact[compact.size() - padding - 1] == '=') ++padding;
    if (padding > 2 || compact.size() % 4 == 1 || (padding != 0 && compact.size() % 4 != 0)) return false;
    const usize dataSize = compact.size() - padding;
    for (usize index = 0; index < dataSize; ++index) {
        if (Base64Value(compact[index]) < 0) return false;
    }
    auto Append = [&](u8 value) {
        if (output.size() >= kImageMaxBytes) return false;
        output.push_back(static_cast<std::byte>(value));
        return true;
    };
    for (usize index = 0; index + 4 <= compact.size(); index += 4) {
        const int a = Base64Value(compact[index]);
        const int b = Base64Value(compact[index + 1]);
        const char cChar = compact[index + 2], dChar = compact[index + 3];
        const int c = cChar == '=' ? 0 : Base64Value(cChar);
        const int d = dChar == '=' ? 0 : Base64Value(dChar);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (cChar == '=' && dChar != '=') ||
            ((cChar == '=' || dChar == '=') && index + 4 != compact.size())) return false;
        if (!Append(static_cast<u8>((a << 2) | (b >> 4)))) return false;
        if (cChar != '=' && !Append(static_cast<u8>((b << 4) | (c >> 2)))) return false;
        if (dChar != '=' && !Append(static_cast<u8>((c << 6) | d))) return false;
        if ((cChar == '=' && (b & 15) != 0) || (dChar == '=' && (c & 3) != 0)) return false;
    }
    const usize remainder = dataSize % 4;
    if (padding == 0 && remainder >= 2) {
        const usize index = dataSize - remainder;
        const int a = Base64Value(compact[index]), b = Base64Value(compact[index + 1]);
        if (a < 0 || b < 0 || !Append(static_cast<u8>((a << 2) | (b >> 4))) ||
            (remainder == 2 && (b & 15) != 0)) return false;
        if (remainder == 3) {
            const int c = Base64Value(compact[index + 2]);
            if (c < 0 || (c & 3) != 0 ||
                !Append(static_cast<u8>((b << 4) | (c >> 2)))) return false;
        }
    }
    return !output.empty();
}
bool DecodePercent(std::string_view input, std::vector<std::byte>& output)
{
    output.reserve(std::min(input.size(), kImageMaxBytes));
    for (usize index = 0; index < input.size(); ++index) {
        if (input[index] != '%') {
            if (output.size() >= kImageMaxBytes) return false;
            output.push_back(static_cast<std::byte>(static_cast<unsigned char>(input[index])));
            continue;
        }
        if (index + 2 >= input.size()) return false;
        const int high = HexValue(input[index + 1]);
        const int low = HexValue(input[index + 2]);
        if (high < 0 || low < 0) return false;
        if (output.size() >= kImageMaxBytes) return false;
        output.push_back(static_cast<std::byte>((high << 4) | low));
        index += 2;
    }
    return true;
}
ImageLoadResult UriFailure()
{
    return ImageLoadResult{.error = ImageLoadError::InvalidUri,
                           .message = ImageLoadErrorMessage(ImageLoadError::InvalidUri)};
}

} // namespace

ImageLoadResult ImageLoader::LoadUri(std::string_view uri,
                                     const std::filesystem::path& baseDirectory)
{
    if (uri.empty()) return UriFailure();
    if (uri.substr(0, 5) != "data:") {
        std::vector<std::byte> decoded;
        try {
            if (!DecodePercent(uri, decoded) || decoded.empty()) return UriFailure();
            const std::string pathString(reinterpret_cast<const char*>(decoded.data()), decoded.size());
            const std::filesystem::path relative{pathString};
            return Load(relative.is_absolute() ? relative : baseDirectory / relative);
        } catch (...) {
            return ImageLoadResult{.error = ImageLoadError::AllocationFailure,
                                   .message = ImageLoadErrorMessage(ImageLoadError::AllocationFailure)};
        }
    }
    const usize comma = uri.find(',');
    if (comma == std::string_view::npos || comma <= 5) return UriFailure();
    const std::string_view metadata = uri.substr(5, comma - 5);
    const std::string_view payload = uri.substr(comma + 1);
    const bool base64 = metadata.size() >= 7 &&
                        metadata.substr(metadata.size() - 7) == ";base64";
    std::vector<std::byte> bytes;
    try {
        if (base64 ? !DecodeBase64(payload, bytes) : !DecodePercent(payload, bytes)) {
            return UriFailure();
        }
    } catch (...) {
        return ImageLoadResult{.error = ImageLoadError::AllocationFailure,
                               .message = ImageLoadErrorMessage(ImageLoadError::AllocationFailure)};
    }
    return Decode(bytes, metadata);
}

} // namespace Concord
