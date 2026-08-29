// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ObjLoaderInternal.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Concord {
namespace {

std::string_view Trim(std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}

bool ReadOptionalWeight(std::istringstream& stream, f32& weight)
{
    std::string token;
    if (!(stream >> token)) return true;
    std::istringstream number(token);
    std::string trailing;
    if (!(number >> weight) || (number >> trailing) || !std::isfinite(weight) || std::abs(weight) < 0.000001f) return false;
    return !(stream >> trailing);
}

bool ReadVec3(std::string_view rest, Vec3& result)
{
    std::istringstream stream{std::string(rest)};
    if (!(stream >> result.x >> result.y >> result.z)) return false;
    f32 w = 1.0f;
    if (!ReadOptionalWeight(stream, w)) return false;
    result = result / w;
    return true;
}

bool ReadVec2(std::string_view rest, Vec2& result)
{
    std::istringstream stream{std::string(rest)};
    if (!(stream >> result.x >> result.y)) return false;
    f32 w = 1.0f;
    if (!ReadOptionalWeight(stream, w)) return false;
    result = result / w;
    return true;
}

} // namespace

namespace AssetObj {

ModelLoadResult DecodeObj(std::string_view text, const ModelLoadOptions& options,
                          const std::filesystem::path& baseDirectory)
{
    if (!std::isfinite(options.scale)) { ModelLoadResult result; result.error.message = "model scale must be finite"; return result; }
    Builder builder;
    builder.options = options;
    builder.asset.name = "obj";
    builder.asset.materials.push_back(ModelMaterial{});
    builder.asset.meshes.push_back(ModelMesh{.name = "default"});
    usize offset = 0;
    while (offset <= text.size()) {
        const usize end = text.find('\n', offset);
        const std::string_view raw = text.substr(offset, end == std::string_view::npos ? text.size() - offset : end - offset);
        builder.line += 1;
        const std::string_view line = Trim(raw.substr(0, raw.find('#')));
        const usize split = line.find_first_of(" \t");
        const std::string_view keyword = split == std::string_view::npos ? line : line.substr(0, split);
        const std::string_view rest = split == std::string_view::npos ? std::string_view{} : Trim(line.substr(split));
        bool okay = true;
        if (keyword == "v") {
            Vec3 value{}; okay = ReadVec3(rest, value) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); value *= options.scale;
            if (okay) builder.positions.push_back(value);
        } else if (keyword == "vn") {
            Vec3 value{}; okay = ReadVec3(rest, value) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); if (okay) builder.normals.push_back(Normalize(value));
        } else if (keyword == "vt") {
            Vec2 value{}; okay = ReadVec2(rest, value) && std::isfinite(value.x) && std::isfinite(value.y); if (okay && options.flipV) value.y = 1.0f - value.y;
            if (okay) builder.texcoords.push_back(value);
        } else if (keyword == "f") {
            okay = ParseFace(builder, rest);
        } else if (keyword == "usemtl") {
            FinishPrimitive(builder);
            const std::string name(rest);
            auto it = std::find_if(builder.asset.materials.begin(), builder.asset.materials.end(),
                                   [&](const ModelMaterial& material) { return material.name == name; });
            if (it == builder.asset.materials.end()) {
                builder.asset.materials.push_back(ModelMaterial{.name = name});
                builder.materialIndex = static_cast<u32>(builder.asset.materials.size() - 1);
            } else builder.materialIndex = static_cast<u32>(std::distance(builder.asset.materials.begin(), it));
        } else if (keyword == "mtllib" && !baseDirectory.empty()) {
            okay = ParseMtlFile(builder, baseDirectory / std::string(rest));
        } else if (keyword == "o" || keyword == "g") {
            FinishPrimitive(builder);
            if (!rest.empty()) {
                if (builder.asset.meshes[builder.meshIndex].primitives.empty()) {
                    builder.asset.meshes[builder.meshIndex].name = std::string(rest);
                } else {
                    builder.asset.meshes.push_back(ModelMesh{.name = std::string(rest)});
                    builder.meshIndex = builder.asset.meshes.size() - 1;
                }
            }
        } else if (!keyword.empty() && keyword != "s" && keyword != "usemap") {
            okay = !options.strict;
        }
        if (!okay && options.strict) { builder.failed = true; builder.error = "invalid OBJ record"; break; }
        if (end == std::string_view::npos) break;
        offset = end + 1;
    }
    FinishPrimitive(builder);
    if (options.generateNormals) GenerateNormals(builder);
    if (options.generateTangents) GenerateTangents(builder);
    ModelLoadResult result;
    result.asset = std::move(builder.asset);
    if (builder.failed) result.error = {builder.error, builder.line, 1};
    else if (!result.asset.IsValid()) result.error.message = "OBJ contains no valid geometry";
    return result;
}

} // namespace AssetObj
} // namespace Concord
