// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ObjLoaderInternal.h"

#include <charconv>

namespace Concord::AssetObj {
namespace {

bool ParseInt(std::string_view text, i32& value) noexcept
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value != 0;
}

i32 Resolve(i32 value, usize count) noexcept
{
    const i64 index = value > 0 ? static_cast<i64>(value - 1)
                                : static_cast<i64>(count) + static_cast<i64>(value);
    return index >= 0 && static_cast<usize>(index) < count ? static_cast<i32>(index) : -1;
}

bool ParseIndex(std::string_view token, Index& result) noexcept
{
    const usize first = token.find('/');
    const usize second = first == std::string_view::npos ? first : token.find('/', first + 1);
    const std::string_view p = token.substr(0, first);
    if (p.empty() || !ParseInt(p, result.position)) return false;
    if (first == std::string_view::npos) return true;
    const std::string_view t = token.substr(first + 1, second == std::string_view::npos ? token.size() - first - 1 : second - first - 1);
    if (!t.empty() && !ParseInt(t, result.texcoord)) return false;
    if (second == std::string_view::npos) return true;
    const std::string_view n = token.substr(second + 1);
    return n.empty() || ParseInt(n, result.normal);
}

u32 AddVertex(Builder& builder, Index key)
{
    const auto found = builder.vertices.find(key);
    if (found != builder.vertices.end()) return found->second;
    const i32 position = Resolve(key.position, builder.positions.size());
    const i32 texcoord = key.texcoord == 0 ? -1 : Resolve(key.texcoord, builder.texcoords.size());
    const i32 normal = key.normal == 0 ? -1 : Resolve(key.normal, builder.normals.size());
    if (position < 0 || (key.texcoord != 0 && texcoord < 0) || (key.normal != 0 && normal < 0)) {
        builder.failed = true;
        return 0;
    }
    ModelVertex vertex{};
    vertex.position = builder.positions[static_cast<usize>(position)];
    if (texcoord >= 0) vertex.texcoord = builder.texcoords[static_cast<usize>(texcoord)];
    if (normal >= 0) vertex.normal = builder.normals[static_cast<usize>(normal)];
    else vertex.normal = {};
    if (builder.primitive.vertices.empty()) builder.primitive.materialIndex = builder.materialIndex;
    const u32 index = static_cast<u32>(builder.primitive.vertices.size());
    builder.primitive.vertices.push_back(vertex);
    builder.vertices.emplace(key, index);
    return index;
}

} // namespace

bool ParseFace(Builder& builder, std::string_view text)
{
    std::vector<u32> polygon;
    usize offset = 0;
    while (offset < text.size()) {
        while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t')) ++offset;
        if (offset >= text.size()) break;
        usize end = offset;
        while (end < text.size() && text[end] != ' ' && text[end] != '\t') ++end;
        Index key{};
        if (!ParseIndex(text.substr(offset, end - offset), key)) return false;
        key.position = key.position;
        key.texcoord = key.texcoord;
        key.normal = key.normal;
        polygon.push_back(AddVertex(builder, key));
        if (builder.failed) return false;
        offset = end;
    }
    if (polygon.size() < 3) return false;
    for (usize i = 1; i + 1 < polygon.size(); ++i) {
        builder.primitive.indices.push_back(polygon[0]);
        builder.primitive.indices.push_back(polygon[i]);
        builder.primitive.indices.push_back(polygon[i + 1]);
    }
    return true;
}

void FinishPrimitive(Builder& builder)
{
    if (builder.primitive.indices.empty()) return;
    if (builder.meshIndex >= builder.asset.meshes.size()) builder.meshIndex = 0;
    builder.asset.meshes[builder.meshIndex].primitives.push_back(std::move(builder.primitive));
    builder.primitive = ModelPrimitive{};
    builder.vertices.clear();
}

} // namespace Concord::AssetObj
