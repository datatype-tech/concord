// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"
#include "engine/asset/ImageAsset.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace Concord::AssetGltf {
namespace {

const AssetJson::Value* ArrayMember(const AssetJson::Value& value,
                                    std::string_view key) noexcept
{
    const AssetJson::Value* member = Member(value, key);
    return member && member->Is(AssetJson::Type::Array) ? member : nullptr;
}

f32 NumberAt(const AssetJson::Value* array, usize index, f32 fallback) noexcept
{
    if (!array || index >= array->array.size()) return fallback;
    f32 value = fallback;
    return FloatValue(&array->array[index], value) ? value : fallback;
}

f32 ScalarNumber(const AssetJson::Value* value, f32 fallback) noexcept
{
    f32 result = fallback;
    return FloatValue(value, result) ? result : fallback;
}

ColorRGBA ColorFromFactor(const AssetJson::Value* factor) noexcept
{
    const auto ToByte = [](f32 value) -> u8 {
        if (!std::isfinite(value)) value = 1.0f;
        return static_cast<u8>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return MakeColor(ToByte(NumberAt(factor, 0, 1.0f)), ToByte(NumberAt(factor, 1, 1.0f)),
                     ToByte(NumberAt(factor, 2, 1.0f)), ToByte(NumberAt(factor, 3, 1.0f)));
}

char Base64Digit(u8 value) noexcept
{
    return value < 26 ? static_cast<char>('A' + value)
        : value < 52 ? static_cast<char>('a' + value - 26)
        : value < 62 ? static_cast<char>('0' + value - 52)
        : value == 62 ? '+' : '/';
}

std::string EncodeDataUri(std::span<const std::byte> bytes, std::string_view mime)
{
    if (bytes.empty() || bytes.size() > kImageMaxBytes || mime.empty() || mime.size() > 128) return {};
    for (const char value : mime) {
        if (value <= 0x20 || value == ',' || value == ';') return {};
    }
    std::string result = "data:";
    result.append(mime);
    result.append(";base64,");
    result.reserve(result.size() + ((bytes.size() + 2) / 3) * 4);
    for (usize index = 0; index < bytes.size(); index += 3) {
        const u32 a = std::to_integer<u8>(bytes[index]);
        const u32 b = index + 1 < bytes.size() ? std::to_integer<u8>(bytes[index + 1]) : 0;
        const u32 c = index + 2 < bytes.size() ? std::to_integer<u8>(bytes[index + 2]) : 0;
        result.push_back(Base64Digit(static_cast<u8>(a >> 2)));
        result.push_back(Base64Digit(static_cast<u8>(((a & 3u) << 4) | (b >> 4))));
        result.push_back(index + 1 < bytes.size() ? Base64Digit(static_cast<u8>(((b & 15u) << 2) | (c >> 6))) : '=');
        result.push_back(index + 2 < bytes.size() ? Base64Digit(static_cast<u8>(c & 63u)) : '=');
    }
    return result;
}

std::string TextureUri(const Context& context, const AssetJson::Value* texture)
{
    usize textureIndex = 0;
    if (!IndexValue(texture, textureIndex)) return {};
    const AssetJson::Value* textures = Member(*context.root, "textures");
    if (!textures || !textures->Is(AssetJson::Type::Array) || textureIndex >= textures->array.size()) return {};
    usize imageIndex = 0;
    if (!IndexValue(Member(textures->array[textureIndex], "source"), imageIndex)) return {};
    const AssetJson::Value* images = Member(*context.root, "images");
    if (!images || !images->Is(AssetJson::Type::Array) || imageIndex >= images->array.size()) return {};
    const AssetJson::Value& image = images->array[imageIndex];
    const AssetJson::Value* uri = Member(image, "uri");
    if (uri && uri->Is(AssetJson::Type::String) && !uri->String().empty()) return std::string(uri->String());
    usize viewIndex = 0;
    if (!IndexValue(Member(image, "bufferView"), viewIndex) || viewIndex >= context.views.size()) return {};
    const AssetJson::Value* mime = Member(image, "mimeType");
    if (!mime || !mime->Is(AssetJson::Type::String)) return {};
    const BufferView& view = context.views[viewIndex];
    const auto& buffer = context.buffers[view.buffer];
    return EncodeDataUri(std::span<const std::byte>(buffer.data() + view.offset, view.length), mime->String());
}

} // namespace

bool ReadMaterials(Context& context)
{
    const AssetJson::Value* materials = Member(*context.root, "materials");
    context.asset.materials.clear();
    if (!materials) {
        context.asset.materials.push_back(ModelMaterial{.baseColor = MakeColor(255, 255, 255), .roughness = 1.0f});
        return true;
    }
    if (!materials->Is(AssetJson::Type::Array)) return context.Fail("glTF materials must be an array");
    for (const AssetJson::Value& record : materials->array) {
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF material");
        ModelMaterial material{};
        material.baseColor = MakeColor(255, 255, 255);
        material.roughness = 1.0f;
        if (const auto* name = Member(record, "name")) material.name = std::string(name->String());
        const AssetJson::Value* pbr = Member(record, "pbrMetallicRoughness");
        if (pbr && !pbr->Is(AssetJson::Type::Object)) return context.Fail("invalid glTF PBR material");
        if (pbr) {
            material.baseColor = ColorFromFactor(Member(*pbr, "baseColorFactor"));
            material.metallic = ScalarNumber(Member(*pbr, "metallicFactor"), 0.0f);
            material.roughness = ScalarNumber(Member(*pbr, "roughnessFactor"), 1.0f);
            material.baseColorTexture = TextureUri(context, Member(*pbr, "baseColorTexture")
                ? Member(*Member(*pbr, "baseColorTexture"), "index") : nullptr);
        }
        const AssetJson::Value* emissive = ArrayMember(record, "emissiveFactor");
        material.emissive = {NumberAt(emissive, 0, 0.0f), NumberAt(emissive, 1, 0.0f), NumberAt(emissive, 2, 0.0f)};
        material.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
        material.roughness = std::clamp(material.roughness, 0.04f, 1.0f);
        context.asset.materials.push_back(std::move(material));
    }
    if (context.asset.materials.empty()) context.asset.materials.push_back(ModelMaterial{.baseColor = MakeColor(255, 255, 255), .roughness = 1.0f});
    return true;
}

bool ReadGeometry(Context& context)
{
    return ReadMaterials(context) && ReadMeshes(context);
}

} // namespace Concord::AssetGltf
