// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Concord::AssetGltf {

const AssetJson::Value* Member(const AssetJson::Value& value,
                               std::string_view key) noexcept
{
    return value.Find(key);
}

bool IndexValue(const AssetJson::Value* value, usize& result) noexcept
{
    if (!value || !value->Is(AssetJson::Type::Number) || value->number < 0.0) return false;
    const f64 integral = std::floor(value->number);
    if (integral != value->number || integral > static_cast<f64>(SIZE_MAX)) return false;
    result = static_cast<usize>(integral);
    return true;
}

bool SignedIndex(const AssetJson::Value* value, i32& result) noexcept
{
    if (!value || !value->Is(AssetJson::Type::Number) ||
        !std::isfinite(value->number) || std::floor(value->number) != value->number ||
        value->number < static_cast<f64>(std::numeric_limits<i32>::min()) ||
        value->number > static_cast<f64>(std::numeric_limits<i32>::max())) return false;
    result = static_cast<i32>(value->number);
    return true;
}

bool FloatValue(const AssetJson::Value* value, f32& result) noexcept
{
    if (!value || !value->Is(AssetJson::Type::Number) || !std::isfinite(value->number) ||
        value->number < -static_cast<f64>(std::numeric_limits<f32>::max()) ||
        value->number > static_cast<f64>(std::numeric_limits<f32>::max())) return false;
    result = static_cast<f32>(value->number);
    return true;
}

bool Context::Fail(std::string message, usize line, usize column) noexcept
{
    if (error.message.empty()) {
        error.message = std::move(message);
        error.line = line;
        error.column = column;
    }
    return false;
}

ModelLoadResult DecodeDocument(std::string_view text,
                               const std::filesystem::path& baseDirectory,
                               std::span<const std::byte> glbBinary,
                               const ModelLoadOptions& options)
{
    if (!std::isfinite(options.scale)) { ModelLoadResult result; result.error.message = "model scale must be finite"; return result; }
    Context context;
    context.baseDirectory = baseDirectory;
    context.options = options;
    AssetJson::Value document;
    usize line = 0, column = 0;
    std::string parseError;
    if (!AssetJson::Parse(text, document, parseError, line, column)) {
        ModelLoadResult result;
        result.error = {std::move(parseError), line, column};
        return result;
    }
    context.root = &document;
    const AssetJson::Value* asset = Member(document, "asset");
    const AssetJson::Value* versionValue = asset ? Member(*asset, "version") : nullptr;
    const std::string_view version = versionValue ? versionValue->String() : std::string_view{};
    if (!version.empty() && version != "2.0") context.Fail("unsupported glTF version");
    context.asset.name = asset && Member(*asset, "generator")
        ? std::string(Member(*asset, "generator")->String("gltf")) : "gltf";
    if (!context.error.message.empty() || !LoadBuffers(context, glbBinary) ||
        !ReadAccessorMetadata(context) || !ReadGeometry(context) || !ReadScene(context)) {
        ModelLoadResult result;
        result.asset = std::move(context.asset);
        result.error = std::move(context.error);
        if (result.error.message.empty()) result.error.message = "invalid glTF document";
        return result;
    }
    ModelLoadResult result;
    result.asset = std::move(context.asset);
    if (!result.asset.IsValid()) result.error.message = "glTF asset failed structural validation";
    return result;
}

ModelLoadResult DecodeGlb(std::span<const std::byte> bytes,
                          const std::filesystem::path& baseDirectory,
                          const ModelLoadOptions& options)
{
    auto ReadU32 = [bytes](usize offset) -> u32 {
        if (offset + 4 > bytes.size()) return 0;
        u32 value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    if (bytes.size() < 12 || ReadU32(0) != 0x46546C67u || ReadU32(4) != 2u ||
        ReadU32(8) > bytes.size() || ReadU32(8) < 12u) {
        ModelLoadResult result; result.error.message = "invalid GLB header"; return result;
    }
    std::string json;
    std::span<const std::byte> binary;
    usize offset = 12;
    const usize total = ReadU32(8);
    while (offset + 8 <= total) {
        const usize length = ReadU32(offset);
        const u32 type = ReadU32(offset + 4);
        if (length < 8 || offset + length > total) {
            ModelLoadResult result; result.error.message = "invalid GLB chunk"; return result;
        }
        const auto payload = bytes.subspan(offset + 8, length - 8);
        if (type == 0x4E4F534Au && json.empty()) {
            json.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
            while (!json.empty() && (json.back() == '\0' || json.back() == ' ' || json.back() == '\n')) json.pop_back();
        } else if (type == 0x004E4942u && binary.empty()) binary = payload;
        offset += length;
    }
    if (json.empty()) { ModelLoadResult result; result.error.message = "GLB has no JSON chunk"; return result; }
    return DecodeDocument(json, baseDirectory, binary, options);
}

} // namespace Concord::AssetGltf
